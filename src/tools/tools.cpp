#include "tools/tools.h"

#include "sim/park/geometry.h"
#include "sim/park/surface.h"

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

// The distance from the point to the ground line, which has at least two points.
double lineDistance(ParkPoint point, const std::vector<CarrierPoint> &line) {
  double nearest = segmentDistance(point, line[0], line[1]);
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    nearest = std::min(nearest, segmentDistance(point, line[i], line[i + 1]));
  }
  return nearest;
}

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
                      std::holds_alternative<OnDrawnSegment>(drawn[i]) ||
                      std::holds_alternative<OnBend>(drawn[i]);
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

// The rounded corner at the point, or none. The corners are in ascending key order of their points.
const ParkCorner *roundedAt(const std::vector<ParkCorner> &corners, EntityKey point) {
  const auto found = std::ranges::lower_bound(corners, point, {}, &ParkCorner::Point);
  return found != corners.end() && found->Point == point && found->Radius > 0.0 ? &*found : nullptr;
}

// How far from the point the arc of its rounded corner touches each of its segments, or 0.
double tangentAt(const std::vector<ParkCorner> &corners, EntityKey point) {
  const ParkCorner *corner = roundedAt(corners, point);
  return corner == nullptr ? 0.0 : distanceBetween(corner->At, arcOf(*corner).Touch[0]);
}

// The nearest place on the straight part of the segment between two distinct positions, which runs
// between where the arcs at its ends touch it: the fraction of the way from the first position to
// the second, and its distance from the point. None when the straight part is shorter than
// MIN_SEGMENT_LENGTH.
std::optional<std::pair<double, double>> straightPlace(const std::vector<ParkCorner> &corners,
                                                       const ParkSegment &segment, ParkPoint from,
                                                       ParkPoint to, ParkPoint point) {
  const double length = distanceBetween(from, to);
  const double t0 = tangentAt(corners, segment.From);
  const double t1 = tangentAt(corners, segment.To);
  const double straight = length - t0 - t1;
  if (straight < MIN_SEGMENT_LENGTH) {
    return std::nullopt;
  }
  const ParkPoint along{(to.X - from.X) / length, (to.Z - from.Z) / length};
  const ParkPoint s0{from.X + along.X * t0, from.Z + along.Z * t0};
  const ParkPoint s1{to.X - along.X * t1, to.Z - along.Z * t1};
  const double t = alongSegment(point, s0, s1);
  const ParkPoint place{s0.X + t * (s1.X - s0.X), s0.Z + t * (s1.Z - s0.Z)};
  return std::pair{(t0 + t * straight) / length, distanceBetween(point, place)};
}

