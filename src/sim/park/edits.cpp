#include "sim/park/edits.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"
#include "sim/park/internal/meetings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

constexpr double HALF_PARK = PARK_SIZE / 2.0;

// A footprint the check compares, with the pose and size it came from.
struct Solid {
  EntityKey Key = NULL_KEY;
  Pose At;
  FootprintSize Size;
  Footprint Shape;
};

// A path's ground line, with half the path's width.
struct Line {
  std::vector<CarrierPoint> Points;
  double HalfWidth = 0.0;
};

std::optional<Solid> solidOf(EntityKey key, const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> shape = footprintOf(pose, size);
  if (!shape) {
    return std::nullopt;
  }
  return Solid{key, pose, size, *shape};
}

// The segment's ground line with half its width, or none when the line is empty.
std::optional<Line> lineOf(const ParkSegment &segment) {
  if (segment.Line.empty()) {
    return std::nullopt;
  }
  return Line{segment.Line, segment.Width / 2.0};
}

// Every entrance's and box's footprint, skipping any with none.
std::vector<Solid> solidsOf(const World &world) {
  std::vector<Solid> solids;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (const std::optional<Solid> solid = solidOf(entrance.Key, entrance.At, ENTRANCE_SIZE)) {
      solids.push_back(*solid);
    }
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<Solid> solid = solidOf(box.Key, box.At, boxSize(box.Kind))) {
      solids.push_back(*solid);
    }
  }
  return solids;
}

// Every segment's ground line, skipping any that is empty.
std::vector<Line> linesOf(const World &world) {
  std::vector<Line> lines;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (std::optional<Line> line = lineOf(segment)) {
      lines.push_back(std::move(*line));
    }
  }
  return lines;
}

// The intent T the key holds, or null.
template <typename T> const T *findIntent(const World &world, EntityKey key) {
  const entt::entity entity = world.findEntity(key);
  return entity == entt::null ? nullptr : world.Registry.try_get<T>(entity);
}

// Whether the kind is one of its enum's values.
template <typename Kind> bool isKnown(Kind kind) {
  return static_cast<size_t>(kind) < enumNames(kind).size();
}

bool leavesPark(const Solid &solid) {
  constexpr double LIMIT = HALF_PARK + CONTACT_TOLERANCE;
  return std::ranges::any_of(solid.Shape.Corners, [](const ParkPoint &corner) {
    return std::abs(corner.X) > LIMIT || std::abs(corner.Z) > LIMIT;
  });
}

bool leavesPark(const Line &line) {
  const double limit = (HALF_PARK - line.HalfWidth) + CONTACT_TOLERANCE;
  return std::ranges::any_of(line.Points, [limit](const CarrierPoint &point) {
    return std::abs(point.X) > limit || std::abs(point.Z) > limit;
  });
}

// The least and greatest projections of the footprint's corners onto the axis.
std::pair<double, double> extentAlong(const Footprint &footprint, const ParkPoint &axis) {
  const auto project = [&axis](const ParkPoint &corner) {
    return corner.X * axis.X + corner.Z * axis.Z;
  };
  double least = project(footprint.Corners[0]);
  double greatest = least;
  for (const ParkPoint &corner : footprint.Corners) {
    least = std::min(least, project(corner));
    greatest = std::max(greatest, project(corner));
  }
  return {least, greatest};
}

// Whether the two footprints' extents along the axis share more than CONTACT_TOLERANCE.
bool overlapAlong(const Footprint &a, const Footprint &b, const ParkPoint &axis) {
  const auto [aLeast, aGreatest] = extentAlong(a, axis);
  const auto [bLeast, bGreatest] = extentAlong(b, axis);
  return std::min(aGreatest, bGreatest) - std::max(aLeast, bLeast) > CONTACT_TOLERANCE;
}

// The separating axis test: no axis of either footprint separates them.
bool overlaps(const Solid &a, const Solid &b) {
  return overlapAlong(a.Shape, b.Shape, a.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, a.Shape.Right) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Forward) &&
         overlapAlong(a.Shape, b.Shape, b.Shape.Right);
}

