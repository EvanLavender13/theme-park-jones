#include "tools/tools.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <math.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// The distance from a point to the segment between two distinct ground line points.
double segmentDistance(ParkPoint point, const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double px = point.X - from.X;
  const double pz = point.Z - from.Z;
  const double t = std::clamp((px * dx + pz * dz) / (dx * dx + dz * dz), 0.0, 1.0);
  const double ex = px - dx * t;
  const double ez = pz - dz * t;
  return sqrt(ex * ex + ez * ez);
}

// How far along the line between two distinct positions the point's nearest place on it lies, as
// a fraction from 0 at the first to 1 at the second.
double alongSegment(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::clamp(((point.X - from.X) * dx + (point.Z - from.Z) * dz) / (dx * dx + dz * dz), 0.0,
                    1.0);
}

double distanceBetween(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return sqrt(dx * dx + dz * dz);
}

// A stop a click snaps to, and how far from the click it lies.
struct Snap {
  PathStop Stop;
  double Distance = 0.0;
};

// Whether the point is the kind's: some segment uses it, and every one that does has the kind.
bool isKindsPoint(const std::vector<ParkSegment> &segments, EntityKey point, PathKind kind) {
  bool used = false;
  for (const ParkSegment &segment : segments) {
    if (segment.From == point || segment.To == point) {
      if (segment.Kind != kind) {
        return false;
      }
      used = true;
    }
  }
  return used;
}

// The nearest of the kind's points within SNAP_REACH of the click, other than the excluded one.
// Points come in ascending key order, and only a strictly nearer one replaces the nearest, so ties
// go to the lower key.
std::optional<Snap> nearestPointSnap(const std::vector<ParkPathPoint> &points,
                                     const std::vector<ParkSegment> &segments, PathKind kind,
                                     ParkPoint click, EntityKey excluded) {
  std::optional<Snap> nearest;
  for (const ParkPathPoint &candidate : points) {
    const double distance = distanceBetween(click, candidate.At);
    if (candidate.Key != excluded && distance <= SNAP_REACH &&
        (!nearest || distance < nearest->Distance) && isKindsPoint(segments, candidate.Key, kind)) {
      nearest = Snap{OnPoint{candidate.Key}, distance};
    }
  }
  return nearest;
}

// The nearest point a drawn stop makes within SNAP_REACH of the click, as a RepeatStop. It leaves
// out the last stop, which a press finishes the path on instead, and stops on existing points,
// which nearestPointSnap finds.
std::optional<Snap> nearestDrawnSnap(const World &world, PathKind kind,
                                     const std::vector<PathStop> &drawn, ParkPoint click) {
  const std::optional<std::vector<ParkPoint>> at = stopPositions(world, AddPath{kind, drawn});
  std::optional<Snap> nearest;
  for (size_t i = 0; at && i + 1 < drawn.size(); ++i) {
    const double distance = distanceBetween(click, (*at)[i]);
    const bool made = std::holds_alternative<NewPoint>(drawn[i]) ||
                      std::holds_alternative<OnSegment>(drawn[i]) ||
                      std::holds_alternative<OnDrawnSegment>(drawn[i]);
    if (made && distance <= SNAP_REACH && (!nearest || distance < nearest->Distance)) {
      nearest = Snap{RepeatStop{i}, distance};
    }
  }
  return nearest;
}

// The stop that names the point a drawn stop makes: the stop itself for an OnPoint, its earlier
// stop for a RepeatStop, and a RepeatStop of it otherwise.
PathStop pointOfDrawn(const std::vector<PathStop> &drawn, size_t stop) {
  if (std::holds_alternative<OnPoint>(drawn[stop])) {
    return drawn[stop];
  }
  if (const auto *repeat = std::get_if<RepeatStop>(&drawn[stop])) {
    return pointOfDrawn(drawn, repeat->Stop);
  }
  return RepeatStop{stop};
}