// The nearest place within SNAP_REACH of the click on the straight part of a segment of the kind
// that doesn't end at the excluded point, or the segment's end point when that place lies within
// MIN_SEGMENT_LENGTH of it. Ties go to the lower key.
std::optional<Snap> nearestSegmentSnap(const std::vector<ParkPathPoint> &points,
                                       const std::vector<ParkSegment> &segments,
                                       const std::vector<ParkCorner> &corners, PathKind kind,
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
    const auto place = straightPlace(corners, segment, *from, *to, click);
    if (!place) {
      continue;
    }
    const auto [t, distance] = *place;
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

// The place on the corner's arc nearest the point, as an OnBend with its distance, or none when it
// lies at an end of the arc.
std::optional<Snap> bendPlaceNear(const ParkCorner &corner, ParkPoint point) {
  const double along = bendAlong(corner, point);
  if (!(along > 0.0) || !(along < 1.0)) {
    return std::nullopt;
  }
  return Snap{OnBend{corner.Point, along}, distanceBetween(point, bendPlace(corner, along))};
}

// The nearest place within SNAP_REACH of the click on the arc of a rounded corner whose two
// segments have the kind and neither ends at the excluded point. Ties go to the lower point key.
std::optional<Snap> nearestBendSnap(const std::vector<ParkSegment> &segments,
                                    const std::vector<ParkCorner> &corners, PathKind kind,
                                    ParkPoint click, EntityKey excluded) {
  std::optional<Snap> nearest;
  for (const ParkCorner &corner : corners) {
    if (!(corner.Radius > 0.0) || corner.Point == excluded) {
      continue;
    }
    const bool fits = std::ranges::all_of(segments, [&](const ParkSegment &segment) {
      const bool atCorner = segment.From == corner.Point || segment.To == corner.Point;
      return !atCorner ||
             (segment.Kind == kind && segment.From != excluded && segment.To != excluded);
    });
    if (!fits) {
      continue;
    }
    const std::optional<Snap> place = bendPlaceNear(corner, click);
    if (place && place->Distance <= SNAP_REACH &&
        (!nearest || place->Distance < nearest->Distance)) {
      nearest = place;
    }
  }
  return nearest;
}

// The nearer of a segment snap and a bend snap, the segment winning a tie.
std::optional<Snap> nearerSnap(std::optional<Snap> segment, std::optional<Snap> bend) {
  return bend && (!segment || bend->Distance < segment->Distance) ? bend : segment;
}

// How many segments use the point.
size_t segmentCount(const World &world, EntityKey point) {
  return static_cast<size_t>(
      std::ranges::count_if(parkSegments(world), [point](const ParkSegment &segment) {
        return segment.From == point || segment.To == point;
      }));
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
      places.push_back(Snap{OnSegment{segment.Key, place->first}, place->second});
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

// The edit a dragged radius handle, edge handle, or side makes with the pointer where it is: the
// arc's middle moved as far into the corner as the pointer has moved, the edge's middle moved as
// far along its Toward as the pointer has moved, or the width grown by twice how far the pointer
// has moved away from the ground line. None when the corner, edge, or segment is gone.
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
  const std::optional<ParkSegment> segment = segmentOf(world, handle.Key);
  if (!segment || segment->Line.size() < 2) {
    return std::nullopt;
  }
  const double width = std::clamp(2.0 * lineDistance(pointer, segment->Line) + grabFrom,
                                  MIN_PATH_WIDTH, MAX_PATH_WIDTH);
  return SetSegmentWidth{segment->Key, width};
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
    return MovePoint{tool.Held, snapPoint(world, tool.Held, tool.PointTo)};
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

// Edit path's press takes, in order, the radius or edge handle, the point, the segment's sides, or
// the insert under the pointer.
void pressEditPath(ToolState &tool, const World &world, ParkPoint pointer) {
  if (const std::optional<EditHandle> handle = handleAt(world, pointer)) {
    if (handle->Kind == HandleKind::Radius) {
      if (const std::optional<ParkCorner> corner = cornerOf(world, handle->Key)) {
        tool.Holding = true;
        tool.Grabbed = handle;
        tool.GrabFrom = distanceBetween(corner->At, corner->Middle) - depthAlong(*corner, pointer);
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

} // namespace

void selectTool(ToolState &tool, ToolKind kind) {
  tool.Kind = kind;
  tool.Holding = false;
  tool.Dragged = false;
  tool.Held = NULL_KEY;
  tool.Inserting.reset();
  tool.Grabbed.reset();
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
    pressEditPath(tool, world, *tool.Pointer);
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
  tool.Grabbed.reset();
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
  const std::vector<ParkCorner> corners = parkCorners(world);
  const std::optional<Snap> onSegment =
      nearerSnap(nearestSegmentSnap(points, segments, corners, kind, point, NULL_KEY),
                 nearestBendSnap(segments, corners, kind, point, NULL_KEY));
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
    const std::vector<ParkCorner> corners = parkCorners(world);
    snap = nearerSnap(nearestSegmentSnap(points, segments, corners, used->Kind, at, point),
                      nearestBendSnap(segments, corners, used->Kind, at, point));
  }
  if (snap) {
    if (const auto *onPoint = std::get_if<OnPoint>(&snap->Stop)) {
      return *onPoint;
    }
    if (const auto *onSegment = std::get_if<OnSegment>(&snap->Stop)) {
      return *onSegment;
    }
    if (const auto *onBend = std::get_if<OnBend>(&snap->Stop)) {
      return *onBend;
    }
  }
  return NewPoint{at};
}

ParkPoint radiusHandleOf(const ParkCorner &corner) {
  const double depth = std::max(distanceBetween(corner.At, corner.Middle), RADIUS_HANDLE_DEPTH);
  return {corner.At.X + corner.Toward.X * depth, corner.At.Z + corner.Toward.Z * depth};
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

} // namespace tpj
