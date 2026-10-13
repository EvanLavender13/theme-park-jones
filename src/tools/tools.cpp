#include "tools/tools.h"

#include "sim/park/geometry.h"
#include "sim/park/surface.h"
#include "sim/routes/networks.h"
#include "sim/tick_buffers.h"
#include "tools/internal/snapping.h"
#include "tools/internal/tool_lists.h"

#include <algorithm>
#include <array>
#include <math.h>
#include <memory>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// How far the point lies from the corner's point along its Toward.
double depthAlong(const ParkCorner &corner, ParkPoint point) {
  return (point.X - corner.At.X) * corner.Toward.X + (point.Z - corner.At.Z) * corner.Toward.Z;
}

// The corner the point makes, or none.
std::optional<ParkCorner> cornerOf(const ToolPark &park, EntityKey point) {
  for (const ParkCorner &corner : park.Corners) {
    if (corner.Point == point) {
      return corner;
    }
  }
  return std::nullopt;
}

// The edge at the point that starts from the segment's left side, or none.
std::optional<ParkEdge> edgeOf(const ToolPark &park, EntityKey point, EntityKey segment) {
  for (const ParkEdge &edge : park.Edges) {
    if (edge.Point == point && edge.Segment == segment) {
      return edge;
    }
  }
  return std::nullopt;
}

// The taper at the point, or none.
std::optional<ParkTaper> taperOf(const ToolPark &park, EntityKey point) {
  for (const ParkTaper &taper : park.Tapers) {
    if (taper.Point == point) {
      return taper;
    }
  }
  return std::nullopt;
}

// How far the point lies from where the taper's sides would stop without curves, along the narrower
// segment.
double depthAlong(const ParkTaper &taper, ParkPoint point) {
  return (point.X - taper.At.X) * taper.Along.X + (point.Z - taper.At.Z) * taper.Along.Z;
}

// How far the point lies from where the edge's sides meet, along its Toward.
double depthAlong(const ParkEdge &edge, ParkPoint point) {
  return (point.X - edge.Sharp.X) * edge.Toward.X + (point.Z - edge.Sharp.Z) * edge.Toward.Z;
}

// The segment the key holds, or nullptr.
const ParkSegment *segmentOf(const ToolPark &park, EntityKey key) {
  for (const ParkSegment &segment : park.Segments) {
    if (segment.Key == key) {
      return &segment;
    }
  }
  return nullptr;
}

// How many segments use the point.
size_t segmentCount(const ToolPark &park, EntityKey point) {
  return static_cast<size_t>(
      std::ranges::count_if(park.Segments, [point](const ParkSegment &segment) {
        return segment.From == point || segment.To == point;
      }));
}

bool isPlaceTool(ToolKind kind) {
  return kind == ToolKind::PlaceShop || kind == ToolKind::PlaceDepot ||
         kind == ToolKind::PlaceBench || kind == ToolKind::PlaceToilet;
}

BoxKind placedKind(ToolKind kind) {
  switch (kind) {
  case ToolKind::PlaceDepot:
    return BoxKind::Depot;
  case ToolKind::PlaceBench:
    return BoxKind::Bench;
  case ToolKind::PlaceToilet:
    return BoxKind::ToiletBlock;
  default:
    return BoxKind::Shop;
  }
}

bool isPathTool(ToolKind kind) {
  return kind == ToolKind::GuestPath || kind == ToolKind::BackstagePath;
}

PathKind drawnKind(ToolKind kind) {
  return kind == ToolKind::GuestPath ? PathKind::Guest : PathKind::Backstage;
}

// Puts the drawn stops' positions in the park's Positions list, as stopPositions gives them.
// False when a stop has none.
bool drawnPositions(const ToolState &tool, ToolPark &park, const World &world) {
  std::vector<ParkPoint> &positions = park.Lists->Positions;
  positions.clear();
  return std::ranges::all_of(tool.Drawn, [&](const PathStop &stop) {
    return appendStopPosition(world, park.Corners, stop, positions);
  });
}

// The stop a path tool's press would append: the snapped pointer, unless it lies within
// FINISH_REACH of the last stop, where a press finishes the path instead.
std::optional<PathStop> nextStop(const ToolState &tool, ToolPark &park, const World &world) {
  if (!tool.Pointer || !tool.Snapped) {
    return std::nullopt;
  }
  const PathStop &stop = tool.Snapped->Stop;
  if (!tool.Drawn.empty()) {
    const std::vector<ParkPoint> &at = park.Lists->Positions;
    if (drawnPositions(tool, park, world) &&
        appendStopPosition(world, park.Corners, stop, park.Lists->Positions) &&
        distanceBetween(at.back(), at[at.size() - 2]) <= FINISH_REACH) {
      return std::nullopt;
    }
  }
  return stop;
}

