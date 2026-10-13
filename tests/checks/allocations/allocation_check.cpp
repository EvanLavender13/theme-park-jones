// The allocation check (decision 0041): the work the app repeats in steady play, repeated on
// full.park after a warm-up, must free no heap block it allocated while it repeats. Each tick is
// followed by the scene's sync, as the app runs it: once for the frame with the tick, and once for
// a frame without one. Then come the previews of two edits held still with no overlay, each with a
// look, synced as the app's frame syncs its preview, twice. At every 60th tick come the overlays:
// each need's with no edit, and food's with a guest path the world refuses, also synced twice.

#include "watch.h"

#include "app/scene/scene_sync.h"
#include "app/session/park_session.h"
#include "legible/inspect.h"
#include "legible/preview.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace {

// Ticks stepped before watching, so that the kept lists have grown to what the park needs.
constexpr uint64_t WARM_TICKS = 300;
// Ticks watched: long enough for shops to serve, offers to change, and packets to arrive many times
// over. Guests and seat services live longer than this, so it can't see their storage reused.
constexpr uint64_t WATCHED_TICKS = 600;
// The overlays are shaded at every this many ticks, since a shading takes about half a second on
// windows-debug. A block one shading allocates and a later one frees is still found.
constexpr uint64_t OVERLAY_TICKS = 60;

std::string readText(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error(std::string("cannot read ") + path);
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The scene's sync after a frame, as the app runs it.
void syncScene(tpj::SceneSync &scene, const tpj::ParkSession &session) {
  scene.syncWorld(session.world(), session.generation(), session.edits());
}

// An edit held still and the look around it, with the scene that keeps its preview.
struct HeldPreview {
  tpj::SceneSync Scene;
  std::optional<tpj::ParkEdit> Edit;
  tpj::PreviewLook Look;
};

// The held preview's sync after a frame, as the app runs it: the scene's world, the preview,
// following ticks while an overlay shows, the overlay's shape before and after the panels, and the
// ghost and shades.
void syncPreview(HeldPreview &held, const tpj::ParkSession &session) {
  const tpj::World &world = session.world();
  held.Scene.syncWorld(world, session.generation(), session.edits());
  const bool shown = held.Look.Overlay.has_value();
  const bool remade = held.Scene.syncPreview(world, held.Edit, shown);
  bool shapeChanged =
      held.Scene.syncOverlayShape(tpj::previewedWorld(world, held.Scene.preview()), shown);
  shapeChanged =
      held.Scene.syncOverlayShape(tpj::previewedWorld(world, held.Scene.preview()), shown) ||
      shapeChanged;
  static_cast<void>(held.Scene.syncLook(world, remade, held.Look, shapeChanged));
}

// The look around a held edit: its box or point highlighted, a handle of each kind there, the
// first guest segment's sides under the pointer, a guest path that would start and finish there,
// and the guest in the middle of the park's guests inspected.
tpj::PreviewLook heldLook(const tpj::World &world, tpj::EntityKey key, tpj::ParkPoint at) {
  tpj::PreviewLook look;
  look.Highlight = key;
  for (const tpj::HandleKind kind : {tpj::HandleKind::Point, tpj::HandleKind::Radius,
                                     tpj::HandleKind::Edge, tpj::HandleKind::Taper}) {
    look.Handles.push_back({kind, key, at, tpj::NULL_KEY});
  }
  std::vector<tpj::EntityKey> segments;
  tpj::parkSegmentKeys(world, tpj::PathKind::Guest, segments);
  if (!segments.empty()) {
    look.Hovered = tpj::EditHandle{tpj::HandleKind::Side, segments.front(), at, tpj::NULL_KEY};
  }
  look.Start = tpj::PathStart{at, tpj::PathKind::Guest, true};
  look.Finish = at;
  const std::vector<tpj::EntityKey> guests = tpj::parkGuests(world);
  if (!guests.empty()) {
    look.Inspected = tpj::InspectorSubject{guests[guests.size() / 2], tpj::SubjectKind::Guest};
  }
  return look;
}

// The held edits: the first shop moved onto its own place, and the first segment the world lets go
// deleted.
void holdEdits(std::vector<HeldPreview> &held, const tpj::World &world) {
  std::optional<tpj::ParkBox> shop;
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (box.Kind == tpj::BoxKind::Shop) {
      shop = box;
      break;
    }
  }
  if (!shop) {
    throw std::runtime_error("full.park holds no shop");
  }
  held[0].Edit = tpj::MoveBox{shop->Key, shop->At};
  held[0].Look = heldLook(world, shop->Key, tpj::ParkPoint{shop->At.X, shop->At.Z});
  for (const tpj::ParkSegment &segment : tpj::parkSegments(world)) {
    if (!segment.Line.empty() && tpj::isAccepted(world, tpj::DeleteSegment{segment.Key})) {
      held[1].Edit = tpj::DeleteSegment{segment.Key};
      held[1].Look = heldLook(world, segment.Key,
                              tpj::ParkPoint{segment.Line.front().X, segment.Line.front().Z});
      return;
    }
  }
  throw std::runtime_error("full.park holds no segment the world lets go");
}

