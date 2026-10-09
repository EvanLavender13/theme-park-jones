#include "tools/tools.h"

#include "sim/park/geometry.h"
#include "sim/park/surface.h"
#include "tools/internal/snapping.h"

#include <algorithm>
#include <math.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// How far the point lies from the corner's point along its Toward.
double depthAlong(const ParkCorner &corner, ParkPoint point) {
  return (point.X - corner.At.X) * corner.Toward.X + (point.Z - corner.At.Z) * corner.Toward.Z;
}

// The corner the point makes, or none.
std::optional<ParkCorner> cornerOf(const World &world, EntityKey point) {
  for (const ParkCorner &corner : parkCorners(world)) {
    if (corner.Point == point) {
      return corner;
    }
  }
  return std::nullopt;
}

// The edge at the point that starts from the segment's left side, or none.
std::optional<ParkEdge> edgeOf(const World &world, EntityKey point, EntityKey segment) {
  for (const ParkEdge &edge : parkEdges(world)) {
    if (edge.Point == point && edge.Segment == segment) {
      return edge;
    }
  }
  return std::nullopt;
}

// The taper at the point, or none.
std::optional<ParkTaper> taperOf(const World &world, EntityKey point) {
  for (const ParkTaper &taper : parkTapers(world)) {
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

// The segment the key holds, or none.
std::optional<ParkSegment> segmentOf(const World &world, EntityKey key) {
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key == key) {
      return segment;
    }
  }
  return std::nullopt;
}