// Whether the world accepts the path through the stops so far and the next stop. Before the first
// stop, there is no path to refuse.
bool acceptsStop(const ToolState &tool, const World &world, const PathStop &next) {
  if (tool.Drawn.empty()) {
    return true;
  }
  AddPath path{drawnKind(tool.Kind), tool.Drawn};
  path.Stops.push_back(next);
  return isAccepted(world, path);
}

// Makes the kept edit an AddPath, giving it the stops of the last path it held, and gives it.
AddPath &keptPath(KeptEdit &kept) {
  if (!kept.Edit || !std::holds_alternative<AddPath>(*kept.Edit)) {
    kept.Edit = AddPath{};
    std::swap(std::get<AddPath>(*kept.Edit).Stops, kept.SpareStops);
  }
  return std::get<AddPath>(*kept.Edit);
}

// Puts the edit, or none, in the kept edit. A path it held keeps its stops in SpareStops.
void keepEdit(KeptEdit &kept, std::optional<ParkEdit> edit) {
  if (kept.Edit && std::holds_alternative<AddPath>(*kept.Edit)) {
    std::swap(std::get<AddPath>(*kept.Edit).Stops, kept.SpareStops);
  }
  kept.Edit = std::move(edit);
}

// A path tool's edit: the stops so far, followed by the next stop while not holding, refilled in
// the stops the kept edit holds.
void pathEdit(const ToolState &tool, ToolPark &park, const World &world, KeptEdit &kept) {
  std::optional<PathStop> next;
  if (!tool.Holding && !tool.Drawn.empty()) {
    next = nextStop(tool, park, world);
  }
  if (tool.Drawn.size() + (next ? 1 : 0) < 2) {
    keepEdit(kept, std::nullopt);
    return;
  }
  AddPath &path = keptPath(kept);
  path.Kind = drawnKind(tool.Kind);
  copyGrowing(path.Stops, std::span<const PathStop>(tool.Drawn));
  if (next) {
    path.Stops.push_back(*next);
  }
}

// The pose the box the key holds has, if it holds one.
std::optional<Pose> boxPose(const ToolPark &park, EntityKey key) {
  for (const ParkBox &box : park.Boxes) {
    if (box.Key == key) {
      return box.At;
    }
  }
  return std::nullopt;
}

// The InsertPoint a click at the point makes on the segment under it, or none: at the nearest
// place on its ground line, on the arc of a rounded corner at either end, or else on its straight
// part.
std::optional<InsertPoint> insertAt(const ToolPark &park, const World &world, ParkPoint point) {
  const std::optional<EntityKey> key = segmentAt(park, point);
  if (!key) {
    return std::nullopt;
  }
  const std::vector<ParkCorner> &corners = park.Corners;
  for (const ParkSegment &segment : park.Segments) {
    if (segment.Key != *key) {
      continue;
    }
    const std::optional<ParkPoint> from = stopPosition(world, OnPoint{segment.From});
    const std::optional<ParkPoint> to = stopPosition(world, OnPoint{segment.To});
    if (!from || !to) {
      return std::nullopt;
    }
    // The straight part's place first, so it wins a tie with an arc's. A segment has one straight
    // part and an arc at each end at most.
    std::array<Snap, 3> found{};
    size_t count = 0;
    if (const auto place = straightPlace(corners, segment, *from, *to, point)) {
      found.at(count++) = makeSnap(OnSegment{segment.Key, place->first}, place->second, {}, {}, {});
    }
    for (const EntityKey end : {segment.From, segment.To}) {
      if (const ParkCorner *corner = roundedAt(corners, end)) {
        if (const std::optional<Snap> place = bendPlaceNear(*corner, point)) {
          found.at(count++) = *place;
        }
      }
    }
    const std::span<const Snap> places = std::span(found).first(count);
    const auto nearest = std::ranges::min_element(places, {}, &Snap::Distance);
    if (nearest != places.end()) {
      if (const auto *onBend = std::get_if<OnBend>(&nearest->Stop)) {
        return InsertPoint{*onBend};
      }
      if (const auto *onSegment = std::get_if<OnSegment>(&nearest->Stop)) {
        return InsertPoint{*onSegment};
      }
    }
    return std::nullopt;
  }
  return std::nullopt;
}

