#include "support/tool_park.h"

#include "tools/tools.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
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

TEST_CASE("boxAt counts a footprint's edges as holding the point") {
  const World world = toolPark();
  // A corner of the turned shop, 3 m along its facing and 4 m across it, so its depth runs along
  // its facing and its width across.
  CHECK(boxAt(world, {37.0, -4.0}) == std::optional{TURNED_SHOP});
  CHECK(boxAt(world, {48.0, 3.0}) == std::optional{OVERLAPPING_SHOP});
}

TEST_CASE("boxAt gives none where no box's footprint holds the point, and never the entrance") {
  const World world = toolPark();
  // 3.5 m along the turned shop's facing: outside its 6 m depth, inside its 8 m width.
  CHECK(!boxAt(world, {36.5, 0.0}).has_value());
  // Just past the overlapping shop's side.
  CHECK(!boxAt(world, {48.01, 0.0}).has_value());
  // The position of a box with no footprint.
  CHECK(!boxAt(world, {80.0, -80.0}).has_value());
  // Inside the entrance's footprint.
  CHECK(!boxAt(world, {3.0, 127.0}).has_value());
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
  CHECK(!pathAt(world, {-38.6, 30.0}).has_value());
  // Inside the entrance's footprint, 5 m from the template path's start.
  CHECK(!pathAt(world, {3.0, 127.0}).has_value());
}

} // namespace
} // namespace tpj
