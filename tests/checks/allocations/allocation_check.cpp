// The allocation check (decision 0041): the work the app repeats in steady play, repeated on
// full.park after a warm-up, must free no heap block it allocated while it repeats. Each tick is
// followed by the scene's sync, as the app runs it: once for the frame with the tick, and once for
// a frame without one. Then come the previews of two edits held still with no overlay, each with a
// look, synced as the app's frame syncs its preview, twice. At every 60th tick come the overlays:
// each need's with no edit, and food's with a guest path the world refuses, also synced twice.
// Last come the tools: each tool's frame work with the pointer held still, and the look's marks
// built again for a look that changes at each tick, as a moving pointer changes it. Then come the
// panels' data, with a guest, a shop, a bench, and a toilet block inspected. After each sync of the
// scene or of a held preview, the check uploads what the app's frame uploads and draws the frame,
// in a hidden window, with SDL's allocations counted too. Before all of that, the check adds guest
// paths with a rounded corner and a taper, which full.park lacks. After the ticks, it undoes and
// redoes a guest path, and watches the app's work after each, with the watch paused while the
// session applies the command. Then it drags a shop along its face and draws a guest path out and
// back, a tick before each frame, through a kept preview and the ghost, and commits edits there
// and back on a copy of the world. There it watches every resolution step, and excuses the blocks
// the steps on its exception list allocate or free. It fails when an excused step no longer
// allocates, so the list only shrinks.

#include "watch.h"

#include "app/input/interaction.h"
#include "app/input/orbit_camera.h"
#include "app/scene/scene_sync.h"
#include "app/scene/scene_uploads.h"
#include "app/session/park_session.h"
#include "app/ui/panel_data.h"
#include "legible/arrivals.h"
#include "legible/inspect.h"
#include "legible/preview.h"
#include "render/day_light.h"
#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
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
// Rounds of undos and redos to warm up, and to watch.
constexpr uint64_t WARM_ROUNDS = 4;
constexpr uint64_t WATCHED_ROUNDS = 20;

std::string readText(const char *path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error(std::string("cannot read ") + path);
  }
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// A hidden window and the renderer that draws into it, as the app makes them for --capture.
class HiddenFrame {
public:
  HiddenFrame() {
    Video = SDL_Init(SDL_INIT_VIDEO);
    if (Video) {
      Window = SDL_CreateWindow("allocation check", 1280, 720, SDL_WINDOW_HIDDEN);
    }
    if (Window != nullptr) {
      ImGui::CreateContext();
      Ui = true;
      Ready = tpj::createRenderer(Gpu, Window, 256.0f);
    }
    if (!Ready) {
      const std::string error = SDL_GetError();
      release();
      throw std::runtime_error("cannot create the hidden window and renderer: " + error);
    }
  }
  ~HiddenFrame() { release(); }
  HiddenFrame(const HiddenFrame &) = delete;
  HiddenFrame &operator=(const HiddenFrame &) = delete;
  HiddenFrame(HiddenFrame &&) = delete;
  HiddenFrame &operator=(HiddenFrame &&) = delete;

  tpj::Renderer Gpu;
  tpj::OrbitCamera Camera;

private:
  void release() {
    tpj::destroyRenderer(Gpu);
    if (Ui) {
      ImGui::DestroyContext();
      Ui = false;
    }
    if (Window != nullptr) {
      SDL_DestroyWindow(Window);
      Window = nullptr;
    }
    if (Video) {
      SDL_Quit();
      Video = false;
    }
  }

  bool Video = false;
  SDL_Window *Window = nullptr;
  bool Ui = false;
  bool Ready = false;
};

// Draws the frame after its uploads, as drawFrame does for the app, then waits for the GPU to
// finish it. The wait keeps one frame in flight. A hidden window's frames don't wait for the
// screen, so without it SDL's cycling would make new internal buffers whenever the GPU fell
// further behind than in the warm-up, and the result would depend on GPU timing.
void drawWaited(HiddenFrame &frame, const tpj::ParkSession &session, bool uploaded,
                const char *work) {
  tpj::check::watchWork("the drawing");
  if (!uploaded ||
      !tpj::drawFrame(frame.Gpu, tpj::orbitCameraView(frame.Camera),
                      tpj::dayLightOf(session.world()), nullptr, nullptr) ||
      !SDL_WaitForGPUIdle(frame.Gpu.Device)) {
    throw std::runtime_error("the hidden frame failed to upload or draw");
  }
  tpj::check::watchWork(work);
}

// The scene's sync after a frame, as the app runs it.
tpj::WorldMeshes syncScene(tpj::SceneSync &scene, const tpj::ParkSession &session) {
  return scene.syncWorld(session.world(), session.generation(), session.edits());
}

// The frame after a sync of the scene, as the app's frame runs it: the uploads, then the drawing.
// Sets the watch's work back to the work given.
void drawScene(HiddenFrame &frame, const tpj::SceneSync &scene, const tpj::ParkSession &session,
               const tpj::WorldMeshes &meshes, const char *work) {
  tpj::check::watchWork("the uploads");
  const bool uploaded = tpj::uploadWorldMeshes(frame.Gpu, scene, meshes, frame.Camera);
  drawWaited(frame, session, uploaded, work);
}

// An edit held still and the look around it, with the scene that keeps its preview.
struct HeldPreview {
  tpj::SceneSync Scene;
  std::optional<tpj::ParkEdit> Edit;
  tpj::PreviewLook Look;
};

// What a held preview's sync gives the frame's uploads.
struct SyncedPreview {
  tpj::PreviewMeshes Meshes;
  bool ShapeChanged = false;
};

// The held preview's sync after a frame, as the app runs it: the scene's world, the preview,
// following ticks while an overlay shows, the overlay's shape before and after the panels, and the
// ghost and shades, with the look's marks read from the park as the tools read it.
SyncedPreview syncPreview(HeldPreview &held, const tpj::ParkSession &session,
                          const tpj::ToolPark &park) {
  const tpj::World &world = session.world();
  held.Scene.syncWorld(world, session.generation(), session.edits());
  const bool shown = held.Look.Overlay.has_value();
  const bool remade = held.Scene.syncPreview(world, held.Edit, shown);
  bool shapeChanged =
      held.Scene.syncOverlayShape(tpj::previewedWorld(world, held.Scene.preview()), shown);
  shapeChanged =
      held.Scene.syncOverlayShape(tpj::previewedWorld(world, held.Scene.preview()), shown) ||
      shapeChanged;
  return {held.Scene.syncLook(world, park, remade, held.Look, shapeChanged), shapeChanged};
}

// The frame after a held preview's sync, as the app's frame runs it: the uploads, then the
// drawing. Sets the watch's work back to the work given.
void drawPreview(HiddenFrame &frame, const HeldPreview &held, const tpj::ParkSession &session,
                 const SyncedPreview &synced, const char *work) {
  tpj::check::watchWork("the uploads");
  const bool uploaded =
      tpj::uploadPreviewMeshes(frame.Gpu, held.Scene, synced.Meshes, synced.ShapeChanged);
  drawWaited(frame, session, uploaded, work);
}

