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

std::optional<Line> lineOf(PathKind kind, const std::vector<ParkPoint> &points) {
  std::vector<CarrierPoint> line = groundLine(points);
  if (line.empty()) {
    return std::nullopt;
  }
  return Line{std::move(line), pathWidth(kind) / 2.0};
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

// Every path's ground line, skipping any that is empty.
std::vector<Line> linesOf(const World &world) {
  std::vector<Line> lines;
  for (const ParkPath &path : parkPaths(world)) {
    if (std::optional<Line> line = lineOf(path.Kind, path.Points)) {
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

} // namespace

void addParkEdits(WorldSchema &schema) {
  schema.addCommand<AddPath>();
  schema.addCommand<AddBox>();
  schema.addCommand<MoveBox>();
  schema.addCommand<DeletePath>();
  schema.addCommand<DeleteBox>();
}

bool isAccepted(const World &world, const AddPath &command) {
  if (!isKnown(command.Kind)) {
    return false;
  }
  const std::optional<Line> line = lineOf(command.Kind, keptPoints(command.Points));
  return line && fits(*line, solidsOf(world));
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

bool isAccepted(const World &world, const DeletePath &command) {
  return findIntent<PathIntent>(world, command.Path) != nullptr;
}

bool isAccepted(const World &world, const DeleteBox &command) {
  return findIntent<BoxIntent>(world, command.Box) != nullptr;
}

void applyCommand(World &world, const AddPath &command) {
  if (!isAccepted(world, command)) {
    return;
  }
  const EntityKey key = world.createEntity();
  world.Registry.emplace<PathIntent>(world.findEntity(key),
                                     PathIntent{command.Kind, keptPoints(command.Points)});
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

void applyCommand(World &world, const DeletePath &command) {
  if (isAccepted(world, command)) {
    world.destroyEntity(command.Path);
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
  // An entrance or box with no footprint, or a path with an empty ground line, is not valid.
  if (solids.size() != parkEntrances(world).size() + parkBoxes(world).size() ||
      lines.size() != parkPaths(world).size()) {
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
