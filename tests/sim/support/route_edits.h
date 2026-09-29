#ifndef TPJ_TESTS_SIM_SUPPORT_ROUTE_EDITS_H
#define TPJ_TESTS_SIM_SUPPORT_ROUTE_EDITS_H

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdint.h>
#include <vector>

// Random park edits that keep meeting what is already drawn: paths snapped onto other paths, and
// boxes beside lines, so edit sequences reach junctions, connected boxes, and unconnected doors.
namespace tpj::test {

// Keyed draws, so a failing sequence is the same on every build and in every run.
class RouteEditDraws {
public:
  explicit RouteEditDraws(uint64_t seed) : Seed(seed) {}

  double uniform() {
    return drawUniform(DrawKey{Seed, NULL_KEY, hashName("route-edits"), 0, Index++});
  }
  double between(double low, double high) { return low + ((high - low) * uniform()); }
  uint64_t below(uint64_t count) {
    return static_cast<uint64_t>(uniform() * static_cast<double>(count));
  }
  bool oneIn(uint64_t count) { return below(count) == 0; }

private:
  uint64_t Seed;
  uint64_t Index = 0;
};

// Paths gather in the middle of the park, so new ones often meet old ones.
inline constexpr double ROUTE_EDIT_REGION = 40.0;

// A point on a line already drawn, as the path tool snaps a click: one of its line points, or a
// point between two of them.
inline ParkPoint snappedPoint(RouteEditDraws &draws, const ParkPath &path) {
  const std::vector<CarrierPoint> line = groundLine(path.Points);
  if (line.size() < 2) {
    return path.Points.front();
  }
  const std::size_t index = draws.below(line.size() - 1);
  const CarrierPoint &a = line[index];
  if (draws.oneIn(2)) {
    return {a.X, a.Z};
  }
  const CarrierPoint &b = line[index + 1];
  const double t = draws.uniform();
  return {a.X + (t * (b.X - a.X)), a.Z + (t * (b.Z - a.Z))};
}

inline AddPath routeEditPath(RouteEditDraws &draws, const std::vector<ParkPath> &paths) {
  // Now and then a copy of a path already drawn, which overlaps it along its whole length when
  // the kinds agree.
  if (!paths.empty() && draws.oneIn(10)) {
    const ParkPath &copied = paths[draws.below(paths.size())];
    return AddPath{static_cast<PathKind>(draws.below(2)), copied.Points};
  }
  AddPath command{static_cast<PathKind>(draws.below(2)), {}};
  const std::size_t count = 2 + draws.below(3);
  const double region = ROUTE_EDIT_REGION;
  ParkPoint point{draws.between(-region, region), draws.between(-region, region)};
  for (std::size_t index = 0; index < count; ++index) {
    if (!paths.empty() && draws.oneIn(3)) {
      point = snappedPoint(draws, paths[draws.below(paths.size())]);
    } else if (index > 0) {
      point = {std::clamp(point.X + draws.between(-25.0, 25.0), -region, region),
               std::clamp(point.Z + draws.between(-25.0, 25.0), -region, region)};
    }
    command.Points.push_back(point);
  }
  return command;
}

inline Pose routeEditPose(RouteEditDraws &draws) {
  const double region = ROUTE_EDIT_REGION;
  return Pose{draws.between(-region, region), draws.between(-region, region),
              draws.between(-1.0, 1.0), draws.between(-1.0, 1.0)};
}

// A box beside a line already drawn, its front, or a shop's back, facing the line across a gap,
// so its door lies sometimes within CONNECTION_REACH of the line and sometimes beyond it.
inline AddBox besideLine(RouteEditDraws &draws, const std::vector<ParkPath> &paths) {
  const ParkPath &path = paths[draws.below(paths.size())];
  const std::vector<CarrierPoint> line = groundLine(path.Points);
  const auto kind = static_cast<BoxKind>(draws.below(2));
  if (line.size() < 2) {
    return AddBox{kind, routeEditPose(draws)};
  }
  const std::size_t index = draws.below(line.size() - 1);
  const CarrierPoint &a = line[index];
  const CarrierPoint &b = line[index + 1];
  const double length = std::sqrt(((b.X - a.X) * (b.X - a.X)) + ((b.Z - a.Z) * (b.Z - a.Z)));
  const double side = draws.oneIn(2) ? 1.0 : -1.0;
  const double normalX = side * -(b.Z - a.Z) / length;
  const double normalZ = side * (b.X - a.X) / length;
  const double door = (pathWidth(path.Kind) / 2.0) + draws.between(0.05, 3.0);
  const double center = door + (boxSize(kind).Depth / 2.0);
  const bool backToLine = kind == BoxKind::Shop && draws.oneIn(2);
  const double facing = backToLine ? 1.0 : -1.0;
  return AddBox{kind, Pose{a.X + (center * normalX), a.Z + (center * normalZ), facing * normalX,
                           facing * normalZ}};
}

// One park edit: mostly adding and deleting paths, which the networks follow, with some boxes,
// which block paths and connect to them, and small moves, which keep a box's connectors.
inline ParkEdit routeEdit(RouteEditDraws &draws, const World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  const uint64_t choice = draws.below(12);
  if (choice < 4) {
    return routeEditPath(draws, paths);
  }
  if (choice < 6 && !paths.empty()) {
    return DeletePath{paths[draws.below(paths.size())].Key};
  }
  if (choice < 9 && choice > 6 && !paths.empty()) {
    return besideLine(draws, paths);
  }
  if (choice < 9 || boxes.empty()) {
    return AddBox{static_cast<BoxKind>(draws.below(2)), routeEditPose(draws)};
  }
  const ParkBox &box = boxes[draws.below(boxes.size())];
  switch (draws.below(4)) {
  case 0:
    return MoveBox{box.Key, routeEditPose(draws)};
  case 1:
    return DeleteBox{box.Key};
  default:
    return MoveBox{box.Key,
                   Pose{box.At.X + draws.between(-1.0, 1.0), box.At.Z + draws.between(-1.0, 1.0),
                        box.At.FacingX, box.At.FacingZ}};
  }
}

} // namespace tpj::test

#endif