// A held preview's sync and the frame after it.
void syncAndDraw(HiddenFrame &frame, HeldPreview &held, const tpj::ParkSession &session,
                 const tpj::ToolPark &park, const char *work) {
  const SyncedPreview synced = syncPreview(held, session, park);
  drawPreview(frame, held, session, synced, work);
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

// The first corner with a radius above 0, if any.
std::optional<tpj::ParkCorner> roundedCorner(const tpj::World &world) {
  for (const tpj::ParkCorner &corner : tpj::parkCorners(world)) {
    if (corner.Radius > 0.0) {
      return corner;
    }
  }
  return std::nullopt;
}

// The key of the path point at the place.
tpj::EntityKey pointAtPlace(const tpj::World &world, tpj::ParkPoint at) {
  for (const tpj::ParkPathPoint &point : tpj::parkPathPoints(world)) {
    if (point.At == at) {
      return point.Key;
    }
  }
  throw std::runtime_error("the held curve's point is missing");
}

// Adds two guest paths in open ground, since full.park has no corner and no taper: one that turns
// at its middle point, whose corner it rounds, and one that runs straight on, one of whose
// segments it widens so a taper forms where they meet.
void addCurve(tpj::ParkSession &session) {
  const tpj::ParkPoint turn{-105.0, -80.0};
  const tpj::ParkPoint joint{-105.0, -50.0};
  tpj::queueEdit(session.commands(),
                 tpj::AddPath{tpj::PathKind::Guest,
                              {tpj::NewPoint{{-120.0, -80.0}}, tpj::NewPoint{turn},
                               tpj::NewPoint{{-105.0, -65.0}}}});
  tpj::queueEdit(session.commands(),
                 tpj::AddPath{tpj::PathKind::Guest,
                              {tpj::NewPoint{{-120.0, -50.0}}, tpj::NewPoint{joint},
                               tpj::NewPoint{{-90.0, -50.0}}}});
  session.applyWithoutTick();
  const tpj::EntityKey corner = pointAtPlace(session.world(), turn);
  const tpj::EntityKey widened = pointAtPlace(session.world(), joint);
  tpj::queueEdit(session.commands(), tpj::SetCornerRadius{corner, 4.0});
  for (const tpj::ParkSegment &segment : tpj::parkSegments(session.world())) {
    if (segment.From == widened || segment.To == widened) {
      tpj::queueEdit(session.commands(), tpj::SetSegmentWidth{segment.Key, 5.0});
      break;
    }
  }
  session.applyWithoutTick();
  if (!roundedCorner(session.world()) || tpj::parkTapers(session.world()).empty()) {
    throw std::runtime_error("the held curves have no rounded corner and taper");
  }
}

// The first segment of the kind with a ground line: its key, its From point, its middle, a place
// on its side, and a place 2 m beside its paving.
struct SegmentPlaces {
  tpj::EntityKey Key = tpj::NULL_KEY;
  tpj::EntityKey From = tpj::NULL_KEY;
  tpj::ParkPoint Middle;
  tpj::ParkPoint Side;
  tpj::ParkPoint Beside;
};

SegmentPlaces segmentPlaces(const tpj::World &world, tpj::PathKind kind) {
  for (const tpj::ParkSegment &segment : tpj::parkSegments(world)) {
    if (segment.Kind != kind || segment.Line.size() < 2) {
      continue;
    }
    const tpj::CarrierPoint &first = segment.Line.front();
    const tpj::CarrierPoint &last = segment.Line.back();
    const double length = std::hypot(last.X - first.X, last.Z - first.Z);
    // A quarter turn counter-clockwise of the direction from the first point to the last.
    const tpj::ParkPoint normal{-(last.Z - first.Z) / length, (last.X - first.X) / length};
    const tpj::ParkPoint middle{0.5 * (first.X + last.X), 0.5 * (first.Z + last.Z)};
    const double half = 0.5 * segment.Width;
    const auto out = [&](double by) {
      return tpj::ParkPoint{middle.X + normal.X * by, middle.Z + normal.Z * by};
    };
    return {segment.Key, segment.From, middle, out(half), out(half + 2.0)};
  }
  throw std::runtime_error("full.park holds no segment of a kind with a ground line");
}

// The position of the path point the key holds.
tpj::ParkPoint pointPosition(const tpj::World &world, tpj::EntityKey key) {
  for (const tpj::ParkPathPoint &point : tpj::parkPathPoints(world)) {
    if (point.Key == key) {
      return point.At;
    }
  }
  throw std::runtime_error("a held segment's point is missing");
}

// A tool held still: the interaction, the frame's look, and the pointer's sample.
struct HeldTool {
  tpj::Interaction Tools;
  tpj::PreviewLook Look;
  tpj::PointerSample Pointer;
};

tpj::PointerSample sampleAt(tpj::ParkPoint at) { return {at, 0.05, false}; }

// A frame's tool work, as the app runs it.
void toolFrame(HeldTool &held, const tpj::ParkSession &session) {
  const tpj::World &world = session.world();
  held.Tools.follow(world, session.generation(), session.edits());
  held.Tools.movePointer(world, held.Pointer);
  static_cast<void>(held.Tools.tentativeEdit(world));
  tpj::fillLook(held.Tools, world, std::nullopt, held.Look);
  static_cast<void>(held.Tools.snapGuides());
  static_cast<void>(held.Tools.measuredLengths(world));
}

// A new held tool of the kind, held at the place.
HeldTool &holdTool(std::vector<HeldTool> &tools, const tpj::ParkSession &session,
                   tpj::ToolKind kind, tpj::ParkPoint at) {
  tools.push_back(HeldTool{
      tpj::Interaction(session.world(), session.generation(), session.edits()), {}, sampleAt(at)});
  tools.back().Tools.selectTool(kind);
  return tools.back();
}

// Moves the held tool's pointer to the place and presses there.
void pressAt(HeldTool &held, tpj::ParkSession &session, tpj::ParkPoint at) {
  held.Tools.movePointer(session.world(), sampleAt(at));
  held.Tools.useButtons(session.world(), session.commands(), tpj::PointerButtons{true, false});
}

// The places the held tools rest on: the first guest and backstage segments' places, the guest
// segment's point, the first shop, the first edge's handle, the rounded corner, the taper, and two
// places in open ground.
struct ToolPlaces {
  SegmentPlaces Guest;
  SegmentPlaces Backstage;
  tpj::ParkPoint Point;
  tpj::ParkPoint Shop;
  tpj::ParkPoint Edge;
  tpj::ParkCorner Corner;
  tpj::ParkTaper Taper;
  tpj::ParkPoint Start{-120.0, -120.0};
  tpj::ParkPoint Along{-110.0, -120.0};
};

ToolPlaces toolPlaces(const tpj::World &world) {
  ToolPlaces places;
  places.Guest = segmentPlaces(world, tpj::PathKind::Guest);
  places.Backstage = segmentPlaces(world, tpj::PathKind::Backstage);
  places.Point = pointPosition(world, places.Guest.From);
  const std::vector<tpj::ParkBox> boxes = tpj::parkBoxes(world);
  const auto shop = std::ranges::find(boxes, tpj::BoxKind::Shop, &tpj::ParkBox::Kind);
  const std::vector<tpj::ParkEdge> edges = tpj::parkEdges(world);
  const std::optional<tpj::ParkCorner> corner = roundedCorner(world);
  const std::vector<tpj::ParkTaper> tapers = tpj::parkTapers(world);
  if (shop == boxes.end() || edges.empty() || !corner || tapers.empty()) {
    throw std::runtime_error("the park holds no shop, edge, rounded corner, or taper");
  }
  places.Shop = tpj::ParkPoint{shop->At.X, shop->At.Z};
  places.Edge = edges.front().Middle;
  places.Corner = *corner;
  places.Taper = tapers.front();
  return places;
}

// The held path tools: on a point and on the arc with nothing drawn, with one stop drawn, with two
// stops drawn and the pointer on the last, and a backstage path on its segment.
void holdPathTools(std::vector<HeldTool> &tools, tpj::ParkSession &session,
                   const ToolPlaces &places) {
  using tpj::ToolKind;
  holdTool(tools, session, ToolKind::GuestPath, places.Point);
  holdTool(tools, session, ToolKind::GuestPath, tpj::bendPlace(places.Corner, 0.5));
  HeldTool &oneStop = holdTool(tools, session, ToolKind::GuestPath, places.Along);
  pressAt(oneStop, session, places.Start);
  if (oneStop.Tools.tool().Drawn.size() != 1) {
    throw std::runtime_error("the held path has no stop drawn");
  }
  HeldTool &twoStops = holdTool(tools, session, ToolKind::GuestPath, places.Along);
  pressAt(twoStops, session, places.Start);
  pressAt(twoStops, session, places.Along);
  if (twoStops.Tools.tool().Drawn.size() != 2) {
    throw std::runtime_error("the held path has no two stops drawn");
  }
  holdTool(tools, session, ToolKind::BackstagePath, places.Backstage.Middle);
}

// The held Edit path tools: on a point, an edge handle, a side, a segment's middle, the radius
// handle, and the taper handle, and dragging a point.
void holdEditTools(std::vector<HeldTool> &tools, tpj::ParkSession &session,
                   const ToolPlaces &places) {
  using tpj::ToolKind;
  holdTool(tools, session, ToolKind::EditPath, places.Point);
  holdTool(tools, session, ToolKind::EditPath, places.Edge);
  holdTool(tools, session, ToolKind::EditPath, places.Guest.Side);
  holdTool(tools, session, ToolKind::EditPath, places.Guest.Middle);
  holdTool(tools, session, ToolKind::EditPath, tpj::radiusHandleOf(places.Corner));
  holdTool(tools, session, ToolKind::EditPath, tpj::taperHandleOf(places.Taper));
  HeldTool &movedPoint = holdTool(tools, session, ToolKind::EditPath,
                                  tpj::ParkPoint{places.Point.X + 1.0, places.Point.Z});
  pressAt(movedPoint, session, places.Point);
  if (movedPoint.Tools.tool().Held == tpj::NULL_KEY) {
    throw std::runtime_error("the held point drag holds no point");
  }
}

// The held tools: each tool still on paths, paving, boxes, and handles, and holding a drawn path,
// a dragged shop, and a dragged point.
void holdTools(std::vector<HeldTool> &tools, tpj::ParkSession &session) {
  using tpj::ToolKind;
  const ToolPlaces places = toolPlaces(session.world());
  holdTool(tools, session, ToolKind::None, places.Guest.Middle);
  holdPathTools(tools, session, places);
  holdTool(tools, session, ToolKind::PlaceShop, places.Guest.Beside);
  holdTool(tools, session, ToolKind::PlaceBench, places.Guest.Beside);
  holdTool(tools, session, ToolKind::PlaceToilet, places.Guest.Beside);
  holdTool(tools, session, ToolKind::PlaceDepot, places.Backstage.Beside);
  holdTool(tools, session, ToolKind::MoveBox, places.Shop);
  HeldTool &movedBox = holdTool(tools, session, ToolKind::MoveBox,
                                tpj::ParkPoint{places.Shop.X + 1.0, places.Shop.Z});
  pressAt(movedBox, session, places.Shop);
  if (movedBox.Tools.tool().Held == tpj::NULL_KEY) {
    throw std::runtime_error("the held box drag holds no box");
  }
  holdEditTools(tools, session, places);
  holdTool(tools, session, ToolKind::RemovePoint, places.Point);
  holdTool(tools, session, ToolKind::Delete, places.Shop);
  holdTool(tools, session, ToolKind::Delete, places.Guest.Middle);
  holdTool(tools, session, ToolKind::Furnish, places.Guest.Middle);
}

// The two looks a moving pointer alternates between: the shop highlighted and inspected, with
// point handles at it and at the guest segment's point and the guest segment's sides hovered; and
// the guest segment highlighted, with one point handle and the backstage segment's sides hovered.
std::array<tpj::PreviewLook, 2> movingLooks(const tpj::World &world) {
  const SegmentPlaces guest = segmentPlaces(world, tpj::PathKind::Guest);
  const SegmentPlaces backstage = segmentPlaces(world, tpj::PathKind::Backstage);
  const tpj::ParkPoint point = pointPosition(world, guest.From);
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (box.Kind != tpj::BoxKind::Shop) {
      continue;
    }
    const tpj::ParkPoint at{box.At.X, box.At.Z};
    std::array<tpj::PreviewLook, 2> looks;
    looks[0].Highlight = box.Key;
    looks[0].Handles = {{tpj::HandleKind::Point, box.Key, at, tpj::NULL_KEY},
                        {tpj::HandleKind::Point, guest.From, point, tpj::NULL_KEY}};
    looks[0].Hovered = tpj::EditHandle{tpj::HandleKind::Side, guest.Key, {}, tpj::NULL_KEY};
    looks[0].Inspected = tpj::InspectorSubject{box.Key, tpj::SubjectKind::Shop};
    looks[1].Highlight = guest.Key;
    looks[1].Handles = {{tpj::HandleKind::Point, guest.From, point, tpj::NULL_KEY}};
    looks[1].Hovered = tpj::EditHandle{tpj::HandleKind::Side, backstage.Key, {}, tpj::NULL_KEY};
    return looks;
  }
  throw std::runtime_error("full.park holds no shop");
}