// The guest segment under the point: the first in key order whose ground line passes within half
// its width of it, or, where the point lies on other guest paving, as at a junction, the one whose
// ground line passes nearest it, the first in key order on a tie.
const ParkSegment *guestSegmentAt(const ToolPark &park, const World &world, ParkPoint point) {
  const ParkSegment *nearest = nullptr;
  double nearestDistance = INFINITY;
  for (const ParkSegment &segment : park.Segments) {
    if (segment.Kind != PathKind::Guest) {
      continue;
    }
    const std::vector<CarrierPoint> &line = segment.Line;
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      const double distance = segmentDistance(point, line[i], line[i + 1]);
      if (distance <= 0.5 * segment.Width) {
        return &segment;
      }
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearest = &segment;
      }
    }
  }
  if (nearest == nullptr) {
    return nullptr;
  }
  // Walkways are guest paving too.
  const PavedSurface &surface = walkedSurface(world);
  const bool onPaving = std::ranges::any_of(surface.Pieces, [point](const SurfacePiece &piece) {
    return piece.Kind == PathKind::Guest && pieceDistance(piece, point) == 0.0;
  });
  return onPaving ? nearest : nullptr;
}

// The Furnish tool's edit: the guest segment under the pointer with the tool's choice on both
// sides, or on the side of the line from its From point to its To point that the pointer lies on,
// the other side keeping its furnishing. A choice with no kind removes furnishing.
std::optional<ParkEdit> furnishEdit(const ToolState &tool, const ToolPark &park,
                                    const World &world) {
  if (!tool.Pointer) {
    return std::nullopt;
  }
  const ParkSegment *segment = guestSegmentAt(park, world, *tool.Pointer);
  if (segment == nullptr) {
    return std::nullopt;
  }
  std::optional<SideFurnishing> chosen;
  if (tool.Furnish.Kind) {
    chosen = SideFurnishing{*tool.Furnish.Kind, tool.Furnish.Spacing};
  }
  if (tool.Furnish.BothSides) {
    return SetFurnishing{segment->Key, Furnishing{chosen, chosen}};
  }
  const std::optional<ParkPoint> from = stopPosition(world, OnPoint{segment->From});
  const std::optional<ParkPoint> to = stopPosition(world, OnPoint{segment->To});
  if (!from || !to) {
    return std::nullopt;
  }
  // A quarter turn counter-clockwise of the segment's direction, seen from above, is its left.
  const double alongX = to->X - from->X;
  const double alongZ = to->Z - from->Z;
  const double leftward =
      ((tool.Pointer->X - from->X) * alongZ) - ((tool.Pointer->Z - from->Z) * alongX);
  Furnishing furnishing = segment->Furnished;
  (leftward > 0.0 ? furnishing.Left : furnishing.Right) = chosen;
  return SetFurnishing{segment->Key, furnishing};
}

// The edit a dragged radius handle, edge handle, taper handle, or side makes with the pointer where
// it is: the arc's middle moved as far into the corner as the pointer has moved, the edge's middle
// moved as far along its Toward as the pointer has moved, where the longest curve starts moved as
// far along the narrower segment as the pointer has moved, or the width grown by twice how far the
// pointer has moved away from the ground line. None when the corner, edge, taper, or segment is
// gone.
std::optional<ParkEdit> handleEdit(const EditHandle &handle, double grabFrom, const ToolPark &park,
                                   ParkPoint pointer) {
  if (handle.Kind == HandleKind::Radius) {
    const std::optional<ParkCorner> corner = cornerOf(park, handle.Key);
    if (!corner) {
      return std::nullopt;
    }
    const double radius = cornerRadiusFor(*corner, depthAlong(*corner, pointer) + grabFrom);
    return SetCornerRadius{corner->Point, std::min(radius, corner->MaxRadius)};
  }
  if (handle.Kind == HandleKind::Edge) {
    const std::optional<ParkEdge> edge = edgeOf(park, handle.Key, handle.Segment);
    if (!edge) {
      return std::nullopt;
    }
    const double radius = edgeRadiusFor(*edge, depthAlong(*edge, pointer) + grabFrom);
    return SetEdgeRadius{edge->Segment, edge->Point,
                         std::clamp(radius, edge->MinRadius, edge->MaxRadius)};
  }
  if (handle.Kind == HandleKind::Taper) {
    const std::optional<ParkTaper> taper = taperOf(park, handle.Key);
    if (!taper) {
      return std::nullopt;
    }
    const double most = std::max(taper->MaxLength, MIN_TAPER_LENGTH);
    return SetTaperLength{
        taper->Point, std::clamp(depthAlong(*taper, pointer) + grabFrom, MIN_TAPER_LENGTH, most)};
  }
  const ParkSegment *segment = segmentOf(park, handle.Key);
  if (segment == nullptr || segment->Line.size() < 2) {
    return std::nullopt;
  }
  const double width = std::clamp(2.0 * lineDistance(pointer, segment->Line) + grabFrom,
                                  MIN_PATH_WIDTH, MAX_PATH_WIDTH);
  return SetSegmentWidth{segment->Key, width};
}