// The nearest place within SNAP_REACH of the click on a segment between two drawn stops, or that
// segment's end point when the place lies within MIN_SEGMENT_LENGTH of it.
std::optional<Snap> nearestDrawnSegmentSnap(const World &world, PathKind kind,
                                            const std::vector<PathStop> &drawn, ParkPoint click) {
  const std::optional<std::vector<ParkPoint>> at = stopPositions(world, AddPath{kind, drawn});
  std::optional<Snap> nearest;
  for (size_t k = 0; at && k + 1 < drawn.size(); ++k) {
    const ParkPoint from = (*at)[k];
    const ParkPoint to = (*at)[k + 1];
    const double length = distanceBetween(from, to);
    if (length < MIN_SEGMENT_LENGTH) {
      continue;
    }
    const double t = alongSegment(click, from, to);
    const double distance =
        distanceBetween(click, {from.X + t * (to.X - from.X), from.Z + t * (to.Z - from.Z)});
    if (distance > SNAP_REACH || (nearest && distance >= nearest->Distance)) {
      continue;
    }
    if (t * length < MIN_SEGMENT_LENGTH) {
      nearest = Snap{pointOfDrawn(drawn, k), distance};
    } else if ((1.0 - t) * length < MIN_SEGMENT_LENGTH) {
      nearest = Snap{pointOfDrawn(drawn, k + 1), distance};
    } else {
      nearest = Snap{OnDrawnSegment{k, t}, distance};
    }
  }
  return nearest;
}

// The nearest place within SNAP_REACH of the click on a segment of the kind that doesn't end at
// the excluded point, or the segment's end point when that place lies within MIN_SEGMENT_LENGTH of
// it. Ties go to the lower key.
std::optional<Snap> nearestSegmentSnap(const std::vector<ParkPathPoint> &points,
                                       const std::vector<ParkSegment> &segments, PathKind kind,
                                       ParkPoint click, EntityKey excluded) {
  const auto positionOf = [&points](EntityKey key) -> std::optional<ParkPoint> {
    const auto found = std::ranges::lower_bound(points, key, {}, &ParkPathPoint::Key);
    return found != points.end() && found->Key == key ? std::optional<ParkPoint>(found->At)
                                                      : std::nullopt;
  };
  std::optional<Snap> nearest;
  for (const ParkSegment &segment : segments) {
    const std::optional<ParkPoint> from = positionOf(segment.From);
    const std::optional<ParkPoint> to = positionOf(segment.To);
    if (segment.Kind != kind || segment.Line.empty() || !from || !to || segment.From == excluded ||
        segment.To == excluded) {
      continue;
    }
    const double t = alongSegment(click, *from, *to);
    const ParkPoint place{from->X + t * (to->X - from->X), from->Z + t * (to->Z - from->Z)};
    const double distance = distanceBetween(click, place);
    if (distance > SNAP_REACH || (nearest && distance >= nearest->Distance)) {
      continue;
    }
    const double length = distanceBetween(*from, *to);
    if (t * length < MIN_SEGMENT_LENGTH) {
      nearest = Snap{OnPoint{segment.From}, distance};
    } else if ((1.0 - t) * length < MIN_SEGMENT_LENGTH) {
      nearest = Snap{OnPoint{segment.To}, distance};
    } else {
      nearest = Snap{OnSegment{segment.Key, t}, distance};
    }
  }
  return nearest;
}

bool isPlaceTool(ToolKind kind) {
  return kind == ToolKind::PlaceShop || kind == ToolKind::PlaceDepot;
}

BoxKind placedKind(ToolKind kind) {
  return kind == ToolKind::PlaceShop ? BoxKind::Shop : BoxKind::Depot;
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
  if (!tool.Pointer) {
    return std::nullopt;
  }
  const PathStop stop = snapStop(world, drawnKind(tool.Kind), tool.Drawn, *tool.Pointer);
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

// The InsertPoint a click at the point makes on the segment under it, or none.
std::optional<InsertPoint> insertAt(const World &world, ParkPoint point) {
  const std::optional<EntityKey> key = segmentAt(world, point);
  if (!key) {
    return std::nullopt;
  }
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key != *key) {
      continue;
    }
    const std::optional<ParkPoint> from = stopPosition(world, OnPoint{segment.From});
    const std::optional<ParkPoint> to = stopPosition(world, OnPoint{segment.To});
    if (!from || !to) {
      return std::nullopt;
    }
    return InsertPoint{*key, alongSegment(point, *from, *to)};
  }
  return std::nullopt;
}

// Edit path's edit: the held point's move once it has been dragged, the held insert, or, while not
// holding, the insert a click away from any point would make.
std::optional<ParkEdit> editPathEdit(const ToolState &tool, const World &world) {
  if (tool.Holding && tool.Held != NULL_KEY) {
    if (!tool.Dragged) {
      return std::nullopt;
    }
    return MovePoint{tool.Held, snapPoint(world, tool.Held, tool.PointTo)};
  }
  if (tool.Holding && tool.Inserting) {
    return *tool.Inserting;
  }
  if (tool.Holding || !tool.Pointer || pointAt(world, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<InsertPoint> insert = insertAt(world, *tool.Pointer)) {
    return *insert;
  }
  return std::nullopt;
}

} // namespace