// A panel's data held still: the subject it inspects and the hours its arrival day asks for.
struct HeldPanel {
  tpj::PanelData Data;
  tpj::InspectorSubject Subject;
  tpj::OpeningHours Hours;
};

// The first box of the kind.
tpj::EntityKey firstBox(const tpj::World &world, tpj::BoxKind kind, const char *name) {
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (box.Kind == kind) {
      return box.Key;
    }
  }
  throw std::runtime_error(std::string("full.park holds no ") + name);
}

// The held panels: the guest in the middle of the park's guests, the first shop, the first bench,
// and the first toilet block. The guest's and the bench's arrival days ask for the park's hours,
// and the shop's and the toilet block's for hours that open an hour later.
void holdPanels(std::vector<HeldPanel> &panels, const tpj::World &world) {
  const std::vector<tpj::EntityKey> guests = tpj::parkGuests(world);
  if (guests.empty()) {
    throw std::runtime_error("full.park holds no guest");
  }
  const tpj::OpeningHours hours = tpj::parkHours(world);
  const tpj::OpeningHours later{hours.Opening + 3600, hours.Closing};
  panels[0].Subject = {guests[guests.size() / 2], tpj::SubjectKind::Guest};
  panels[0].Hours = hours;
  panels[1].Subject = {firstBox(world, tpj::BoxKind::Shop, "shop"), tpj::SubjectKind::Shop};
  panels[1].Hours = later;
  panels[2].Subject = {firstBox(world, tpj::BoxKind::Bench, "bench"), tpj::SubjectKind::Bench};
  panels[2].Hours = hours;
  panels[3].Subject = {firstBox(world, tpj::BoxKind::ToiletBlock, "toilet block"),
                       tpj::SubjectKind::ToiletBlock};
  panels[3].Hours = later;
}