// The point in the solid's frame: X along its Forward and Z along its Right, from its position.
ParkPoint toLocal(const Solid &solid, const CarrierPoint &point) {
  const double dx = point.X - solid.At.X;
  const double dz = point.Z - solid.At.Z;
  return {dx * solid.Shape.Forward.X + dz * solid.Shape.Forward.Z,
          dx * solid.Shape.Right.X + dz * solid.Shape.Right.Z};
}

// Whether the segment from a to b enters the box |x| <= halfX, |z| <= halfZ: Liang and Barsky's
// clip against both slabs.
bool entersBox(const ParkPoint &a, const ParkPoint &b, double halfX, double halfZ) {
  double enter = 0.0;
  double leave = 1.0;
  const auto clip = [&enter, &leave](double start, double delta, double half) {
    if (delta == 0.0) {
      return std::abs(start) <= half;
    }
    double low = (-half - start) / delta;
    double high = (half - start) / delta;
    if (low > high) {
      std::swap(low, high);
    }
    enter = std::max(enter, low);
    leave = std::min(leave, high);
    return enter <= leave;
  };
  return clip(a.X, b.X - a.X, halfX) && clip(a.Z, b.Z - a.Z, halfZ);
}

double squaredDistanceToBox(const ParkPoint &point, double halfX, double halfZ) {
  const double dx = std::max(std::abs(point.X) - halfX, 0.0);
  const double dz = std::max(std::abs(point.Z) - halfZ, 0.0);
  return dx * dx + dz * dz;
}

double squaredDistanceToSegment(const ParkPoint &point, const ParkPoint &a, const ParkPoint &b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double length = dx * dx + dz * dz;
  double t = 0.0;
  if (length > 0.0) {
    t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / length, 0.0, 1.0);
  }
  const double ex = a.X + dx * t - point.X;
  const double ez = a.Z + dz * t - point.Z;
  return ex * ex + ez * ez;
}

// Whether some segment of the line lies nearer the solid's rectangle than its half width less
// CONTACT_TOLERANCE. A segment that enters the rectangle lies at distance zero; otherwise the
// least distance is from an end to the rectangle or from a corner to the segment.
bool meets(const Line &line, const Solid &solid) {
  const double halfDepth = solid.Size.Depth / 2.0;
  const double halfWidth = solid.Size.Width / 2.0;
  const double reach = line.HalfWidth - CONTACT_TOLERANCE;
  const std::array<ParkPoint, 4> corners{
      ParkPoint{halfDepth, -halfWidth}, ParkPoint{halfDepth, halfWidth},
      ParkPoint{-halfDepth, halfWidth}, ParkPoint{-halfDepth, -halfWidth}};
  for (size_t i = 1; i < line.Points.size(); ++i) {
    const ParkPoint a = toLocal(solid, line.Points[i - 1]);
    const ParkPoint b = toLocal(solid, line.Points[i]);
    if (entersBox(a, b, halfDepth, halfWidth)) {
      return true;
    }
    double nearest = std::min(squaredDistanceToBox(a, halfDepth, halfWidth),
                              squaredDistanceToBox(b, halfDepth, halfWidth));
    for (const ParkPoint &corner : corners) {
      nearest = std::min(nearest, squaredDistanceToSegment(corner, a, b));
    }
    if (nearest < reach * reach) {
      return true;
    }
  }
  return false;
}

// Whether the solid stays in the park and conflicts with no line and no other solid. A solid
// holding the same key is the one being moved, so it is skipped.
bool fits(const Solid &solid, const std::vector<Solid> &solids, const std::vector<Line> &lines) {
  if (leavesPark(solid)) {
    return false;
  }
  if (std::ranges::any_of(solids, [&solid](const Solid &other) {
        return other.Key != solid.Key && overlaps(solid, other);
      })) {
    return false;
  }
  return std::ranges::none_of(lines, [&solid](const Line &line) { return meets(line, solid); });
}

// Whether the line stays in the park and meets no solid.
bool fits(const Line &line, const std::vector<Solid> &solids) {
  return !leavesPark(line) &&
         std::ranges::none_of(solids, [&line](const Solid &solid) { return meets(line, solid); });
}