// Where a dragged point lands, from the snap in effect: its stop as a PointTarget, or a new point
// at where it lands for a stop no PointTarget names, or at PointTo without a snap.
PointTarget pointTarget(const ToolState &tool) {
  if (!tool.Snapped) {
    return NewPoint{tool.PointTo};
  }
  const PathStop &stop = tool.Snapped->Stop;
  if (const auto *onPoint = std::get_if<OnPoint>(&stop)) {
    return *onPoint;
  }
  if (const auto *onSegment = std::get_if<OnSegment>(&stop)) {
    return *onSegment;
  }
  if (const auto *onBend = std::get_if<OnBend>(&stop)) {
    return *onBend;
  }
  return NewPoint{tool.Snapped->At};
}

// Edit path's edit: the held handle's or point's once it has been dragged, the held insert, or,
// while not holding, the insert a click away from any handle, point, or side would make.
std::optional<ParkEdit> editPathEdit(const ToolState &tool, const ToolPark &park,
                                     const World &world) {
  if (tool.Holding && tool.Grabbed) {
    if (!tool.Dragged || !tool.Pointer) {
      return std::nullopt;
    }
    return handleEdit(*tool.Grabbed, tool.GrabFrom, park, *tool.Pointer);
  }
  if (tool.Holding && tool.Held != NULL_KEY) {
    if (!tool.Dragged) {
      return std::nullopt;
    }
    return MovePoint{tool.Held, pointTarget(tool)};
  }
  if (tool.Holding && tool.Inserting) {
    return *tool.Inserting;
  }
  if (tool.Holding || !tool.Pointer || handleAt(park, *tool.Pointer) ||
      pointAt(park, *tool.Pointer) || sideAt(park, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<InsertPoint> insert = insertAt(park, world, *tool.Pointer)) {
    return *insert;
  }
  return std::nullopt;
}

// Edit path's press takes, in order, the radius, edge, or taper handle, the point, the segment's
// sides, or the insert under the pointer.
void pressEditPath(ToolState &tool, const ToolPark &park, const World &world, ParkPoint pointer) {
  if (const std::optional<EditHandle> handle = handleAt(park, pointer)) {
    if (handle->Kind == HandleKind::Radius) {
      if (const std::optional<ParkCorner> corner = cornerOf(park, handle->Key)) {
        tool.Holding = true;
        tool.Grabbed = handle;
        tool.GrabFrom = distanceBetween(corner->At, corner->Middle) - depthAlong(*corner, pointer);
        return;
      }
    } else if (handle->Kind == HandleKind::Taper) {
      if (const std::optional<ParkTaper> taper = taperOf(park, handle->Key)) {
        tool.Holding = true;
        tool.Grabbed = handle;
        tool.GrabFrom = taper->Length - depthAlong(*taper, pointer);
        return;
      }
    } else if (const std::optional<ParkEdge> edge = edgeOf(park, handle->Key, handle->Segment)) {
      tool.Holding = true;
      tool.Grabbed = handle;
      tool.GrabFrom = depthAlong(*edge, edge->Middle) - depthAlong(*edge, pointer);
      return;
    }
  }
  if (const std::optional<EntityKey> point = pointAt(park, pointer)) {
    if (const std::optional<ParkPoint> at = stopPosition(world, OnPoint{*point})) {
      tool.Holding = true;
      tool.Held = *point;
      tool.PointTo = *at;
      tool.GrabOffset = ParkPoint{at->X - pointer.X, at->Z - pointer.Z};
      return;
    }
  }
  if (const std::optional<EntityKey> key = sideAt(park, pointer)) {
    if (const ParkSegment *segment = segmentOf(park, *key)) {
      tool.Holding = true;
      tool.Grabbed = EditHandle{HandleKind::Side, *key, {}};
      tool.GrabFrom = segment->Width - 2.0 * lineDistance(pointer, segment->Line);
      return;
    }
  }
  if (const std::optional<InsertPoint> insert = insertAt(park, world, pointer)) {
    tool.Holding = true;
    tool.Inserting = *insert;
  }
}

// The kind of the segments at the point, or none when no segment uses it.
std::optional<PathKind> pointKind(const ToolPark &park, EntityKey point) {
  for (const ParkSegment &segment : park.Segments) {
    if (segment.From == point || segment.To == point) {
      return segment.Kind;
    }
  }
  return std::nullopt;
}

// Finds the snap in effect for the tool as it is now: a path tool's next stop at the pointer, or
// where Edit path's dragged point lands. Keeps what the last snap held when asked to.
void resnap(ToolState &tool, ToolPark &park, const World &world, bool keepHolds) {
  std::array<SnapHold, 2> held{};
  size_t heldCount = 0;
  if (keepHolds && tool.Snapped) {
    held = tool.Snapped->Holds;
    heldCount = tool.Snapped->HoldCount;
  }
  tool.Snapped.reset();
  if (isPlaceTool(tool.Kind)) {
    // A press keeps the frontage it was made with through the hold.
    if (!tool.Holding) {
      tool.Fronted = tool.Pointer && !tool.SnapsOff
                         ? frontagePose(world, park, placedKind(tool.Kind), *tool.Pointer)
                         : std::nullopt;
    }
    return;
  }
  SnapQuery query;
  query.CatchReach = SNAP_CATCH_PIXELS * tool.MetersPerPixel;
  query.ReleaseReach = SNAP_RELEASE_PIXELS * tool.MetersPerPixel;
  query.Held = std::span(held).first(heldCount);
  if (isPathTool(tool.Kind) && tool.Pointer) {
    query.Kind = drawnKind(tool.Kind);
    query.Drawn = tool.Drawn;
    query.Pointer = *tool.Pointer;
  } else if (tool.Kind == ToolKind::EditPath && tool.Holding && tool.Held != NULL_KEY &&
             tool.Dragged) {
    const std::optional<PathKind> kind = pointKind(park, tool.Held);
    if (!kind || tool.SnapsOff) {
      tool.Snapped = PointerSnap{tool.PointTo, NewPoint{tool.PointTo}};
      return;
    }
    query.Kind = *kind;
    query.Dragged = tool.Held;
    query.Pointer = tool.PointTo;
  } else {
    return;
  }
  if (tool.SnapsOff) {
    tool.Snapped = PointerSnap{query.Pointer, NewPoint{query.Pointer}};
    return;
  }
  tool.Snapped = findSnap(world, park, query);
}

} // namespace