void selectTool(ToolState &tool, ToolKind kind) {
  tool.Kind = kind;
  tool.Holding = false;
  tool.Dragged = false;
  tool.Held = NULL_KEY;
  tool.Inserting.reset();
  tool.Drawn.clear();
}

void movePointer(ToolState &tool, std::optional<ParkPoint> ground) {
  // A hold counts as a drag only once the ground point changes, since a recomputed position can
  // differ from the one at the press by rounding alone.
  if (tool.Holding && ground && !(tool.Pointer && *tool.Pointer == *ground)) {
    tool.Dragged = true;
  }
  tool.Pointer = ground;
  if (!tool.Holding || !ground) {
    return;
  }
  if (isPlaceTool(tool.Kind)) {
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

void pressPointer(ToolState &tool, const World &world) {
  if (tool.Holding) {
    return;
  }
  tool.Dragged = false;
  if (tool.Kind == ToolKind::Delete) {
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
    tool.Landing = Pose{tool.Pointer->X, tool.Pointer->Z, tool.FacingX, tool.FacingZ};
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
    const std::optional<EntityKey> point = pointAt(world, *tool.Pointer);
    const std::optional<ParkPoint> at = point ? stopPosition(world, OnPoint{*point}) : std::nullopt;
    if (at) {
      tool.Holding = true;
      tool.Held = *point;
      tool.PointTo = *at;
      tool.GrabOffset = ParkPoint{at->X - tool.Pointer->X, at->Z - tool.Pointer->Z};
    } else if (const std::optional<InsertPoint> insert = insertAt(world, *tool.Pointer)) {
      tool.Holding = true;
      tool.Inserting = *insert;
    }
  }
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
  if (tool.Kind == ToolKind::Delete && tool.Pointer) {
    if (const std::optional<EntityKey> box = boxAt(world, *tool.Pointer)) {
      return DeleteBox{*box};
    }
    if (const std::optional<EntityKey> point = pointAt(world, *tool.Pointer)) {
      return DeletePoint{*point};
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
  const PathStop stop = snapStop(world, drawnKind(tool.Kind), tool.Drawn, *tool.Pointer);
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
  if (tool.Kind == ToolKind::EditPath && !tool.Holding && tool.Pointer) {
    return pointAt(world, *tool.Pointer);
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

PathStop snapStop(const World &world, PathKind kind, const std::vector<PathStop> &drawn,
                  ParkPoint point) {
  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  const std::vector<ParkSegment> segments = parkSegments(world);
  const std::optional<Snap> onPoint = nearestPointSnap(points, segments, kind, point, NULL_KEY);
  const std::optional<Snap> onDrawn = nearestDrawnSnap(world, kind, drawn, point);
  // An existing point wins a tie with a drawn stop's.
  if (onDrawn && (!onPoint || onDrawn->Distance < onPoint->Distance)) {
    return onDrawn->Stop;
  }
  if (onPoint) {
    return onPoint->Stop;
  }
  const std::optional<Snap> onSegment = nearestSegmentSnap(points, segments, kind, point, NULL_KEY);
  const std::optional<Snap> onDrawnSegment = nearestDrawnSegmentSnap(world, kind, drawn, point);
  // An existing segment wins a tie with a drawn one.
  if (onDrawnSegment && (!onSegment || onDrawnSegment->Distance < onSegment->Distance)) {
    return onDrawnSegment->Stop;
  }
  if (onSegment) {
    return onSegment->Stop;
  }
  return NewPoint{point};
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

PointTarget snapPoint(const World &world, EntityKey point, ParkPoint at) {
  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  const std::vector<ParkSegment> segments = parkSegments(world);
  const auto used = std::ranges::find_if(segments, [point](const ParkSegment &segment) {
    return segment.From == point || segment.To == point;
  });
  if (used == segments.end()) {
    return NewPoint{at};
  }
  std::optional<Snap> snap = nearestPointSnap(points, segments, used->Kind, at, point);
  if (!snap) {
    snap = nearestSegmentSnap(points, segments, used->Kind, at, point);
  }
  if (snap) {
    if (const auto *onPoint = std::get_if<OnPoint>(&snap->Stop)) {
      return *onPoint;
    }
    if (const auto *onSegment = std::get_if<OnSegment>(&snap->Stop)) {
      return *onSegment;
    }
  }
  return NewPoint{at};
}

} // namespace tpj
