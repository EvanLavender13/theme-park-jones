#include "views/food_overlay.h"

#include "legible/food.h"
#include "render/food_overlay.h"
#include "render/park_mesh.h"
#include "sim/medium/network.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace tpj {
namespace {

// warm.park's shop is stocked and its food offer says meals are supplied, so the availability is
// above 0 along its paths and the shading depends on the value function.
World warmPark() {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

bool sameVertex(const ParkVertex &a, const ParkVertex &b) {
  return std::equal(std::begin(a.Position), std::end(a.Position), std::begin(b.Position)) &&
         std::equal(std::begin(a.Normal), std::end(a.Normal), std::begin(b.Normal)) &&
         a.Color == b.Color;
}

TEST_CASE("buildFoodAvailabilityOverlay gives buildFoodOverlay shaded by foodAvailability's Value, "
          "vertex for vertex and index for index") {
  const World world = warmPark();
  const ParkMesh expected = buildFoodOverlay(
      world, [&world](const Place &place) { return foodAvailability(world, place).Value; });
  // Were every place's availability 0, an overlay shaded by any value not above 0 would match.
  REQUIRE(std::ranges::any_of(expected.Vertices, [](const ParkVertex &vertex) {
    return !(vertex.Color == OVERLAY_ZERO_COLOR);
  }));

  const ParkMesh actual = buildFoodAvailabilityOverlay(world);

  REQUIRE(actual.Vertices.size() == expected.Vertices.size());
  for (size_t i = 0; i < expected.Vertices.size(); ++i) {
    INFO("vertex " << i);
    CHECK(sameVertex(actual.Vertices[i], expected.Vertices[i]));
  }
  CHECK(actual.Indices == expected.Indices);
}

TEST_CASE("buildFoodAvailabilityOverlay changes nothing in the world it shades") {
  const World world = warmPark();
  const World before = copyWorld(world);

  const ParkMesh overlay = buildFoodAvailabilityOverlay(world);

  CHECK_FALSE(overlay.Vertices.empty());
  CHECK(worldsEqual(world, before));
  CHECK(world.isResolvePending() == before.isResolvePending());
}

} // namespace
} // namespace tpj
