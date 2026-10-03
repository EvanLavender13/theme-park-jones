#include "support/tool_park.h"

#include "tools/tools.h"

#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace tpj {
namespace {

using test::BACKSTAGE_PATH;
using test::CROSSING_PATH;
using test::OVERLAPPING_SHOP;
using test::toolPark;
using test::TURNED_SHOP;

TEST_CASE("boxAt gives the least-keyed box whose footprint holds the point") {
  const World world = toolPark();
  // Held by both shops, and nearer the overlapping shop's center, so neither the nearest box nor
  // the last one answers for the least key.
  CHECK(boxAt(world, {42.5, 0.0}) == std::optional{TURNED_SHOP});
  // Held by the overlapping shop alone.
  CHECK(boxAt(world, {47.0, 0.0}) == std::optional{OVERLAPPING_SHOP});
  // Held by the depot, which stands over a path.
  CHECK(boxAt(world, {-40.0, 20.0}) == std::optional{test::DEPOT_ON_PATH});
}

TEST_CASE("boxAt gives none where no box's footprint holds the point, and never the entrance") {
  const World world = toolPark();
  // 3.5 m along the turned shop's facing: outside its 6 m depth, inside its 8 m width.
  CHECK_FALSE(boxAt(world, {36.5, 0.0}).has_value());
  // Just past the overlapping shop's side.
  CHECK_FALSE(boxAt(world, {48.01, 0.0}).has_value());
  // The position of a box with no footprint.
  CHECK_FALSE(boxAt(world, {80.0, -80.0}).has_value());
  // Inside the entrance's footprint.
  CHECK_FALSE(boxAt(world, {3.0, 127.0}).has_value());
}

TEST_CASE("pathAt gives the least-keyed path whose ground line has a segment within half its "
          "pathWidth of the point") {
  const World world = toolPark();
  // Within reach of both paths where they cross, and nearer the crossing path's line, so neither
  // the nearest path nor the last one answers for the least key.
  CHECK(pathAt(world, {-40.5, 0.3}) == std::optional{BACKSTAGE_PATH});
  // 1.4 m from the guest path, within its half width of 1.5 m.
  CHECK(pathAt(world, {-50.0, 1.4}) == std::optional{CROSSING_PATH});

  // 0.9 m from the backstage line, but midway between two of its points, so a segment is within
  // its half width of 1 m while no point of the line is.
  const ParkPoint between{-39.1, 30.5};
  const std::vector<ParkPath> paths = parkPaths(world);
  REQUIRE(paths.at(1).Key == BACKSTAGE_PATH);
  const std::vector<CarrierPoint> line = groundLine(paths.at(1).Points);
  REQUIRE(std::ranges::none_of(line, [&between](const CarrierPoint &point) {
    return std::hypot(point.X - between.X, point.Z - between.Z) <= 1.0;
  }));
  CHECK(pathAt(world, between) == std::optional{BACKSTAGE_PATH});
}

TEST_CASE("pathAt gives none where no path's ground line comes within half its pathWidth, and "
          "never the entrance") {
  const World world = toolPark();
  // 1.4 m from the backstage path: beyond its half width, though within a guest path's.
  CHECK_FALSE(pathAt(world, {-38.6, 30.0}).has_value());
  // Inside the entrance's footprint, 5 m from the template path's start.
  CHECK_FALSE(pathAt(world, {3.0, 127.0}).has_value());
}

// The tool park with two more guest paths: a curve through (60, -60), (80, -40), and (60, -20),
// whose ground line bends away from the lines between its points, and a straight path along
// z = 3 from x = -30 to -10, beside the crossing path and keyed after it.
World snapPark() {
  World world = toolPark();
  for (const AddPath &path :
       {AddPath{PathKind::Guest, {{60.0, -60.0}, {80.0, -40.0}, {60.0, -20.0}}},
        AddPath{PathKind::Guest, {{-30.0, 3.0}, {-10.0, 3.0}}}}) {
    REQUIRE(isAccepted(world, path));
    applyCommand(world, path);
  }
  return world;
}

double distance(ParkPoint from, ParkPoint to) { return std::hypot(to.X - from.X, to.Z - from.Z); }

// How near the point comes to any segment of any ground line of the kind: what snapToPath must
// match, since no point of such a segment may be nearer than the one it gives.
double lineDistance(const World &world, PathKind kind, ParkPoint point) {
  double nearest = std::numeric_limits<double>::infinity();
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind != kind) {
      continue;
    }
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    for (std::size_t i = 1; i < line.size(); ++i) {
      const ParkPoint from{line[i - 1].X, line[i - 1].Z};
      const double alongX = line[i].X - from.X;
      const double alongZ = line[i].Z - from.Z;
      const double t = std::clamp(((point.X - from.X) * alongX + (point.Z - from.Z) * alongZ) /
                                      (alongX * alongX + alongZ * alongZ),
                                  0.0, 1.0);
      nearest = std::min(nearest, distance(point, {from.X + alongX * t, from.Z + alongZ * t}));
    }
  }
  return nearest;
}