// Where the panels look: the food overlay's shape, the place the need tooltips are read at, and
// the camera's view the Graph view is built for.
struct PanelPlaces {
  const tpj::OverlayShape *Shape = nullptr;
  tpj::ParkPoint Tooltip;
  tpj::CameraView View;
};

// A panel's data as the app works it out in a frame.
void panelFrame(HeldPanel &held, const tpj::ParkSession &session, const PanelPlaces &places) {
  const tpj::World &world = session.world();
  static_cast<void>(held.Data.guestCount(world));
  static_cast<void>(tpj::departureTally(world));
  static_cast<void>(held.Data.arrivalDay(world, held.Hours));
  static_cast<void>(held.Data.inspection(world, held.Subject));
  static_cast<void>(tpj::PanelData::shopStock(world, held.Subject.Key));
  for (const tpj::GuestNeed need : tpj::NEEDS) {
    static_cast<void>(held.Data.tooltipAvailability(world, need, *places.Shape, places.Tooltip));
  }
  static_cast<void>(held.Data.graph(world, places.View, 1280.0f, 720.0f));
}

// The scene and the previews steady play keeps, each synced after every tick as the app syncs it,
// the held tools, and the held panels.
struct SteadyPlay {
  HiddenFrame Frame;
  tpj::SceneSync Scene;
  // The park as the tools read it, which the looks' marks read.
  tpj::ToolPark Park;
  std::vector<HeldPreview> Held = std::vector<HeldPreview>(2);
  std::vector<HeldPreview> Overlays = std::vector<HeldPreview>(tpj::NEEDS.size() + 1);
  std::vector<HeldTool> Tools;
  HeldPreview Moving;
  std::array<tpj::PreviewLook, 2> MovingLooks;
  std::vector<HeldPanel> Panels = std::vector<HeldPanel>(4);
  PanelPlaces Places;
};

// Throws when the held panels would check nothing: the guest shows no choices, the tooltip's
// point has no overlay place, or the Graph view draws no line.
void requireShownPanels(SteadyPlay &play, const tpj::ParkSession &session) {
  const tpj::World &world = session.world();
  HeldPanel &guest = play.Panels[0];
  if (guest.Data.inspection(world, guest.Subject).Choices.empty()) {
    throw std::runtime_error("the held guest shows no choices");
  }
  if (guest.Data.tooltipAvailability(world, tpj::GuestNeed::Hunger, *play.Places.Shape,
                                     play.Places.Tooltip) == nullptr) {
    throw std::runtime_error("the held tooltip's point has no overlay place");
  }
  if (guest.Data.graph(world, play.Places.View, 1280.0f, 720.0f).Lines.empty()) {
    throw std::runtime_error("the held Graph view draws no line");
  }
}

// Each held panel's data after a tick.
void syncPanels(SteadyPlay &play, const tpj::ParkSession &session) {
  for (HeldPanel &held : play.Panels) {
    panelFrame(held, session, play.Places);
  }
}

// The tools' work after a tick: each held tool's frame, and the moving look synced with the look
// for the tick.
void syncTools(SteadyPlay &play, const tpj::ParkSession &session) {
  for (HeldTool &held : play.Tools) {
    toolFrame(held, session);
  }
  play.Moving.Look = play.MovingLooks.at(session.world().Tick % 2);
  syncAndDraw(play.Frame, play.Moving, session, play.Park, "the tools");
}

// One tick of steady play and the frames after it, as the warm-up and the watched ticks both run
// it: the tick, the scene's sync, the held previews, the overlays at every OVERLAY_TICKS ticks, the
// tools, and the panels, with each sync's frame uploaded and drawn.
void steadyTick(SteadyPlay &play, tpj::ParkSession &session, uint64_t tick) {
  tpj::check::watchWork("the tick");
  session.step();
  tpj::check::watchWork("the scene's sync");
  for (int frame = 0; frame < 2; ++frame) {
    const tpj::WorldMeshes meshes = syncScene(play.Scene, session);
    drawScene(play.Frame, play.Scene, session, meshes, "the scene's sync");
  }
  tpj::check::watchWork("the preview");
  for (HeldPreview &edit : play.Held) {
    syncAndDraw(play.Frame, edit, session, play.Park, "the preview");
    syncAndDraw(play.Frame, edit, session, play.Park, "the preview");
  }
  if (tick % OVERLAY_TICKS == 0) {
    tpj::check::watchWork("the overlay");
    for (HeldPreview &overlay : play.Overlays) {
      syncAndDraw(play.Frame, overlay, session, play.Park, "the overlay");
      syncAndDraw(play.Frame, overlay, session, play.Park, "the overlay");
    }
  }
  tpj::check::watchWork("the tools");
  syncTools(play, session);
  tpj::check::watchWork("the panels");
  syncPanels(play, session);
}

// Steps the park through the warm-up as the watched ticks step it, so the kept lists and the
// renderer's buffers and pools grow to what the park needs. Throws when the held shop move shows no
// connected shop context, or the held panels would check nothing.
void warmUp(SteadyPlay &play, tpj::ParkSession &session) {
  for (uint64_t tick = 0; tick < WARM_TICKS; ++tick) {
    steadyTick(play, session, tick);
    if (tick == 0) {
      const tpj::Preview &moved = play.Held[0].Scene.preview();
      if (!moved.Candidate || !moved.Shop || !moved.Shop->Connection) {
        throw std::runtime_error("the held shop move has no connected shop context");
      }
      requireShownPanels(play, session);
    }
  }
}