// A point an AddPath's segments run through: an existing path point, or, when Existing is
// NULL_KEY, a point the path adds.
struct PlanPoint {
  ParkPoint At;
  EntityKey Existing = NULL_KEY;
};

// A plan point on a segment, Along of the way from the segment's first end to its second.
struct PlacedPoint {
  size_t Point = 0;
  double Along = 0.0;
};

// A segment of the world that an AddPath splits, and the plan points it splits at.
struct Split {
  EntityKey Segment = NULL_KEY;
  std::vector<PlacedPoint> At;
};

// What an accepted AddPath makes. Points holds each stop's point in stop order, a RepeatStop
// sharing its earlier stop's, then each point a meeting adds, in the order found. Stops gives each
// stop's entry in Points. Splits holds the world's segments the path splits, in ascending key
// order. Drawn holds, for each of the path's own segments from stop i to stop i + 1, the points it
// runs through between them. Both list their points in order of Along. Crossings holds the entries
// in Points where a meeting splits a segment, in the order found.
struct PathPlan {
  std::vector<PlanPoint> Points;
  std::vector<size_t> Stops;
  std::vector<Split> Splits;
  std::vector<std::vector<PlacedPoint>> Drawn;
  std::vector<size_t> Crossings;
};

// Whether the key holds a path point that some segment uses, every such segment of the kind.
bool isPointOfKind(const World &world, const std::vector<ParkSegment> &segments, EntityKey point,
                   PathKind kind) {
  if (findIntent<PathPointIntent>(world, point) == nullptr) {
    return false;
  }
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

// The segment the key holds, or null. The segments are in ascending key order.
const ParkSegment *findSegment(const std::vector<ParkSegment> &segments, EntityKey key) {
  const auto found = std::ranges::lower_bound(segments, key, {}, &ParkSegment::Key);
  return found != segments.end() && found->Key == key ? &*found : nullptr;
}

// Whether the stop names what it lands on, of the kind.
bool isStopOfKind(const World &world, const std::vector<ParkSegment> &segments,
                  const PathStop &stop, PathKind kind) {
  if (const auto *on = std::get_if<OnPoint>(&stop)) {
    return isPointOfKind(world, segments, on->Point, kind);
  }
  if (const auto *on = std::get_if<OnSegment>(&stop)) {
    const ParkSegment *segment = findSegment(segments, on->Segment);
    return std::isfinite(on->Along) && on->Along > 0.0 && on->Along < 1.0 && segment != nullptr &&
           segment->Kind == kind && !segment->Line.empty();
  }
  if (const auto *on = std::get_if<OnDrawnSegment>(&stop)) {
    return std::isfinite(on->Along) && on->Along > 0.0 && on->Along < 1.0;
  }
  return true;
}

// The entry in the plan's points for the existing path point, added at its position if there is
// none.
size_t pointEntry(PathPlan &plan, EntityKey key, ParkPoint at) {
  const auto found = std::ranges::find(plan.Points, key, &PlanPoint::Existing);
  if (found != plan.Points.end()) {
    return static_cast<size_t>(found - plan.Points.begin());
  }
  plan.Points.push_back(PlanPoint{at, key});
  return plan.Points.size() - 1;
}

// Places the point on a segment's list, unless the list holds it already. Whether it placed it.
bool place(std::vector<PlacedPoint> &on, size_t point, double along) {
  if (std::ranges::find(on, point, &PlacedPoint::Point) != on.end()) {
    return false;
  }
  on.push_back(PlacedPoint{point, along});
  return true;
}

// The points the world's segment splits at, starting none.
std::vector<PlacedPoint> &splitOf(PathPlan &plan, EntityKey segment) {
  const auto found = std::ranges::find(plan.Splits, segment, &Split::Segment);
  if (found != plan.Splits.end()) {
    return found->At;
  }
  plan.Splits.push_back(Split{segment, {}});
  return plan.Splits.back().At;
}

void markCrossing(PathPlan &plan, size_t point) {
  if (std::ranges::find(plan.Crossings, point) == plan.Crossings.end()) {
    plan.Crossings.push_back(point);
  }
}

// The point for a crossing: a point the plan already has within MIN_SEGMENT_LENGTH of it, as where
// a path doubles back across a segment, or else a new one.
size_t crossingPoint(PathPlan &plan, ParkPoint at) {
  const auto near = std::ranges::find_if(plan.Points, [at](const PlanPoint &point) {
    const double dx = point.At.X - at.X;
    const double dz = point.At.Z - at.Z;
    return dx * dx + dz * dz <= MIN_SEGMENT_LENGTH * MIN_SEGMENT_LENGTH;
  });
  if (near != plan.Points.end()) {
    return static_cast<size_t>(near - plan.Points.begin());
  }
  plan.Points.push_back(PlanPoint{at, NULL_KEY});
  return plan.Points.size() - 1;
}

// The plan's stop points, and the points of OnSegment and OnDrawnSegment stops on the segments
// they name.
PathPlan planStops(const AddPath &command, const std::vector<ParkPoint> &positions) {
  PathPlan plan;
  plan.Drawn.resize(command.Stops.size() - 1);
  for (size_t i = 0; i < command.Stops.size(); ++i) {
    const PathStop &stop = command.Stops[i];
    if (const auto *on = std::get_if<OnPoint>(&stop)) {
      plan.Stops.push_back(pointEntry(plan, on->Point, positions[i]));
    } else if (const auto *repeat = std::get_if<RepeatStop>(&stop)) {
      plan.Stops.push_back(plan.Stops[repeat->Stop]);
    } else {
      plan.Points.push_back(PlanPoint{positions[i], NULL_KEY});
      plan.Stops.push_back(plan.Points.size() - 1);
    }
    if (const auto *onSegment = std::get_if<OnSegment>(&stop)) {
      place(splitOf(plan, onSegment->Segment), plan.Stops[i], onSegment->Along);
    } else if (const auto *onDrawn = std::get_if<OnDrawnSegment>(&stop)) {
      place(plan.Drawn[onDrawn->Stop], plan.Stops[i], onDrawn->Along);
    }
  }
  return plan;
}

// The ends of the path's own segment from stop k to stop k + 1.
std::pair<ParkPoint, ParkPoint> drawnEnds(const PathPlan &plan, size_t k) {
  return {plan.Points[plan.Stops[k]].At, plan.Points[plan.Stops[k + 1]].At};
}

// Adds the point where the path's own segment k meets the world's segment, if they meet.
void meetWorldSegment(PathPlan &plan, size_t k, const ParkSegment &segment) {
  const auto [a0, a1] = drawnEnds(plan, k);
  const ParkPoint b0{segment.Line.front().X, segment.Line.front().Z};
  const ParkPoint b1{segment.Line.back().X, segment.Line.back().Z};
  const std::optional<Meeting> meeting = meetingOf(a0, a1, b0, b1);
  if (!meeting) {
    return;
  }
  size_t point = 0;
  bool placed = false;
  if (meeting->Kind == MeetingKind::AEndOnB) {
    point = plan.Stops[k + meeting->End];
    placed = place(splitOf(plan, segment.Key), point, meeting->AlongB);
  } else if (meeting->Kind == MeetingKind::BEndOnA) {
    point = pointEntry(plan, meeting->End == 0 ? segment.From : segment.To, meeting->At);
    placed = place(plan.Drawn[k], point, meeting->AlongA);
  } else {
    point = crossingPoint(plan, meeting->At);
    placed = place(plan.Drawn[k], point, meeting->AlongA);
    placed = place(splitOf(plan, segment.Key), point, meeting->AlongB) || placed;
  }
  if (placed) {
    markCrossing(plan, point);
  }
}

// Adds the point where the path's own earlier segment m meets its own segment k, if they meet.
void meetDrawnSegment(PathPlan &plan, size_t m, size_t k) {
  const auto [a0, a1] = drawnEnds(plan, m);
  const auto [b0, b1] = drawnEnds(plan, k);
  const std::optional<Meeting> meeting = meetingOf(a0, a1, b0, b1);
  if (!meeting) {
    return;
  }
  size_t point = 0;
  bool placed = false;
  if (meeting->Kind == MeetingKind::AEndOnB) {
    point = plan.Stops[m + meeting->End];
    placed = place(plan.Drawn[k], point, meeting->AlongB);
  } else if (meeting->Kind == MeetingKind::BEndOnA) {
    point = plan.Stops[k + meeting->End];
    placed = place(plan.Drawn[m], point, meeting->AlongA);
  } else {
    point = crossingPoint(plan, meeting->At);
    placed = place(plan.Drawn[m], point, meeting->AlongA);
    placed = place(plan.Drawn[k], point, meeting->AlongB) || placed;
  }
  if (placed) {
    markCrossing(plan, point);
  }
}

// Adds the points where each of the path's own segments meets a world segment of the kind, in
// ascending key order, and then each of its own earlier segments.
void addMeetings(PathPlan &plan, const std::vector<ParkSegment> &segments, PathKind kind) {
  for (size_t k = 0; k < plan.Drawn.size(); ++k) {
    for (const ParkSegment &segment : segments) {
      if (segment.Kind == kind && !segment.Line.empty()) {
        meetWorldSegment(plan, k, segment);
      }
    }
    for (size_t m = 0; m < k; ++m) {
      meetDrawnSegment(plan, m, k);
    }
  }
}

// Whether each part from the first position through the placed points to the last has a ground
// line and fits the solids, at the width.
bool runFits(const PathPlan &plan, ParkPoint first, const std::vector<PlacedPoint> &through,
             ParkPoint last, double width, const std::vector<Solid> &solids) {
  ParkPoint from = first;
  const auto fitsTo = [&](ParkPoint to) {
    std::vector<CarrierPoint> points = segmentLine(from, to);
    from = to;
    return !points.empty() && fits(Line{std::move(points), width / 2.0}, solids);
  };
  return std::ranges::all_of(
             through,
             [&](const PlacedPoint &placed) { return fitsTo(plan.Points[placed.Point].At); }) &&
         fitsTo(last);
}

// Whether every part of every segment the path splits or adds has a ground line and fits the
// solids. A part bends at most MIN_SEGMENT_LENGTH from its segment's line where an end lies on it.
bool partsFit(const World &world, const PathPlan &plan, const std::vector<ParkSegment> &segments,
              const std::vector<Solid> &solids, PathKind kind) {
  for (const Split &split : plan.Splits) {
    // A split segment has a ground line, so both of its ends are path points.
    const ParkSegment &segment = *findSegment(segments, split.Segment);
    if (!runFits(plan, findIntent<PathPointIntent>(world, segment.From)->At, split.At,
                 findIntent<PathPointIntent>(world, segment.To)->At, segment.Width, solids)) {
      return false;
    }
  }
  for (size_t k = 0; k < plan.Drawn.size(); ++k) {
    const auto [from, to] = drawnEnds(plan, k);
    if (!runFits(plan, from, plan.Drawn[k], to, pathWidth(kind), solids)) {
      return false;
    }
  }
  return true;
}

// The plan of an AddPath the world accepts, or none when it refuses it.
std::optional<PathPlan> planPath(const World &world, const AddPath &command) {
  if (!isKnown(command.Kind) || command.Stops.size() < 2) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, command);
  if (!positions || !std::ranges::all_of(command.Stops, [&](const PathStop &stop) {
        return isStopOfKind(world, segments, stop, command.Kind);
      })) {
    return std::nullopt;
  }
  PathPlan plan = planStops(command, *positions);
  for (size_t k = 0; k < plan.Drawn.size(); ++k) {
    const auto [from, to] = drawnEnds(plan, k);
    if (segmentLine(from, to).empty()) {
      return std::nullopt;
    }
  }
  addMeetings(plan, segments, command.Kind);
  for (Split &split : plan.Splits) {
    std::ranges::sort(split.At, {}, &PlacedPoint::Along);
  }
  for (std::vector<PlacedPoint> &drawn : plan.Drawn) {
    std::ranges::sort(drawn, {}, &PlacedPoint::Along);
  }
  std::ranges::sort(plan.Splits, {}, &Split::Segment);
  if (!partsFit(world, plan, segments, solidsOf(world), command.Kind)) {
    return std::nullopt;
  }
  return plan;
}

} // namespace