// Distances are compared with a margin for the rounding of recomputing them.
constexpr double ROUNDING = 1e-9;

TEST_CASE("snapToPath gives the point itself when no ground line of a path of the kind comes "
          "within SNAP_REACH") {
  CHECK(SNAP_REACH == 2.0);
  const World world = snapPark();
  const auto checkUnsnapped = [&world](PathKind kind, ParkPoint point) {
    INFO("point " << point.X << ", " << point.Z);
    CHECK(snapToPath(world, kind, point) == point);
  };
  // Far from every path.
  checkUnsnapped(PathKind::Guest, {100.0, 100.0});
  checkUnsnapped(PathKind::Backstage, {100.0, 100.0});
  // 2.1 m from the crossing path, just beyond reach.
  checkUnsnapped(PathKind::Guest, {-50.0, 2.1});
  // 2.1 m from the backstage path, just beyond reach.
  checkUnsnapped(PathKind::Backstage, {-37.9, -30.0});
  // 1.5 m from the backstage path, but a guest path is sought.
  checkUnsnapped(PathKind::Guest, {-41.5, -30.0});
}

TEST_CASE("snapToPath gives a point on a ground line of the kind within SNAP_REACH, and no point "
          "of any such line is nearer") {
  const World world = snapPark();
  const auto checkSnapped = [&world](PathKind kind, ParkPoint point) {
    INFO("point " << point.X << ", " << point.Z);
    const ParkPoint snapped = snapToPath(world, kind, point);
    CHECK(distance(point, snapped) <= SNAP_REACH);
    CHECK(lineDistance(world, kind, snapped) <= ROUNDING);
    CHECK(distance(point, snapped) <= lineDistance(world, kind, point) + ROUNDING);
  };
  // Beside the middle of the crossing path.
  checkSnapped(PathKind::Guest, {-50.0, 1.2});
  // Between two points of the backstage line, nearer a segment than any of its points.
  checkSnapped(PathKind::Backstage, {-39.1, 30.5});
  // Past the end of the crossing path, so the nearest point is its end.
  checkSnapped(PathKind::Guest, {-18.5, 0.5});
  // Within reach of the crossing path and of the later, nearer path along z = 3.
  checkSnapped(PathKind::Guest, {-25.0, 1.8});
  // 1 m from the curve's middle point, where its ground line bends.
  checkSnapped(PathKind::Guest, {79.2, -40.6});
  checkSnapped(PathKind::Backstage, {-38.1, -30.0});
}

TEST_CASE("Paths of the other kind never change snapToPath") {
  const World world = snapPark();
  World guestOnly = copyWorld(world);
  World backstageOnly = copyWorld(world);
  for (const ParkPath &path : parkPaths(world)) {
    applyCommand(path.Kind == PathKind::Guest ? backstageOnly : guestOnly, DeletePath{path.Key});
  }
  REQUIRE(parkPaths(guestOnly).size() == 4);
  REQUIRE(parkPaths(backstageOnly).size() == 1);

  // Near where the crossing and backstage paths cross: 0.5 m from the backstage path and 1.5 m from
  // the crossing path, so both are within reach and the other kind's path is nearer for a guest.
  const ParkPoint crossing{-40.5, 1.5};
  REQUIRE(snapToPath(world, PathKind::Guest, crossing) != crossing);
  REQUIRE(snapToPath(world, PathKind::Backstage, crossing) != crossing);
  CHECK(snapToPath(world, PathKind::Guest, crossing) ==
        snapToPath(guestOnly, PathKind::Guest, crossing));
  CHECK(snapToPath(world, PathKind::Backstage, crossing) ==
        snapToPath(backstageOnly, PathKind::Backstage, crossing));
}

} // namespace
} // namespace tpj