// The work after an undo or a redo: an interaction that picks and presses each tool, a scene that
// shows the Food overlay and inspects a shop, a second history that follows the session's world,
// the queue the releases put their edits on and the second history's commands go on, and the places
// they press and pick at.
struct EditRounds {
  explicit EditRounds(const tpj::ParkSession &session)
      : Tools(session.world(), session.generation(), session.edits()) {}

  tpj::Interaction Tools;
  HeldPreview Preview;
  tpj::EditHistory History;
  tpj::CommandQueue Queued;
  ToolPlaces Places;
  tpj::ParkPoint Shop;
  tpj::ParkPoint Bench;
};

// The position of the box the key holds.
tpj::ParkPoint boxPosition(const tpj::World &world, tpj::EntityKey key) {
  const std::optional<tpj::ParkBox> box = tpj::parkBox(world, key);
  if (!box) {
    throw std::runtime_error("a held box is missing");
  }
  return {box->At.X, box->At.Z};
}

// Finds the places the rounds press and pick at, and the look, and adds a guest path in open
// ground, which the rounds undo and redo, noting it on the second history too. Throws when the
// world refuses the path.
void beginRounds(EditRounds &rounds, tpj::ParkSession &session) {
  const tpj::World &world = session.world();
  rounds.Places = toolPlaces(world);
  const tpj::EntityKey shop = firstBox(world, tpj::BoxKind::Shop, "shop");
  rounds.Shop = boxPosition(world, shop);
  rounds.Bench = boxPosition(world, firstBox(world, tpj::BoxKind::Bench, "bench"));
  tpj::PreviewLook &look = rounds.Preview.Look;
  look.Inspected = tpj::InspectorSubject{shop, tpj::SubjectKind::Shop};
  look.Highlight = rounds.Places.Guest.From;
  look.Hovered = tpj::EditHandle{tpj::HandleKind::Side, rounds.Places.Guest.Key, {}, tpj::NULL_KEY};
  tpj::clearHistory(rounds.History, session.world());
  tpj::queueEdit(session.commands(),
                 tpj::AddPath{tpj::PathKind::Guest,
                              {tpj::NewPoint{{-120.0, -100.0}}, tpj::NewPoint{{-110.0, -100.0}}}});
  session.applyWithoutTick();
  tpj::noteEdits(rounds.History, session.world());
  if (!tpj::canUndo(rounds.History)) {
    throw std::runtime_error("the open-ground path was refused");
  }
}

// Moves the round's pointer to the place and takes the frame's tentative edit, as the app's frame
// does.
void pointAt(EditRounds &rounds, const tpj::World &world, tpj::ParkPoint at) {
  rounds.Tools.movePointer(world, sampleAt(at));
  static_cast<void>(rounds.Tools.tentativeEdit(world));
}

// Selects the tool, points at the place, presses, and releases.
void pressAndRelease(EditRounds &rounds, const tpj::World &world, tpj::ToolKind kind,
                     tpj::ParkPoint at) {
  rounds.Tools.selectTool(kind);
  pointAt(rounds, world, at);
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{true, false});
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{false, true});
  rounds.Queued.clear();
}

// Draws a guest path of two stops and finishes it. The second stop's press asks the world whether
// it accepts the path.
void drawPath(EditRounds &rounds, const tpj::World &world) {
  const ToolPlaces &places = rounds.Places;
  rounds.Tools.selectTool(tpj::ToolKind::GuestPath);
  pointAt(rounds, world, places.Start);
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{true, false});
  pointAt(rounds, world, places.Along);
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{true, false});
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{true, false});
  if (!rounds.Tools.tool().Holding) {
    throw std::runtime_error("the drawn path did not finish");
  }
  rounds.Tools.useButtons(world, rounds.Queued, tpj::PointerButtons{false, true});
  rounds.Queued.clear();
}

// Each tool's press and release: on the places the held tools rest on, and drawing a path.
void pressEachTool(EditRounds &rounds, const tpj::World &world) {
  using tpj::ToolKind;
  const ToolPlaces &places = rounds.Places;
  pressAndRelease(rounds, world, ToolKind::PlaceShop, places.Guest.Beside);
  pressAndRelease(rounds, world, ToolKind::MoveBox, places.Shop);
  for (const tpj::ParkPoint at :
       {places.Point, tpj::radiusHandleOf(places.Corner), tpj::taperHandleOf(places.Taper),
        places.Edge, places.Guest.Side, places.Guest.Middle}) {
    pressAndRelease(rounds, world, ToolKind::EditPath, at);
  }
  pressAndRelease(rounds, world, ToolKind::RemovePoint, places.Point);
  pressAndRelease(rounds, world, ToolKind::Delete, places.Shop);
  pressAndRelease(rounds, world, ToolKind::Furnish, places.Guest.Middle);
  drawPath(rounds, world);
  rounds.Tools.selectTool(ToolKind::None);
}

// The pick at the screen's middle, from a view 60 m above and 30 m south of the place, looking at
// it.
void pickAt(EditRounds &rounds, const tpj::World &world, tpj::ParkPoint at) {
  tpj::CameraView view;
  view.Eye = {static_cast<float>(at.X), 60.0f, static_cast<float>(at.Z) + 30.0f};
  view.Target = {static_cast<float>(at.X), 0.0f, static_cast<float>(at.Z)};
  tpj::CursorNdc cursor;
  cursor.Aspect = 16.0f / 9.0f;
  rounds.Tools.pick(world, rounds.Preview.Scene.seated(), cursor, view);
}

// One undo or redo and the app's work after it. The watch pauses while the session takes and
// applies the command, and watches the second history take and note the same one and queue its
// command, as the session does.
void editRound(EditRounds &rounds, HiddenFrame &frame, tpj::ParkSession &session, bool undo) {
  tpj::check::watchWork("the history");
  const tpj::RestoreIntent *restore =
      undo ? tpj::takeUndo(rounds.History) : tpj::takeRedo(rounds.History);
  if (restore == nullptr) {
    throw std::runtime_error("the history has nothing to undo or redo");
  }
  rounds.Queued.push(*restore);
  rounds.Queued.clear();
  tpj::check::pauseWatching();
  if (undo) {
    session.undo();
  } else {
    session.redo();
  }
  session.applyWithoutTick();
  tpj::check::resumeWatching();
  const tpj::World &world = session.world();
  tpj::noteEdits(rounds.History, world);
  if ((undo ? rounds.History.Redos : rounds.History.Undos).empty()) {
    throw std::runtime_error("the undo or redo did not apply");
  }
  tpj::check::watchWork("the tool park");
  rounds.Tools.follow(world, session.generation(), session.edits());
  tpj::check::watchWork("the scene's sync");
  HeldPreview &preview = rounds.Preview;
  const tpj::WorldMeshes meshes = syncScene(preview.Scene, session);
  drawScene(frame, preview.Scene, session, meshes, "the scene's sync");
  for (const bool shown : {true, false, true}) {
    preview.Look.Overlay =
        shown ? std::optional<tpj::GuestNeed>(tpj::GuestNeed::Hunger) : std::nullopt;
    syncAndDraw(frame, preview, session, rounds.Tools.park(), "the scene's sync");
  }
  tpj::check::watchWork("the picking");
  pickAt(rounds, world, undo ? rounds.Shop : rounds.Bench);
  tpj::check::watchWork("the presses");
  pressEachTool(rounds, world);
}