ToolPark::ToolPark() : Lists(std::make_unique<ToolLists>()) {}
ToolPark::~ToolPark() = default;
ToolPark::ToolPark(ToolPark &&) noexcept = default;
ToolPark &ToolPark::operator=(ToolPark &&) noexcept = default;

void keepToolPark(const World &world, ToolPark &park) {
  TickBuffers &buffers = park.Lists->Buffers;
  parkPathPoints(world, park.Points);
  parkSegments(world, park.Segments, buffers);
  parkCorners(world, park.Corners, buffers);
  parkEdges(world, park.Edges, buffers);
  parkTapers(world, park.Tapers, buffers);
  parkBoxes(world, park.Boxes);
  parkDoors(world, PathKind::Guest, park.GuestDoors, buffers);
  parkDoors(world, PathKind::Backstage, park.BackstageDoors, buffers);
  const PavedSurface &surface = resolvedSurface(world);
  edgeSteps(surface, PathKind::Guest, park.Lists->Frontage.GuestSteps);
  edgeSteps(surface, PathKind::Backstage, park.Lists->Frontage.BackstageSteps);
}

void selectTool(ToolState &tool, ToolKind kind) {
  tool.Kind = kind;
  tool.Holding = false;
  tool.Dragged = false;
  tool.Held = NULL_KEY;
  tool.Inserting.reset();
  tool.Grabbed.reset();
  tool.Drawn.clear();
  tool.Snapped.reset();
  tool.Fronted.reset();
}

