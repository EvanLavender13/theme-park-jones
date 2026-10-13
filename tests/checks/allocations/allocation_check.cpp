// The allocation check (decision 0041): the work the app repeats in steady play, repeated on
// full.park after a warm-up, must free no heap block it allocated while it repeats. Each tick is
// followed by the scene's sync, as the app runs it: once for the frame with the tick, and once for
// a frame without one. Then come the previews of two edits held still with no overlay, each with a
// look, synced as the app's frame syncs its preview, twice. At every 60th tick come the overlays:
// each need's with no edit, and food's with a guest path the world refuses, also synced twice.
// Last come the tools: each tool's frame work with the pointer held still, and the look's marks
// built again for a look that changes at each tick, as a moving pointer changes it. Then come the
// panels' data, with a guest, a shop, a bench, and a toilet block inspected. Before all of that,
// the check adds guest paths with a rounded corner and a taper, which full.park lacks.

#include "watch.h"

#include "app/input/interaction.h"
#include "app/input/orbit_camera.h"
#include "app/scene/scene_sync.h"
#include "app/session/park_session.h"
#include "app/ui/panel_data.h"
#include "legible/arrivals.h"
#include "legible/inspect.h"
#include "legible/preview.h"
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

#include <algorithm>
#include <array>
#include <cmath>
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
  tpj::SceneSync Scene;
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
  syncPreview(play.Moving, session);
}

// Steps the park through the warm-up, syncing as the watched ticks do, so the kept lists grow to
// what the park needs. Throws when the held shop move shows no connected shop context, or the held
// panels would check nothing.
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
    syncTools(play, session);
    syncPanels(play, session);
    if (tick == 0) {
      requireShownPanels(play, session);
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
      << " ticks of full.park, the scene's sync, the preview, the overlays, the tools, and the "
         "panels freed no block they allocated\n";
  return 0;
}

// Steps the park through the app's own session, syncing the scene and the previews after each
// tick, and reports the blocks the watched ticks freed.
int checkTicks(tpj::ParkSession &session) {
  SteadyPlay play;
  addCurve(session);
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
    tpj::check::watchWork("the tools");
    syncTools(play, session);
    tpj::check::watchWork("the panels");
    syncPanels(play, session);
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