// Prints the blocks freed during the watched work, or that its table overflowed, and gives 1, or
// prints nothing and gives 0 when it freed none. count is how many it freed, and blocks the first
// of them.
int reportBlocks(const char *watched, size_t count,
                 std::span<const tpj::check::FreedBlock> blocks) {
  if (tpj::check::tableOverflowed()) {
    std::cout << "allocation check: " << watched << " noted more blocks than the table holds\n";
    return 1;
  }
  if (count > 0) {
    std::cout << "allocation check: " << count << " blocks were allocated and freed during "
              << watched << '\n';
    for (const tpj::check::FreedBlock &block : blocks) {
      std::cout << "allocation check: " << block.FreedBy << " freed a " << block.Size
                << "-byte block at tick " << block.Freed << " that " << block.AllocatedBy
                << " allocated at tick " << block.Allocated << '\n';
    }
    return 1;
  }
  return 0;
}

// Prints every block freed during the watched work, as reportBlocks prints them.
int reportFreed(const char *watched) {
  return reportBlocks(watched, tpj::check::freedCount(), tpj::check::freedBlocks());
}

// Steps the park through the app's own session, syncing the scene and the previews after each
// tick, and reports the blocks the watched ticks freed.
int checkTicks(SteadyPlay &play, tpj::ParkSession &session) {
  addCurve(session);
  tpj::keepToolPark(session.world(), play.Park);
  holdEdits(play.Held, session.world());
  holdOverlays(play.Overlays, session.world());
  holdTools(play.Tools, session);
  play.MovingLooks = movingLooks(session.world());
  holdPanels(play.Panels, session.world());
  play.Places = {&play.Overlays[0].Scene.overlayShape(),
                 segmentPlaces(session.world(), tpj::PathKind::Guest).Middle,
                 tpj::orbitCameraView(tpj::OrbitCamera{})};
  warmUp(play, session);
  tpj::check::startWatching(session.world().Tick);
  for (uint64_t tick = 0; tick < WATCHED_TICKS; ++tick) {
    tpj::check::watchTick(session.world().Tick);
    steadyTick(play, session, tick);
  }
  tpj::check::stopWatching();
  return reportFreed("the watched ticks");
}

// Undoes and redoes a guest path on the session, running the app's work after each in the play's
// hidden frame, and reports the blocks the watched rounds freed.
int checkEdits(SteadyPlay &play, tpj::ParkSession &session) {
  EditRounds rounds(session);
  beginRounds(rounds, session);
  for (uint64_t round = 0; round < WARM_ROUNDS; ++round) {
    editRound(rounds, play.Frame, session, round % 2 == 0);
  }
  if (!rounds.Tools.subject()) {
    throw std::runtime_error("the first pick picked nothing");
  }
  tpj::check::startWatching(session.world().Tick);
  for (uint64_t round = 0; round < WATCHED_ROUNDS; ++round) {
    editRound(rounds, play.Frame, session, round % 2 == 0);
  }
  tpj::check::stopWatching();
  return reportFreed("the watched rounds");
}

// A resolution step that still allocates, and the feature that removes it.
struct ExcusedStep {
  std::string_view Step;
  std::string_view RemovedBy;
};

// The resolution steps that still allocate, each with the feature that removes it. The drags and
// commits excuse the blocks these steps allocate or free. The check fails when one no longer
// allocates, so the list only shrinks.
constexpr std::array EXCUSED_STEPS{
    ExcusedStep{"path-networks", "a later feature: the path networks' carriers"},
    ExcusedStep{"guest-ground", "a later feature: guest ground"},
    ExcusedStep{"best-seats", "a later feature: best seats"},
    ExcusedStep{"food-offer", "a later feature: the offers"},
    ExcusedStep{"drink-offer", "a later feature: the offers"},
    ExcusedStep{"rest-offer", "a later feature: the offers"},
    ExcusedStep{"toilet-offer", "a later feature: the offers"},
    ExcusedStep{"seat-offers", "a later feature: the offers"},
    ExcusedStep{"finishers", "a later feature: carrying guests and footfall"},
};

// Which excused steps allocated or freed a block the drags or the commits freed, across both.
std::array<bool, EXCUSED_STEPS.size()> SeenSteps{};
// How many blocks the watched drags or commits freed that no excused step allocated or freed, and
// the first REPORTED_BLOCKS of them.
size_t UnexcusedCount = 0;
std::array<tpj::check::FreedBlock, tpj::check::REPORTED_BLOCKS> Unexcused{};

// The excused step the work names, or EXCUSED_STEPS.size() when it names none.
size_t excusedStep(const char *work) {
  const std::string_view name(work);
  return static_cast<size_t>(std::ranges::find(EXCUSED_STEPS, name, &ExcusedStep::Step) -
                             EXCUSED_STEPS.begin());
}

// Excuses a block an excused step allocated or freed, marking that step seen, and keeps any other
// block. Called from operator delete, so it allocates nothing.
void excuseOrKeep(const tpj::check::FreedBlock &block) {
  const size_t allocatedBy = excusedStep(block.AllocatedBy);
  const size_t freedBy = excusedStep(block.FreedBy);
  if (allocatedBy < EXCUSED_STEPS.size()) {
    SeenSteps.at(allocatedBy) = true;
  }
  if (freedBy < EXCUSED_STEPS.size()) {
    SeenSteps.at(freedBy) = true;
  }
  if (allocatedBy < EXCUSED_STEPS.size() || freedBy < EXCUSED_STEPS.size()) {
    return;
  }
  if (UnexcusedCount < Unexcused.size()) {
    Unexcused.at(UnexcusedCount) = block;
  }
  ++UnexcusedCount;
}

// Starts watching resolutions at the tick: the blocks kept so far are forgotten, and the steps
// seen so far stay seen.
void startWatchingSteps(uint64_t tick) {
  tpj::check::startWatching(tick);
  UnexcusedCount = 0;
  tpj::check::onFreed(excuseOrKeep);
}

void stopWatchingSteps() {
  tpj::check::stopWatching();
  tpj::check::onFreed(nullptr);
}