// The overlays: each need's with no edit, and food's with a guest path outside the park, which
// the world refuses.
void holdOverlays(std::vector<HeldPreview> &overlays, const tpj::World &world) {
  for (size_t i = 0; i < tpj::NEEDS.size(); ++i) {
    overlays[i].Look.Overlay = tpj::NEEDS[i];
  }
  HeldPreview &refused = overlays.back();
  const tpj::AddPath path{tpj::PathKind::Guest,
                          {tpj::NewPoint{{-1000.0, -1000.0}}, tpj::NewPoint{{-990.0, -1000.0}}}};
  if (tpj::isAccepted(world, path)) {
    throw std::runtime_error("the held guest path is accepted");
  }
  refused.Edit = path;
  refused.Look.Overlay = tpj::GuestNeed::Hunger;
}

// The scene and the previews steady play keeps, each synced after every tick as the app syncs it.
struct SteadyPlay {
  tpj::SceneSync Scene;
  std::vector<HeldPreview> Held = std::vector<HeldPreview>(2);
  std::vector<HeldPreview> Overlays = std::vector<HeldPreview>(tpj::NEEDS.size() + 1);
};

// Steps the park through the warm-up, syncing as the watched ticks do, so the kept lists grow to
// what the park needs. Throws when the held shop move shows no connected shop context.
void warmUp(SteadyPlay &play, tpj::ParkSession &session) {
  for (uint64_t tick = 0; tick < WARM_TICKS; ++tick) {
    session.step();
    syncScene(play.Scene, session);
    for (HeldPreview &edit : play.Held) {
      syncPreview(edit, session);
    }
    if (tick == 0) {
      const tpj::Preview &moved = play.Held[0].Scene.preview();
      if (!moved.Candidate || !moved.Shop || !moved.Shop->Connection) {
        throw std::runtime_error("the held shop move has no connected shop context");
      }
    }
    if (tick % OVERLAY_TICKS == 0) {
      for (HeldPreview &overlay : play.Overlays) {
        syncPreview(overlay, session);
      }
    }
  }
}

// Prints the blocks the watched ticks freed, or that they freed none, and gives the exit status.
int report() {
  if (tpj::check::tableOverflowed()) {
    std::cout << "allocation check: the watched ticks noted more blocks than the table holds\n";
    return 1;
  }
  if (tpj::check::freedCount() > 0) {
    std::cout << "allocation check: " << tpj::check::freedCount()
              << " blocks were allocated and freed during the watched ticks\n";
    for (const tpj::check::FreedBlock &block : tpj::check::freedBlocks()) {
      std::cout << "allocation check: " << block.FreedBy << " freed a " << block.Size
                << "-byte block at tick " << block.Freed << " that " << block.AllocatedBy
                << " allocated at tick " << block.Allocated << '\n';
    }
    return 1;
  }
  std::cout
      << "allocation check: " << WATCHED_TICKS
      << " ticks of full.park, the scene's sync, the preview, and the overlays freed no block "
         "they allocated\n";
  return 0;
}

// Steps the park through the app's own session, syncing the scene and the previews after each
// tick, and reports the blocks the watched ticks freed.
int checkTicks(tpj::ParkSession &session) {
  SteadyPlay play;
  holdEdits(play.Held, session.world());
  holdOverlays(play.Overlays, session.world());
  warmUp(play, session);
  tpj::check::startWatching(session.world().Tick);
  for (uint64_t tick = 0; tick < WATCHED_TICKS; ++tick) {
    tpj::check::watchTick(session.world().Tick);
    tpj::check::watchWork("the tick");
    session.step();
    tpj::check::watchWork("the scene's sync");
    syncScene(play.Scene, session);
    syncScene(play.Scene, session);
    tpj::check::watchWork("the preview");
    for (HeldPreview &edit : play.Held) {
      syncPreview(edit, session);
      syncPreview(edit, session);
    }
    if (tick % OVERLAY_TICKS == 0) {
      tpj::check::watchWork("the overlay");
      for (HeldPreview &overlay : play.Overlays) {
        syncPreview(overlay, session);
        syncPreview(overlay, session);
      }
    }
  }
  tpj::check::stopWatching();
  return report();
}

} // namespace

int main() {
  try {
    const std::string text = readText(TPJ_PARKS_DIR "/stress/full.park");
    tpj::World world = tpj::loadWorld(tpj::makeParkSchema(), text);
    tpj::resolveWorld(world);
    tpj::ParkSession session(std::move(world));
    return checkTicks(session);
  } catch (const std::exception &error) {
    std::cout << "allocation check: " << error.what() << '\n';
    return 2;
  }
}
