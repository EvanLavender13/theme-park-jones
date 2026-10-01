#include "sim/entity_key.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace tpj {
namespace {

// The ground distance from a point to a footprint's rectangle, measured along its own axes.
double distanceToFootprint(const CarrierPoint &point, const Footprint &footprint,
                           FootprintSize size) {
  const double centerX = (footprint.Corners[0].X + footprint.Corners[2].X) / 2.0;
  const double centerZ = (footprint.Corners[0].Z + footprint.Corners[2].Z) / 2.0;
  const double dx = point.X - centerX;
  const double dz = point.Z - centerZ;
  const double along = std::abs(dx * footprint.Forward.X + dz * footprint.Forward.Z);
  const double across = std::abs(dx * footprint.Right.X + dz * footprint.Right.Z);
  const double outAlong = std::max(0.0, along - size.Depth / 2.0);
  const double outAcross = std::max(0.0, across - size.Width / 2.0);
  return std::sqrt(outAlong * outAlong + outAcross * outAcross);
}

TEST_CASE("makeNewPark gives the template with the given schema and seed: an entrance on the +z "
          "edge and a guest path leading in") {
  // A schema holding only park intent, unlike the park's, so the world shows which schema it got.
  auto schema = std::make_shared<WorldSchema>();
  addParkIntent(*schema);
  const std::shared_ptr<const WorldSchema> given = schema;
  const World park = makeNewPark(given, 7);

  CHECK(&park.schema() == given.get());
  CHECK(park.Seed == 7);
  CHECK(park.Tick == 0);
  CHECK(park.nextKey() == 3);
  CHECK(park.isResolvePending());
  CHECK(park.keys() == std::vector<EntityKey>{EntityKey{1}, EntityKey{2}});
  CHECK(parkEntrances(park) ==
        std::vector<ParkEntrance>{{EntityKey{1}, Pose{0.0, 126.5, 0.0, -1.0}}});
  CHECK(parkPaths(park) ==
        std::vector<ParkPath>{{EntityKey{2}, PathKind::Guest, {{0.0, 123.0}, {0.0, 103.0}}}});
  CHECK(parkBoxes(park).empty());
}

TEST_CASE("makeNewPark(seed) is makeNewPark with the park's schema and the seed") {
  const World park = makeNewPark(7);

  CHECK(park.schema().sameComponents(*makeParkSchema()));
  CHECK(park.Seed == 7);
  CHECK(worldsEqual(park, makeNewPark(makeParkSchema(), 7)));
}

TEST_CASE(
    "The template's entrance and widened path lie in the park, the path clear of the entrance") {
  const World park = makeNewPark(1);
  const auto entrances = parkEntrances(park);
  const auto paths = parkPaths(park);
  REQUIRE(entrances.size() == 1);
  REQUIRE(paths.size() == 1);

  const double edge = PARK_SIZE / 2.0;
  const std::optional<Footprint> entrance = footprintOf(entrances[0].At, ENTRANCE_SIZE);
  REQUIRE(entrance.has_value());
  const Footprint footprint = entrance.value_or(Footprint{});
  for (const ParkPoint &corner : footprint.Corners) {
    CHECK(std::abs(corner.X) <= edge);
    CHECK(std::abs(corner.Z) <= edge);
  }

  const auto line = groundLine(paths[0].Points);
  REQUIRE_FALSE(line.empty());
  const double halfWidth = pathWidth(paths[0].Kind) / 2.0;
  for (const CarrierPoint &point : line) {
    CHECK(std::abs(point.X) + halfWidth <= edge);
    CHECK(std::abs(point.Z) + halfWidth <= edge);
    CHECK(distanceToFootprint(point, footprint, ENTRANCE_SIZE) >= halfWidth);
  }
}

TEST_CASE("tests/parks/new.park is the save of makeNewPark with seed 1") {
  std::ifstream file(TPJ_PARKS_DIR "/new.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();

  CHECK(text.str() == saveWorld(makeNewPark(1)));
}

} // namespace
} // namespace tpj