// Prints the blocks the watched resolutions freed that no excused step allocated or freed, as
// reportBlocks prints them.
int reportUnexcused(const char *watched) {
  return reportBlocks(watched, UnexcusedCount,
                      std::span<const tpj::check::FreedBlock>(Unexcused).first(
                          std::min(UnexcusedCount, Unexcused.size())));
}

// Prints each excused step that allocated or freed no block the drags and the commits freed, and
// gives 1, or prints nothing and gives 0 when every one did.
int reportStaleSteps() {
  int failed = 0;
  for (size_t i = 0; i < EXCUSED_STEPS.size(); ++i) {
    if (!SeenSteps.at(i)) {
      std::cout << "allocation check: " << EXCUSED_STEPS.at(i).Step
                << " no longer allocates; remove it from EXCUSED_STEPS\n";
      failed = 1;
    }
  }
  return failed;
}

// Counts the watch's work against each resolution step while it runs, and against the work around
// it again once it ends. It acts only while Watching is set.
class WatchedSteps : public tpj::ResolveTrace {
public:
  bool Watching = false;

  void started(std::string_view step) override {
    if (Watching) {
      Outer = tpj::check::currentWork();
      // A step's name is a resolver's name in the schema or a string literal, both ending in a
      // null and outliving the watch.
      tpj::check::watchWork(step.data());
    }
  }
  void ended(std::string_view /*step*/) override {
    if (Watching) {
      tpj::check::watchWork(Outer);
    }
  }

private:
  const char *Outer = "";
};

// Frames of each drag to warm up, and to watch.
constexpr size_t WARM_FRAMES = 8;
constexpr size_t WATCHED_FRAMES = 16;
// How far a drag moves its edit each frame, and how far out it goes before coming back, as
// tpj_bench's drags move theirs.
constexpr double DRAG_STEP = 0.5;
constexpr double DRAG_SPAN = 2.0;
// How far beyond its segment's side the dragged path's end starts.
constexpr double DRAG_START_REACH = 4.0;

// How far out a drag is at the frame: DRAG_STEP further each frame, out to DRAG_SPAN and back.
double dragOffset(size_t frame) {
  const auto steps = static_cast<size_t>(DRAG_SPAN / DRAG_STEP);
  const size_t phase = (frame + 1) % (2 * steps);
  const size_t out = phase <= steps ? phase : 2 * steps - phase;
  return DRAG_STEP * static_cast<double>(out);
}

// A drag on its own copy of the world: the buffers its ticks use, its kept preview and ghost, the
// edit it holds, refilled in place at each frame, and the trace its preview's resolutions tell.
struct Drag {
  tpj::World World;
  tpj::TickBuffers Stepping;
  tpj::KeptPreview Kept;
  tpj::GhostLists Ghost;
  std::optional<tpj::ParkEdit> Held;
  WatchedSteps Steps;

  explicit Drag(const tpj::World &world) : World(tpj::copyWorld(world)) {
    Kept.Buffers.get<tpj::ResolveTraceSlot>().Trace = &Steps;
  }
};

// The lowest-keyed shop.
tpj::ParkBox lowestShop(const tpj::World &world) {
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (box.Kind == tpj::BoxKind::Shop) {
      return box;
    }
  }
  throw std::runtime_error("full.park holds no shop");
}

// The shop's pose moved the distance along its face.
tpj::Pose alongFace(const tpj::ParkBox &shop, double distance) {
  const double facing = std::hypot(shop.At.FacingX, shop.At.FacingZ);
  tpj::Pose pose = shop.At;
  pose.X += -shop.At.FacingZ / facing * distance;
  pose.Z += shop.At.FacingX / facing * distance;
  return pose;
}

// A guest path from the middle of the first guest segment the world accepts one from, to the
// point the distance beyond its side, trying the side a quarter turn counter-clockwise of its
// direction first.
tpj::AddPath spurPath(const tpj::World &world, double distance) {
  for (const tpj::ParkSegment &segment : tpj::parkSegments(world)) {
    if (segment.Kind != tpj::PathKind::Guest || segment.Line.size() < 2) {
      continue;
    }
    const tpj::CarrierPoint &first = segment.Line.front();
    const tpj::CarrierPoint &last = segment.Line.back();
    const double length = std::hypot(last.X - first.X, last.Z - first.Z);
    const tpj::ParkPoint middle{0.5 * (first.X + last.X), 0.5 * (first.Z + last.Z)};
    for (const double side : {1.0, -1.0}) {
      const double reach = 0.5 * segment.Width + distance;
      const tpj::ParkPoint normal{-(last.Z - first.Z) / length * side,
                                  (last.X - first.X) / length * side};
      const tpj::AddPath path{
          tpj::PathKind::Guest,
          {tpj::OnSegment{segment.Key, 0.5},
           tpj::NewPoint{{middle.X + normal.X * reach, middle.Z + normal.Z * reach}}}};
      if (tpj::isAccepted(world, path)) {
        return path;
      }
    }
  }
  throw std::runtime_error("full.park holds no guest segment that accepts a path from its side");
}

// Runs the frames of the drag: a tick, the kept preview of the edit at the frame, and its ghost.
// editAt refills the drag's held edit for a frame.
template <typename EditAt> void dragFrames(Drag &drag, size_t frames, EditAt editAt) {
  for (size_t frame = 0; frame < frames; ++frame) {
    tpj::check::watchTick(drag.World.Tick);
    tpj::check::watchWork("the drag's tick");
    tpj::stepWorld(drag.World, drag.Stepping);
    if (!drag.Held) {
      throw std::logic_error("a drag holds no edit");
    }
    tpj::ParkEdit &edit = *drag.Held;
    editAt(edit, frame);
    tpj::check::watchWork("the drag's preview");
    tpj::keepPreview(drag.Kept, drag.World, drag.Held, false);
    if (!tpj::isAccepted(drag.Kept.Made.Planned)) {
      throw std::runtime_error("the world refuses a drag's edit");
    }
    tpj::check::watchWork("the drag's ghost");
    tpj::buildGhostMesh(drag.World, edit, drag.Kept.Made.Planned, drag.Kept.Made.Candidate,
                        drag.Ghost);
  }
}

