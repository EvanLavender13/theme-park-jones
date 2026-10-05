#include "sim/park/edits.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/internal/components.h"

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

// A segment an AddPath splits: its key, and the indices of the stops on it in order of Along.
struct Split {
  EntityKey Segment = NULL_KEY;
  std::vector<size_t> Stops;
};

// What an accepted AddPath makes: each stop's position, the segments it splits in ascending key
// order, and, for each of its own segments, the indices of the later stops on it in order of
// Along.
struct PathPlan {
  std::vector<ParkPoint> Positions;
  std::vector<Split> Splits;
  std::vector<std::vector<size_t>> DrawnSplits;
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

// For each of the path's own segments, the stops on it in order of Along, or none when a part it
// splits into has no ground line.
std::optional<std::vector<std::vector<size_t>>> planDrawnSplits(const AddPath &command,
                                                                const std::vector<ParkPoint> &at) {
  std::vector<std::vector<size_t>> splits(command.Stops.size() - 1);
  for (size_t i = 0; i < command.Stops.size(); ++i) {
    if (const auto *on = std::get_if<OnDrawnSegment>(&command.Stops[i])) {
      splits[on->Stop].push_back(i);
    }
  }
  for (size_t k = 0; k < splits.size(); ++k) {
    std::ranges::sort(splits[k], {}, [&command](size_t stop) {
      return std::get<OnDrawnSegment>(command.Stops[stop]).Along;
    });
    ParkPoint from = at[k];
    for (const size_t stop : splits[k]) {
      if (segmentLine(from, at[stop]).empty()) {
        return std::nullopt;
      }
      from = at[stop];
    }
    if (!splits[k].empty() && segmentLine(from, at[k + 1]).empty()) {
      return std::nullopt;
    }
  }
  return splits;
}

// The plan of an AddPath the world accepts, or none when it refuses it.
std::optional<PathPlan> planPath(const World &world, const AddPath &command) {
  if (!isKnown(command.Kind) || command.Stops.size() < 2) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  std::optional<std::vector<ParkPoint>> positions = stopPositions(world, command);
  if (!positions || !std::ranges::all_of(command.Stops, [&](const PathStop &stop) {
        return isStopOfKind(world, segments, stop, command.Kind);
      })) {
    return std::nullopt;
  }
  std::optional<std::vector<std::vector<size_t>>> drawnSplits =
      planDrawnSplits(command, *positions);
  if (!drawnSplits) {
    return std::nullopt;
  }
  PathPlan plan{std::move(*positions), {}, std::move(*drawnSplits)};
  const std::vector<Solid> solids = solidsOf(world);
  for (size_t i = 0; i + 1 < plan.Positions.size(); ++i) {
    std::vector<CarrierPoint> points = segmentLine(plan.Positions[i], plan.Positions[i + 1]);
    if (points.empty() || !fits(Line{std::move(points), pathWidth(command.Kind) / 2.0}, solids)) {
      return std::nullopt;
    }
  }
  for (size_t i = 0; i < command.Stops.size(); ++i) {
    if (const auto *on = std::get_if<OnSegment>(&command.Stops[i])) {
      const auto split = std::ranges::find(plan.Splits, on->Segment, &Split::Segment);
      if (split == plan.Splits.end()) {
        plan.Splits.push_back(Split{on->Segment, {i}});
      } else {
        split->Stops.push_back(i);
      }
    }
  }
  std::ranges::sort(plan.Splits, {}, &Split::Segment);
  for (Split &split : plan.Splits) {
    const auto along = [&command](size_t stop) {
      return std::get<OnSegment>(command.Stops[stop]).Along;
    };
    std::ranges::sort(split.Stops, {}, along);
    // Each part of the split segment, from its From point through its stops to its To point, must
    // have a ground line. The segment has one, so both of its ends are path points.
    const ParkSegment &segment = *findSegment(segments, split.Segment);
    ParkPoint from = findIntent<PathPointIntent>(world, segment.From)->At;
    for (const size_t stop : split.Stops) {
      if (segmentLine(from, plan.Positions[stop]).empty()) {
        return std::nullopt;
      }
      from = plan.Positions[stop];
    }
    if (segmentLine(from, findIntent<PathPointIntent>(world, segment.To)->At).empty()) {
      return std::nullopt;
    }
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
  const auto addPoint = [&world](ParkPoint at) {
    const EntityKey key = world.createEntity();
    world.Registry.emplace<PathPointIntent>(world.findEntity(key), PathPointIntent{at});
    return key;
  };
  const auto addSegment = [&world](PathKind kind, EntityKey from, EntityKey to, double width) {
    const EntityKey key = world.createEntity();
    world.Registry.emplace<PathSegmentIntent>(world.findEntity(key),
                                              PathSegmentIntent{kind, from, to, width});
  };
  // Each stop's point: a new one for a NewPoint or an OnSegment, in stop order, and an earlier
  // stop's for a RepeatStop.
  std::vector<EntityKey> points;
  for (size_t i = 0; i < command.Stops.size(); ++i) {
    if (const auto *on = std::get_if<OnPoint>(&command.Stops[i])) {
      points.push_back(on->Point);
    } else if (const auto *repeat = std::get_if<RepeatStop>(&command.Stops[i])) {
      points.push_back(points[repeat->Stop]);
    } else {
      points.push_back(addPoint(plan->Positions[i]));
    }
  }
  // Each split segment ends at its first stop, and new parts run on through the rest to its old
  // end.
  for (const Split &split : plan->Splits) {
    PathSegmentIntent &segment =
        world.Registry.get<PathSegmentIntent>(world.findEntity(split.Segment));
    const PathSegmentIntent was = segment;
    segment.To = points[split.Stops.front()];
    for (size_t k = 0; k < split.Stops.size(); ++k) {
      const EntityKey to = k + 1 < split.Stops.size() ? points[split.Stops[k + 1]] : was.To;
      addSegment(was.Kind, points[split.Stops[k]], to, was.Width);
    }
  }
  // Each of the path's own segments runs through the later stops on it.
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    EntityKey from = points[i];
    for (const size_t stop : plan->DrawnSplits[i]) {
      addSegment(command.Kind, from, points[stop], pathWidth(command.Kind));
      from = points[stop];
    }
    addSegment(command.Kind, from, points[i + 1], pathWidth(command.Kind));
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