void addParkEdits(WorldSchema &schema) {
  schema.addCommand<AddPath>();
  schema.addCommand<AddBox>();
  schema.addCommand<MoveBox>();
  schema.addCommand<DeleteSegment>();
  schema.addCommand<DeleteBox>();
}

std::optional<ParkPoint> stopPosition(const World &world, const PathStop &stop) {
  if (const auto *at = std::get_if<NewPoint>(&stop)) {
    return at->At;
  }
  if (const auto *on = std::get_if<OnPoint>(&stop)) {
    const PathPointIntent *point = findIntent<PathPointIntent>(world, on->Point);
    return point == nullptr ? std::nullopt : std::optional<ParkPoint>(point->At);
  }
  if (std::holds_alternative<RepeatStop>(stop) || std::holds_alternative<OnDrawnSegment>(stop)) {
    return std::nullopt;
  }
  const auto &on = std::get<OnSegment>(stop);
  const PathSegmentIntent *segment = findIntent<PathSegmentIntent>(world, on.Segment);
  if (segment == nullptr) {
    return std::nullopt;
  }
  const PathPointIntent *from = findIntent<PathPointIntent>(world, segment->From);
  const PathPointIntent *to = findIntent<PathPointIntent>(world, segment->To);
  if (from == nullptr || to == nullptr) {
    return std::nullopt;
  }
  return ParkPoint{from->At.X + on.Along * (to->At.X - from->At.X),
                   from->At.Z + on.Along * (to->At.Z - from->At.Z)};
}