void movePointer(ToolState &tool, ToolPark &park, const World &world,
                 const PointerSample &pointer) {
  const std::optional<ParkPoint> &ground = pointer.Ground;
  tool.MetersPerPixel = pointer.MetersPerPixel;
  tool.SnapsOff = pointer.SnapsOff;
  // A hold counts as a drag only once the ground point changes, since a recomputed position can
  // differ from the one at the press by rounding alone.
  if (tool.Holding && ground && !(tool.Pointer && *tool.Pointer == *ground)) {
    tool.Dragged = true;
  }
  tool.Pointer = ground;
  if (tool.Holding && ground) {
    if (isPlaceTool(tool.Kind) && !tool.Fronted) {
      const double dx = ground->X - tool.Landing.X;
      const double dz = ground->Z - tool.Landing.Z;
      if (dx * dx + dz * dz >= MIN_FACING_DRAG * MIN_FACING_DRAG) {
        tool.Landing.FacingX = dx;
        tool.Landing.FacingZ = dz;
      }
    } else if (tool.Kind == ToolKind::MoveBox) {
      tool.Target.X = ground->X + tool.GrabOffset.X;
      tool.Target.Z = ground->Z + tool.GrabOffset.Z;
    } else if (tool.Kind == ToolKind::EditPath && tool.Held != NULL_KEY) {
      tool.PointTo = ParkPoint{ground->X + tool.GrabOffset.X, ground->Z + tool.GrabOffset.Z};
    }
  }
  resnap(tool, park, world, true);
}

void pressPointer(ToolState &tool, ToolPark &park, const World &world) {
  if (tool.Holding) {
    return;
  }
  tool.Dragged = false;
  if (tool.Kind == ToolKind::Delete || tool.Kind == ToolKind::RemovePoint ||
      tool.Kind == ToolKind::Furnish) {
    tool.Holding = true;
  } else if (isPathTool(tool.Kind) && tool.Pointer) {
    if (const std::optional<PathStop> next = nextStop(tool, park, world)) {
      if (acceptsStop(tool, world, *next)) {
        tool.Drawn.push_back(*next);
      }
    } else {
      tool.Holding = true;
    }
  } else if (isPlaceTool(tool.Kind) && tool.Pointer) {
    tool.Holding = true;
    tool.Landing =
        tool.Fronted.value_or(Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ});
  } else if (tool.Kind == ToolKind::MoveBox && tool.Pointer) {
    const std::optional<EntityKey> key = boxAt(park, *tool.Pointer);
    const std::optional<Pose> pose = key ? boxPose(park, *key) : std::nullopt;
    if (pose) {
      tool.Holding = true;
      tool.Held = *key;
      tool.Target = *pose;
      tool.GrabOffset = ParkPoint{pose->X - tool.Pointer->X, pose->Z - tool.Pointer->Z};
    }
  } else if (tool.Kind == ToolKind::EditPath && tool.Pointer) {
    pressEditPath(tool, park, world, *tool.Pointer);
  }
  resnap(tool, park, world, false);
}

void releasePointer(ToolState &tool, ToolPark &park, const World &world, KeptEdit &kept) {
  if (!tool.Holding) {
    keepEdit(kept, std::nullopt);
    return;
  }
  tentativeEdit(tool, park, world, kept);
  if (isPlaceTool(tool.Kind)) {
    tool.FacingX = tool.Landing.FacingX;
    tool.FacingZ = tool.Landing.FacingZ;
  }
  if (isPathTool(tool.Kind)) {
    tool.Drawn.clear();
  }
  tool.Holding = false;
  tool.Dragged = false;
  tool.Held = NULL_KEY;
  tool.Inserting.reset();
  tool.Grabbed.reset();
  resnap(tool, park, world, false);
}