// How many segments use the point.
size_t segmentCount(const World &world, EntityKey point) {
  return static_cast<size_t>(
      std::ranges::count_if(parkSegments(world), [point](const ParkSegment &segment) {
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

// The stop a path tool's press would append: the snapped pointer, unless it lies within
// FINISH_REACH of the last stop, where a press finishes the path instead.
std::optional<PathStop> nextStop(const ToolState &tool, const World &world) {
  if (!tool.Pointer || !tool.Snapped) {
    return std::nullopt;
  }
  const PathStop &stop = tool.Snapped->Stop;
  if (!tool.Drawn.empty()) {
    AddPath path{drawnKind(tool.Kind), tool.Drawn};
    path.Stops.push_back(stop);
    const std::optional<std::vector<ParkPoint>> at = stopPositions(world, path);
    if (at && distanceBetween(at->back(), (*at)[at->size() - 2]) <= FINISH_REACH) {
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

// A path tool's edit: the stops so far, followed by the next stop while not holding.
std::optional<ParkEdit> pathEdit(const ToolState &tool, const World &world) {
  std::vector<PathStop> stops = tool.Drawn;
  if (!tool.Holding && !stops.empty()) {
    if (const std::optional<PathStop> next = nextStop(tool, world)) {
      stops.push_back(*next);
    }
  }
  if (stops.size() < 2) {
    return std::nullopt;
  }
  return AddPath{drawnKind(tool.Kind), std::move(stops)};
}

// The pose the box the key holds has, if it holds one.
std::optional<Pose> boxPose(const World &world, EntityKey key) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == key) {
      return box.At;
    }
  }
  return std::nullopt;
}

// The InsertPoint a click at the point makes on the segment under it, or none: at the nearest
// place on its ground line, on the arc of a rounded corner at either end, or else on its straight
// part.
std::optional<InsertPoint> insertAt(const World &world, ParkPoint point) {
  const std::optional<EntityKey> key = segmentAt(world, point);
  if (!key) {
    return std::nullopt;
  }
  const std::vector<ParkCorner> corners = parkCorners(world);
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key != *key) {
      continue;
    }
    const std::optional<ParkPoint> from = stopPosition(world, OnPoint{segment.From});
    const std::optional<ParkPoint> to = stopPosition(world, OnPoint{segment.To});
    if (!from || !to) {
      return std::nullopt;
    }
    // The straight part's place first, so it wins a tie with an arc's.
    std::vector<Snap> places;
    if (const auto place = straightPlace(corners, segment, *from, *to, point)) {
      places.push_back(makeSnap(OnSegment{segment.Key, place->first}, place->second, {}, {}, {}));
    }
    for (const EntityKey end : {segment.From, segment.To}) {
      if (const ParkCorner *corner = roundedAt(corners, end)) {
        if (const std::optional<Snap> place = bendPlaceNear(*corner, point)) {
          places.push_back(*place);
        }
      }
    }
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

// The edit a dragged radius handle, edge handle, taper handle, or side makes with the pointer where
// it is: the arc's middle moved as far into the corner as the pointer has moved, the edge's middle
// moved as far along its Toward as the pointer has moved, where the longest curve starts moved as
// far along the narrower segment as the pointer has moved, or the width grown by twice how far the
// pointer has moved away from the ground line. None when the corner, edge, taper, or segment is
// gone.
std::optional<ParkEdit> handleEdit(const EditHandle &handle, double grabFrom, const World &world,
                                   ParkPoint pointer) {
  if (handle.Kind == HandleKind::Radius) {
    const std::optional<ParkCorner> corner = cornerOf(world, handle.Key);
    if (!corner) {
      return std::nullopt;
    }
    const double radius = cornerRadiusFor(*corner, depthAlong(*corner, pointer) + grabFrom);
    return SetCornerRadius{corner->Point, std::min(radius, corner->MaxRadius)};
  }
  if (handle.Kind == HandleKind::Edge) {
    const std::optional<ParkEdge> edge = edgeOf(world, handle.Key, handle.Segment);
    if (!edge) {
      return std::nullopt;
    }
    const double radius = edgeRadiusFor(*edge, depthAlong(*edge, pointer) + grabFrom);
    return SetEdgeRadius{edge->Segment, edge->Point,
                         std::clamp(radius, edge->MinRadius, edge->MaxRadius)};
  }
  if (handle.Kind == HandleKind::Taper) {
    const std::optional<ParkTaper> taper = taperOf(world, handle.Key);
    if (!taper) {
      return std::nullopt;
    }
    const double most = std::max(taper->MaxLength, MIN_TAPER_LENGTH);
    return SetTaperLength{
        taper->Point, std::clamp(depthAlong(*taper, pointer) + grabFrom, MIN_TAPER_LENGTH, most)};
  }
  const std::optional<ParkSegment> segment = segmentOf(world, handle.Key);
  if (!segment || segment->Line.size() < 2) {
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
std::optional<ParkEdit> editPathEdit(const ToolState &tool, const World &world) {
  if (tool.Holding && tool.Grabbed) {
    if (!tool.Dragged || !tool.Pointer) {
      return std::nullopt;
    }
    return handleEdit(*tool.Grabbed, tool.GrabFrom, world, *tool.Pointer);
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
  if (tool.Holding || !tool.Pointer || handleAt(world, *tool.Pointer) ||
      pointAt(world, *tool.Pointer) || sideAt(world, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<InsertPoint> insert = insertAt(world, *tool.Pointer)) {
    return *insert;
  }
  return std::nullopt;
}

// Edit path's press takes, in order, the radius, edge, or taper handle, the point, the segment's
// sides, or the insert under the pointer.
void pressEditPath(ToolState &tool, const World &world, ParkPoint pointer) {
  if (const std::optional<EditHandle> handle = handleAt(world, pointer)) {
    if (handle->Kind == HandleKind::Radius) {
      if (const std::optional<ParkCorner> corner = cornerOf(world, handle->Key)) {
        tool.Holding = true;
        tool.Grabbed = handle;
        tool.GrabFrom = distanceBetween(corner->At, corner->Middle) - depthAlong(*corner, pointer);
        return;
      }
    } else if (handle->Kind == HandleKind::Taper) {
      if (const std::optional<ParkTaper> taper = taperOf(world, handle->Key)) {
        tool.Holding = true;
        tool.Grabbed = handle;
        tool.GrabFrom = taper->Length - depthAlong(*taper, pointer);
        return;
      }
    } else if (const std::optional<ParkEdge> edge = edgeOf(world, handle->Key, handle->Segment)) {
      tool.Holding = true;
      tool.Grabbed = handle;
      tool.GrabFrom = depthAlong(*edge, edge->Middle) - depthAlong(*edge, pointer);
      return;
    }
  }
  if (const std::optional<EntityKey> point = pointAt(world, pointer)) {
    if (const std::optional<ParkPoint> at = stopPosition(world, OnPoint{*point})) {
      tool.Holding = true;
      tool.Held = *point;
      tool.PointTo = *at;
      tool.GrabOffset = ParkPoint{at->X - pointer.X, at->Z - pointer.Z};
      return;
    }
  }
  if (const std::optional<EntityKey> key = sideAt(world, pointer)) {
    if (const std::optional<ParkSegment> segment = segmentOf(world, *key)) {
      tool.Holding = true;
      tool.Grabbed = EditHandle{HandleKind::Side, *key, {}};
      tool.GrabFrom = segment->Width - 2.0 * lineDistance(pointer, segment->Line);
      return;
    }
  }
  if (const std::optional<InsertPoint> insert = insertAt(world, pointer)) {
    tool.Holding = true;
    tool.Inserting = *insert;
  }
}

// The kind of the segments at the point, or none when no segment uses it.
std::optional<PathKind> pointKind(const World &world, EntityKey point) {
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.From == point || segment.To == point) {
      return segment.Kind;
    }
  }
  return std::nullopt;
}

// Finds the snap in effect for the tool as it is now: a path tool's next stop at the pointer, or
// where Edit path's dragged point lands. Keeps what the last snap held when asked to.
void resnap(ToolState &tool, const World &world, bool keepHolds) {
  std::vector<SnapHold> held;
  if (keepHolds && tool.Snapped) {
    held = std::move(tool.Snapped->Holds);
  }
  tool.Snapped.reset();
  if (isPlaceTool(tool.Kind)) {
    // A press keeps the frontage it was made with through the hold.
    if (!tool.Holding) {
      tool.Fronted = tool.Pointer && !tool.SnapsOff
                         ? frontagePose(world, placedKind(tool.Kind), *tool.Pointer)
                         : std::nullopt;
    }
    return;
  }
  SnapQuery query;
  query.CatchReach = SNAP_CATCH_PIXELS * tool.MetersPerPixel;
  query.ReleaseReach = SNAP_RELEASE_PIXELS * tool.MetersPerPixel;
  query.Held = held;
  if (isPathTool(tool.Kind) && tool.Pointer) {
    query.Kind = drawnKind(tool.Kind);
    query.Drawn = tool.Drawn;
    query.Pointer = *tool.Pointer;
  } else if (tool.Kind == ToolKind::EditPath && tool.Holding && tool.Held != NULL_KEY &&
             tool.Dragged) {
    const std::optional<PathKind> kind = pointKind(world, tool.Held);
    if (!kind || tool.SnapsOff) {
      tool.Snapped = PointerSnap{tool.PointTo, NewPoint{tool.PointTo}, {}, {}};
      return;
    }
    query.Kind = *kind;
    query.Dragged = tool.Held;
    query.Pointer = tool.PointTo;
  } else {
    return;
  }
  if (tool.SnapsOff) {
    tool.Snapped = PointerSnap{query.Pointer, NewPoint{query.Pointer}, {}, {}};
    return;
  }
  tool.Snapped = findSnap(world, query);
}

} // namespace

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

void movePointer(ToolState &tool, const World &world, const PointerSample &pointer) {
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
  resnap(tool, world, true);
}

void pressPointer(ToolState &tool, const World &world) {
  if (tool.Holding) {
    return;
  }
  tool.Dragged = false;
  if (tool.Kind == ToolKind::Delete || tool.Kind == ToolKind::RemovePoint) {
    tool.Holding = true;
  } else if (isPathTool(tool.Kind) && tool.Pointer) {
    if (const std::optional<PathStop> next = nextStop(tool, world)) {
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
    const std::optional<EntityKey> key = boxAt(world, *tool.Pointer);
    const std::optional<Pose> pose = key ? boxPose(world, *key) : std::nullopt;
    if (pose) {
      tool.Holding = true;
      tool.Held = *key;
      tool.Target = *pose;
      tool.GrabOffset = ParkPoint{pose->X - tool.Pointer->X, pose->Z - tool.Pointer->Z};
    }
  } else if (tool.Kind == ToolKind::EditPath && tool.Pointer) {
    pressEditPath(tool, world, *tool.Pointer);
  }
  resnap(tool, world, false);
}

std::optional<ParkEdit> releasePointer(ToolState &tool, const World &world) {
  if (!tool.Holding) {
    return std::nullopt;
  }
  std::optional<ParkEdit> edit = tentativeEdit(tool, world);
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
  resnap(tool, world, false);
  return edit;
}

std::optional<ParkEdit> tentativeEdit(const ToolState &tool, const World &world) {
  if (isPathTool(tool.Kind)) {
    return pathEdit(tool, world);
  }
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
    return editPathEdit(tool, world);
  }
  if (tool.Kind == ToolKind::RemovePoint && tool.Pointer) {
    if (const std::optional<EntityKey> point = pointAt(world, *tool.Pointer)) {
      return RemovePoint{*point};
    }
    return std::nullopt;
  }
  if (tool.Kind == ToolKind::Delete && tool.Pointer) {
    if (const std::optional<EntityKey> box = boxAt(world, *tool.Pointer)) {
      return DeleteBox{*box};
    }
    if (const std::optional<EntityKey> segment = segmentAt(world, *tool.Pointer)) {
      return DeleteSegment{*segment};
    }
  }
  return std::nullopt;
}

std::optional<PathStart> pathStart(const ToolState &tool, const World &world) {
  if (!isPathTool(tool.Kind) || !tool.Drawn.empty() || !tool.Pointer) {
    return std::nullopt;
  }
  if (!tool.Snapped) {
    return std::nullopt;
  }
  const PathStop &stop = tool.Snapped->Stop;
  const std::optional<ParkPoint> at = stopPosition(world, stop);
  if (!at) {
    return std::nullopt;
  }
  return PathStart{*at, drawnKind(tool.Kind), !std::holds_alternative<NewPoint>(stop)};
}

std::optional<ParkPoint> pathFinish(const ToolState &tool, const World &world) {
  if (!isPathTool(tool.Kind) || tool.Drawn.size() < 2 || !tool.Pointer || nextStop(tool, world)) {
    return std::nullopt;
  }
  const std::optional<std::vector<ParkPoint>> at =
      stopPositions(world, AddPath{drawnKind(tool.Kind), tool.Drawn});
  return at ? std::optional<ParkPoint>(at->back()) : std::nullopt;
}

std::optional<EntityKey> highlightedEntity(const ToolState &tool, const World &world) {
  if (tool.Kind == ToolKind::MoveBox && !tool.Holding && tool.Pointer) {
    return boxAt(world, *tool.Pointer);
  }
  if (tool.Kind == ToolKind::EditPath && !tool.Holding && tool.Pointer &&
      !handleAt(world, *tool.Pointer)) {
    return pointAt(world, *tool.Pointer);
  }
  if (tool.Kind == ToolKind::RemovePoint && !tool.Holding && tool.Pointer) {
    const std::optional<EntityKey> point = pointAt(world, *tool.Pointer);
    if (point && segmentCount(world, *point) == 2) {
      return point;
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> boxAt(const World &world, ParkPoint point) {
  for (const ParkBox &box : parkBoxes(world)) {
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

std::optional<EntityKey> segmentAt(const World &world, ParkPoint point) {
  for (const ParkSegment &segment : parkSegments(world)) {
    const std::vector<CarrierPoint> &line = segment.Line;
    for (size_t i = 0; i + 1 < line.size(); ++i) {
      if (segmentDistance(point, line[i], line[i + 1]) <= 0.5 * segment.Width) {
        return segment.Key;
      }
    }
  }
  return std::nullopt;
}

std::optional<EntityKey> pointAt(const World &world, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestDistance = 0.0;
  for (const ParkPathPoint &candidate : parkPathPoints(world)) {
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

std::optional<EditHandle> handleAt(const World &world, ParkPoint point) {
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
  for (const ParkCorner &corner : parkCorners(world)) {
    consider(EditHandle{HandleKind::Radius, corner.Point, radiusHandleOf(corner)});
  }
  for (const ParkEdge &edge : parkEdges(world)) {
    consider(EditHandle{HandleKind::Edge, edge.Point, edge.Middle, edge.Segment});
  }
  for (const ParkTaper &taper : parkTapers(world)) {
    consider(EditHandle{HandleKind::Taper, taper.Point, taperHandleOf(taper), taper.Segment});
  }
  return nearest;
}

std::optional<EntityKey> sideAt(const World &world, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestOff = 0.0;
  for (const ParkSegment &segment : parkSegments(world)) {
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

std::vector<EditHandle> editHandles(const ToolState &tool, const World &world) {
  std::vector<EditHandle> handles;
  if ((tool.Kind == ToolKind::EditPath || tool.Kind == ToolKind::RemovePoint) && !tool.Holding) {
    for (const ParkPathPoint &point : parkPathPoints(world)) {
      handles.push_back(EditHandle{HandleKind::Point, point.Key, point.At});
    }
  }
  if (tool.Kind == ToolKind::EditPath && !tool.Holding) {
    for (const ParkCorner &corner : parkCorners(world)) {
      handles.push_back(EditHandle{HandleKind::Radius, corner.Point, radiusHandleOf(corner)});
    }
    for (const ParkEdge &edge : parkEdges(world)) {
      handles.push_back(EditHandle{HandleKind::Edge, edge.Point, edge.Middle, edge.Segment});
    }
    for (const ParkTaper &taper : parkTapers(world)) {
      handles.push_back(
          EditHandle{HandleKind::Taper, taper.Point, taperHandleOf(taper), taper.Segment});
    }
  }
  return handles;
}

std::optional<EditHandle> hoveredHandle(const ToolState &tool, const World &world) {
  if (tool.Kind != ToolKind::EditPath || tool.Holding || !tool.Pointer) {
    return std::nullopt;
  }
  if (const std::optional<EditHandle> handle = handleAt(world, *tool.Pointer)) {
    return handle;
  }
  if (pointAt(world, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<EntityKey> key = sideAt(world, *tool.Pointer)) {
    return EditHandle{HandleKind::Side, *key, {}};
  }
  return std::nullopt;
}

std::vector<SnapGuide> snapGuides(const ToolState &tool) {
  return tool.Snapped ? tool.Snapped->Guides : std::vector<SnapGuide>{};
}

std::vector<MeasuredLength> measuredLengths(const ToolState &tool, const World &world) {
  std::vector<MeasuredLength> lengths;
  if (!tool.Snapped) {
    return lengths;
  }
  if (isPathTool(tool.Kind) && !tool.Drawn.empty() && nextStop(tool, world)) {
    const std::optional<std::vector<ParkPoint>> at =
        stopPositions(world, AddPath{drawnKind(tool.Kind), tool.Drawn});
    if (at && !at->empty()) {
      lengths.push_back({at->back(), tool.Snapped->At});
    }
  } else if (tool.Kind == ToolKind::EditPath && tool.Held != NULL_KEY) {
    for (const ParkSegment &segment : parkSegments(world)) {
      if (segment.From != tool.Held && segment.To != tool.Held) {
        continue;
      }
      const EntityKey end = segment.From == tool.Held ? segment.To : segment.From;
      if (const std::optional<ParkPoint> at = stopPosition(world, OnPoint{end})) {
        lengths.push_back({*at, tool.Snapped->At});
      }
    }
  }
  return lengths;
}

} // namespace tpj