std::optional<std::vector<ParkPoint>> stopPositions(const World &world, const AddPath &path) {
  std::vector<ParkPoint> positions;
  for (const PathStop &stop : path.Stops) {
    if (const auto *repeat = std::get_if<RepeatStop>(&stop)) {
      if (repeat->Stop >= positions.size()) {
        return std::nullopt;
      }
      positions.push_back(positions[repeat->Stop]);
      continue;
    }
    if (const auto *on = std::get_if<OnDrawnSegment>(&stop)) {
      if (on->Stop + 1 >= positions.size()) {
        return std::nullopt;
      }
      const ParkPoint &from = positions[on->Stop];
      const ParkPoint &to = positions[on->Stop + 1];
      positions.push_back(
          ParkPoint{from.X + on->Along * (to.X - from.X), from.Z + on->Along * (to.Z - from.Z)});
      continue;
    }
    const std::optional<ParkPoint> position = stopPosition(world, stop);
    if (!position) {
      return std::nullopt;
    }
    positions.push_back(*position);
  }
  return positions;
}

std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path) {
  const std::optional<PathPlan> plan = planPath(world, path);
  std::vector<ParkPoint> crossings;
  if (plan) {
    for (const size_t point : plan->Crossings) {
      crossings.push_back(plan->Points[point].At);
    }
  }
  return crossings;
}