namespace {

// The edit of a tool other than a path tool.
std::optional<ParkEdit> otherEdit(const ToolState &tool, const ToolPark &park, const World &world) {
  if (isPlaceTool(tool.Kind)) {
    if (tool.Holding) {
      return AddBox{placedKind(tool.Kind), tool.Landing};
    }
    if (tool.Fronted) {
      return AddBox{placedKind(tool.Kind), *tool.Fronted};
    }
    if (tool.Pointer) {
      return AddBox{placedKind(tool.Kind),
                    Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ}};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::MoveBox) {
    if (tool.Holding && tool.Dragged) {
      return MoveBox{tool.Held, tool.Target};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::EditPath) {
    return editPathEdit(tool, park, world);
  }
  if (tool.Kind == ToolKind::RemovePoint && tool.Pointer) {
    if (const std::optional<EntityKey> point = pointAt(park, *tool.Pointer)) {
      return RemovePoint{*point};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::Furnish) {
    return furnishEdit(tool, park, world);
  }
  if (tool.Kind == ToolKind::Delete && tool.Pointer) {
    if (const std::optional<EntityKey> box = boxAt(park, *tool.Pointer)) {
      return DeleteBox{*box};
    }
    if (const std::optional<EntityKey> segment = segmentAt(park, *tool.Pointer)) {
      return DeleteSegment{*segment};
    }
  }
  return std::nullopt;
}

} // namespace

void tentativeEdit(const ToolState &tool, ToolPark &park, const World &world, KeptEdit &kept) {
  if (isPathTool(tool.Kind)) {
    pathEdit(tool, park, world, kept);
    return;
  }
  keepEdit(kept, otherEdit(tool, park, world));
}

std::optional<ParkEdit> tentativeEdit(const ToolState &tool, ToolPark &park, const World &world) {
  KeptEdit kept;
  tentativeEdit(tool, park, world, kept);
  return std::move(kept.Edit);
}

std::optional<PathStart> pathStart(const ToolState &tool, ToolPark &park, const World &world) {
  if (!isPathTool(tool.Kind) || !tool.Drawn.empty() || !tool.Pointer) {
    return std::nullopt;
  }
  if (!tool.Snapped) {
    return std::nullopt;
  }
  const PathStop &stop = tool.Snapped->Stop;
  std::vector<ParkPoint> &at = park.Lists->Positions;
  at.clear();
  if (!appendStopPosition(world, park.Corners, stop, at)) {
    return std::nullopt;
  }
  return PathStart{at.back(), drawnKind(tool.Kind), !std::holds_alternative<NewPoint>(stop)};
}

std::optional<ParkPoint> pathFinish(const ToolState &tool, ToolPark &park, const World &world) {
  if (!isPathTool(tool.Kind) || tool.Drawn.size() < 2 || !tool.Pointer ||
      nextStop(tool, park, world)) {
    return std::nullopt;
  }
  if (!drawnPositions(tool, park, world)) {
    return std::nullopt;
  }
  return park.Lists->Positions.back();
}

std::optional<EntityKey> highlightedEntity(const ToolState &tool, const ToolPark &park,
                                           const World &world) {
  if (tool.Kind == ToolKind::MoveBox && !tool.Holding && tool.Pointer) {
    return boxAt(park, *tool.Pointer);
  }
  if (tool.Kind == ToolKind::EditPath && !tool.Holding && tool.Pointer &&
      !handleAt(park, *tool.Pointer)) {
    return pointAt(park, *tool.Pointer);
  }
  if (tool.Kind == ToolKind::RemovePoint && !tool.Holding && tool.Pointer) {
    const std::optional<EntityKey> point = pointAt(park, *tool.Pointer);
    if (point && segmentCount(park, *point) == 2) {
      return point;
    }
  }
  if (tool.Kind == ToolKind::Furnish && !tool.Holding && tool.Pointer) {
    if (const ParkSegment *segment = guestSegmentAt(park, world, *tool.Pointer)) {
      return segment->Key;
    }
  }
  return std::nullopt;
}

void setFurnishChoice(ToolState &tool, const FurnishChoice &choice) { tool.Furnish = choice; }

std::optional<EntityKey> boxAt(const ToolPark &park, ParkPoint point) {
  for (const ParkBox &box : park.Boxes) {
    const FootprintSize size = boxSize(box.Kind);
    const std::optional<Footprint> footprint = footprintOf(box.At, size);
    if (!footprint) {
      continue;
    }
    const double dx = point.X - box.At.X;
    const double dz = point.Z - box.At.Z;
    const double along = dx * footprint->Forward.X + dz * footprint->Forward.Z;
    const double across = dx * footprint->Right.X + dz * footprint->Right.Z;
    if (fabs(along) <= 0.5 * size.Depth && fabs(across) <= 0.5 * size.Width) {
      return box.Key;
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> segmentAt(const ToolPark &park, ParkPoint point) {
  for (const ParkSegment &segment : park.Segments) {
    const std::vector<CarrierPoint> &line = segment.Line;
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      if (segmentDistance(point, line[i], line[i + 1]) <= 0.5 * segment.Width) {
        return segment.Key;
      }
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> pointAt(const ToolPark &park, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestDistance = 0.0;
  for (const ParkPathPoint &candidate : park.Points) {
    const double distance = distanceBetween(point, candidate.At);
    if (distance <= POINT_REACH && (!nearest || distance < nearestDistance)) {
      nearest = candidate.Key;
      nearestDistance = distance;
    }
  }
  return nearest;
}

ParkPoint radiusHandleOf(const ParkCorner &corner) {
  const double depth = std::max(distanceBetween(corner.At, corner.Middle), RADIUS_HANDLE_DEPTH);
  return {corner.At.X + corner.Toward.X * depth, corner.At.Z + corner.Toward.Z * depth};
}

ParkPoint taperHandleOf(const ParkTaper &taper) {
  const double depth = std::max(taper.Length, std::min(TAPER_HANDLE_DEPTH, taper.MaxLength));
  return {taper.At.X + taper.Along.X * depth, taper.At.Z + taper.Along.Z * depth};
}

std::optional<EditHandle> handleAt(const ToolPark &park, ParkPoint point) {
  std::optional<EditHandle> nearest;
  double nearestDistance = 0.0;
  // Radius handles come first, so a tie goes to the first in this order.
  const auto consider = [&](const EditHandle &handle) {
    const double distance = distanceBetween(point, handle.At);
    if (distance <= HANDLE_REACH && (!nearest || distance < nearestDistance)) {
      nearest = handle;
      nearestDistance = distance;
    }
  };
  for (const ParkCorner &corner : park.Corners) {
    consider(EditHandle{HandleKind::Radius, corner.Point, radiusHandleOf(corner)});
  }
  for (const ParkEdge &edge : park.Edges) {
    consider(EditHandle{HandleKind::Edge, edge.Point, edge.Middle, edge.Segment});
  }
  for (const ParkTaper &taper : park.Tapers) {
    consider(EditHandle{HandleKind::Taper, taper.Point, taperHandleOf(taper), taper.Segment});
  }
  return nearest;
}

std::optional<EntityKey> sideAt(const ToolPark &park, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestOff = 0.0;
  for (const ParkSegment &segment : park.Segments) {
    if (segment.Line.size() < 2) {
      continue;
    }
    const double off = fabs(lineDistance(point, segment.Line) - 0.5 * segment.Width);
    if (off <= SIDE_REACH && (!nearest || off < nearestOff)) {
      nearest = segment.Key;
      nearestOff = off;
    }
  }
  return nearest;
}

void editHandles(const ToolState &tool, const ToolPark &park, std::vector<EditHandle> &handles) {
  handles.clear();
  if ((tool.Kind == ToolKind::EditPath || tool.Kind == ToolKind::RemovePoint) && !tool.Holding) {
    for (const ParkPathPoint &point : park.Points) {
      handles.push_back(EditHandle{HandleKind::Point, point.Key, point.At});
    }
  }
  if (tool.Kind == ToolKind::EditPath && !tool.Holding) {
    for (const ParkCorner &corner : park.Corners) {
      handles.push_back(EditHandle{HandleKind::Radius, corner.Point, radiusHandleOf(corner)});
    }
    for (const ParkEdge &edge : park.Edges) {
      handles.push_back(EditHandle{HandleKind::Edge, edge.Point, edge.Middle, edge.Segment});
    }
    for (const ParkTaper &taper : park.Tapers) {
      handles.push_back(
          EditHandle{HandleKind::Taper, taper.Point, taperHandleOf(taper), taper.Segment});
    }
  }
}

std::optional<EditHandle> hoveredHandle(const ToolState &tool, const ToolPark &park) {
  if (tool.Kind != ToolKind::EditPath || tool.Holding || !tool.Pointer) {
    return std::nullopt;
  }
  if (const std::optional<EditHandle> handle = handleAt(park, *tool.Pointer)) {
    return handle;
  }
  if (pointAt(park, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<EntityKey> key = sideAt(park, *tool.Pointer)) {
    return EditHandle{HandleKind::Side, *key, {}};
  }
  return std::nullopt;
}

std::span<const SnapGuide> snapGuides(const ToolState &tool) {
  if (!tool.Snapped) {
    return {};
  }
  return std::span(tool.Snapped->Guides).first(tool.Snapped->GuideCount);
}

void measuredLengths(const ToolState &tool, ToolPark &park, const World &world,
                     std::vector<MeasuredLength> &lengths) {
  lengths.clear();
  if (!tool.Snapped) {
    return;
  }
  if (isPathTool(tool.Kind) && !tool.Drawn.empty() && nextStop(tool, park, world)) {
    if (drawnPositions(tool, park, world) && !park.Lists->Positions.empty()) {
      lengths.push_back({park.Lists->Positions.back(), tool.Snapped->At});
    }
  } else if (tool.Kind == ToolKind::EditPath && tool.Held != NULL_KEY) {
    for (const ParkSegment &segment : park.Segments) {
      if (segment.From != tool.Held && segment.To != tool.Held) {
        continue;
      }
      const EntityKey end = segment.From == tool.Held ? segment.To : segment.From;
      if (const std::optional<ParkPoint> at = stopPosition(world, OnPoint{end})) {
        lengths.push_back({*at, tool.Snapped->At});
      }
    }
  }
}

} // namespace tpj