// Drags the lowest-keyed shop along its face and draws a guest path out and back from a guest
// segment, each on its own copy of the world, and reports the blocks the watched frames freed that
// no excused step allocated or freed.
int checkDrags(const tpj::World &world) {
  const tpj::ParkBox shop = lowestShop(world);
  Drag shopDrag(world);
  shopDrag.Held = tpj::MoveBox{shop.Key, shop.At};
  const auto moveShop = [&shop](tpj::ParkEdit &edit, size_t frame) {
    std::get<tpj::MoveBox>(edit).At = alongFace(shop, dragOffset(frame));
  };
  Drag pathDrag(world);
  const tpj::AddPath start = spurPath(world, DRAG_START_REACH);
  pathDrag.Held = start;
  const tpj::ParkPoint from = std::get<tpj::NewPoint>(start.Stops[1]).At;
  const std::optional<tpj::ParkPoint> middleAt = tpj::stopPosition(world, start.Stops[0]);
  if (!middleAt) {
    throw std::runtime_error("the dragged path's first stop has no position");
  }
  const tpj::ParkPoint middle = *middleAt;
  const double reach = std::hypot(from.X - middle.X, from.Z - middle.Z);
  const tpj::ParkPoint out{(from.X - middle.X) / reach, (from.Z - middle.Z) / reach};
  const auto drawPathOut = [from, out](tpj::ParkEdit &edit, size_t frame) {
    const double offset = dragOffset(frame);
    std::get<tpj::AddPath>(edit).Stops[1] =
        tpj::NewPoint{{from.X + out.X * offset, from.Z + out.Z * offset}};
  };
  dragFrames(shopDrag, WARM_FRAMES, moveShop);
  dragFrames(pathDrag, WARM_FRAMES, drawPathOut);
  startWatchingSteps(world.Tick);
  shopDrag.Steps.Watching = true;
  pathDrag.Steps.Watching = true;
  dragFrames(shopDrag, WATCHED_FRAMES, moveShop);
  dragFrames(pathDrag, WATCHED_FRAMES, drawPathOut);
  shopDrag.Steps.Watching = false;
  pathDrag.Steps.Watching = false;
  stopWatchingSteps();
  return reportUnexcused("the watched drags");
}

// Whether the world accepts the move of the point, with no crossings.
bool movesClean(const tpj::World &world, const tpj::MovePoint &move) {
  return tpj::isAccepted(world, move) && tpj::pathCrossings(world, move).empty();
}

// The first path point, in key order, that the world lets move 0.5 m along +x and back, with no
// crossings either way: the move there, and the move back.
std::pair<tpj::MovePoint, tpj::MovePoint> pointThereAndBack(const tpj::World &world) {
  for (const tpj::ParkPathPoint &point : tpj::parkPathPoints(world)) {
    const tpj::MovePoint there{point.Key, tpj::NewPoint{{point.At.X + 0.5, point.At.Z}}};
    const tpj::MovePoint back{point.Key, tpj::NewPoint{point.At}};
    if (!movesClean(world, there)) {
      continue;
    }
    tpj::CommandQueue queue;
    queue.push(there);
    tpj::World moved = tpj::copyWorld(world);
    tpj::commitCommands(moved, queue);
    if (movesClean(moved, back)) {
      return {there, back};
    }
  }
  throw std::runtime_error("full.park holds no path point that moves there and back cleanly");
}

// The first guest segment's width set 0.5 m wider by a RestoreIntent, as an undo sets it, and the
// RestoreIntent that sets it back.
std::pair<tpj::RestoreIntent, tpj::RestoreIntent> widthThereAndBack(const tpj::World &world) {
  std::vector<tpj::SegmentRecord> records;
  tpj::segmentRecords(world, records);
  for (const tpj::ParkSegment &segment : tpj::parkSegments(world)) {
    if (segment.Kind != tpj::PathKind::Guest || segment.Line.empty()) {
      continue;
    }
    const auto record = std::ranges::find(records, segment.Key, &tpj::SegmentRecord::Key);
    tpj::RestoreIntent wider;
    wider.Set.Segments.push_back(*record);
    wider.Set.Segments.back().Width += 0.5;
    tpj::RestoreIntent back;
    back.Set.Segments.push_back(*record);
    if (tpj::isAccepted(world, wider)) {
      return {wider, back};
    }
  }
  throw std::runtime_error("full.park holds no guest segment the world lets widen");
}

// Commits each edit there and back, round after round, on a copy of the world, and reports the
// blocks the watched rounds freed that no excused step allocated or freed.
int checkCommits(const tpj::World &start) {
  tpj::World world = tpj::copyWorld(start);
  const tpj::ParkBox shop = lowestShop(world);
  const tpj::MoveBox shopThere{shop.Key, alongFace(shop, DRAG_STEP)};
  const tpj::MoveBox shopBack{shop.Key, shop.At};
  if (!tpj::isAccepted(world, shopThere)) {
    throw std::runtime_error("the world refuses the shop's move along its face");
  }
  const auto [pointThere, pointBack] = pointThereAndBack(world);
  const auto [widthThere, widthBack] = widthThereAndBack(world);
  tpj::TickBuffers buffers;
  WatchedSteps steps;
  buffers.get<tpj::ResolveTraceSlot>().Trace = &steps;
  tpj::CommandQueue queue;
  const auto round = [&]() {
    const auto commit = [&](const auto &command) {
      queue.push(command);
      tpj::commitCommands(world, queue, buffers);
    };
    commit(shopThere);
    commit(shopBack);
    commit(pointThere);
    commit(pointBack);
    commit(widthThere);
    commit(widthBack);
  };
  for (uint64_t warm = 0; warm < WARM_ROUNDS; ++warm) {
    round();
  }
  startWatchingSteps(world.Tick);
  tpj::check::watchWork("the commits");
  steps.Watching = true;
  for (uint64_t watched = 0; watched < WATCHED_ROUNDS; ++watched) {
    round();
  }
  steps.Watching = false;
  stopWatchingSteps();
  return reportUnexcused("the watched commits");
}

} // namespace

int main() {
  if (!SDL_SetMemoryFunctions(tpj::check::watchedMalloc, tpj::check::watchedCalloc,
                              tpj::check::watchedRealloc, tpj::check::watchedFree)) {
    std::cout << "allocation check: cannot give SDL the watched memory functions\n";
    return 2;
  }
  try {
    const std::string text = readText(TPJ_PARKS_DIR "/stress/full.park");
    tpj::World world = tpj::loadWorld(tpj::makeParkSchema(), text);
    tpj::resolveWorld(world);
    tpj::ParkSession session(std::move(world));
    SteadyPlay play;
    if (checkTicks(play, session) != 0 || checkEdits(play, session) != 0) {
      return 1;
    }
    // The drags and the commits both run before the stale steps are found, since an excused step
    // is seen across both.
    const int drags = checkDrags(session.world());
    const int commits = checkCommits(session.world());
    if (drags != 0 || commits != 0 || reportStaleSteps() != 0) {
      return 1;
    }
    std::cout << "allocation check: " << WATCHED_TICKS
              << " ticks of full.park, the scene's sync, the preview, the overlays, the tools, the "
                 "panels, the uploads, and the drawing, "
              << WATCHED_ROUNDS << " undos and redos with the app's work after each, "
              << WATCHED_FRAMES << " frames each of the watched drags of a shop and a path, and "
              << WATCHED_ROUNDS
              << " rounds of the watched commits, freed no block they allocated but those of the "
              << EXCUSED_STEPS.size() << " excused resolution steps\n";
    return 0;
  } catch (const std::exception &error) {
    std::cout << "allocation check: " << error.what() << '\n';
    return 2;
  }
}