bool isAccepted(const World &world, const AddPath &command) {
  return planPath(world, command).has_value();
}

bool isAccepted(const World &world, const AddBox &command) {
  if (!isKnown(command.Kind)) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(NULL_KEY, command.At, boxSize(command.Kind));
  return solid && fits(*solid, solidsOf(world), linesOf(world));
}

bool isAccepted(const World &world, const MoveBox &command) {
  const BoxIntent *box = findIntent<BoxIntent>(world, command.Box);
  if (box == nullptr) {
    return false;
  }
  const std::optional<Solid> solid = solidOf(command.Box, command.At, boxSize(box->Kind));
  return solid && fits(*solid, solidsOf(world), linesOf(world));
}

bool isAccepted(const World &world, const DeleteSegment &command) {
  return findIntent<PathSegmentIntent>(world, command.Segment) != nullptr;
}

bool isAccepted(const World &world, const DeleteBox &command) {
  return findIntent<BoxIntent>(world, command.Box) != nullptr;
}

void applyCommand(World &world, const AddPath &command) {
  const std::optional<PathPlan> plan = planPath(world, command);
  if (!plan) {
    return;
  }
  const auto addSegment = [&world](PathKind kind, EntityKey from, EntityKey to, double width) {
    const EntityKey key = world.createEntity();
    world.Registry.emplace<PathSegmentIntent>(world.findEntity(key),
                                              PathSegmentIntent{kind, from, to, width});
  };
  // Each plan point's key: its existing point's, or a new point's, in the plan's order.
  std::vector<EntityKey> keys;
  for (const PlanPoint &point : plan->Points) {
    if (point.Existing != NULL_KEY) {
      keys.push_back(point.Existing);
    } else {
      const EntityKey key = world.createEntity();
      world.Registry.emplace<PathPointIntent>(world.findEntity(key), PathPointIntent{point.At});
      keys.push_back(key);
    }
  }
  // Each split segment ends at its first point, and new parts run on through the rest to its old
  // end.
  for (const Split &split : plan->Splits) {
    PathSegmentIntent &segment =
        world.Registry.get<PathSegmentIntent>(world.findEntity(split.Segment));
    const PathSegmentIntent was = segment;
    segment.To = keys[split.At.front().Point];
    for (size_t k = 0; k < split.At.size(); ++k) {
      const EntityKey to = k + 1 < split.At.size() ? keys[split.At[k + 1].Point] : was.To;
      addSegment(was.Kind, keys[split.At[k].Point], to, was.Width);
    }
  }
  // Each of the path's own segments runs through the points on it.
  for (size_t i = 0; i < plan->Drawn.size(); ++i) {
    EntityKey from = keys[plan->Stops[i]];
    for (const PlacedPoint &placed : plan->Drawn[i]) {
      addSegment(command.Kind, from, keys[placed.Point], pathWidth(command.Kind));
      from = keys[placed.Point];
    }
    addSegment(command.Kind, from, keys[plan->Stops[i + 1]], pathWidth(command.Kind));
  }
}

void applyCommand(World &world, const AddBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const EntityKey key = world.createEntity();
  world.Registry.emplace<BoxIntent>(world.findEntity(key), BoxIntent{command.Kind, command.At});
}

void applyCommand(World &world, const MoveBox &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  world.Registry.get<BoxIntent>(world.findEntity(command.Box)).At = command.At;
}

void applyCommand(World &world, const DeleteSegment &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const PathSegmentIntent segment = *findIntent<PathSegmentIntent>(world, command.Segment);
  world.destroyEntity(command.Segment);
  // A point the segment leaves with no segment goes with it.
  const auto used = [&world](EntityKey point) {
    return std::ranges::any_of(world.keys(), [&world, point](EntityKey key) {
      const PathSegmentIntent *other = findIntent<PathSegmentIntent>(world, key);
      return other != nullptr && (other->From == point || other->To == point);
    });
  };
  for (const EntityKey point : {segment.From, segment.To}) {
    if (findIntent<PathPointIntent>(world, point) != nullptr && !used(point)) {
      world.destroyEntity(point);
    }
  }
}

void applyCommand(World &world, const DeleteBox &command) {
  if (isAccepted(world, command)) {
    world.destroyEntity(command.Box);
  }
}

bool isPhysicallyValid(const World &world) {
  const std::vector<Solid> solids = solidsOf(world);
  const std::vector<Line> lines = linesOf(world);
  // An entrance or box with no footprint, or a segment with an empty ground line, is not valid.
  if (solids.size() != parkEntrances(world).size() + parkBoxes(world).size() ||
      lines.size() != parkSegments(world).size()) {
    return false;
  }
  for (size_t i = 0; i < solids.size(); ++i) {
    if (leavesPark(solids[i])) {
      return false;
    }
    for (size_t j = i + 1; j < solids.size(); ++j) {
      if (overlaps(solids[i], solids[j])) {
        return false;
      }
    }
  }
  return std::ranges::all_of(lines, [&solids](const Line &line) { return fits(line, solids); });
}

bool isAccepted(const World &world, const ParkEdit &edit) {
  return std::visit([&world](const auto &command) { return isAccepted(world, command); }, edit);
}

void queueEdit(CommandQueue &queue, const ParkEdit &edit) {
  std::visit([&queue](const auto &command) { queue.push(command); }, edit);
}

} // namespace tpj
