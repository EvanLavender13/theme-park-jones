#include "legible/food.h"

#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

// Food availability over the boxes-and-tubes slice's park files, each run 300 ticks.
namespace tpj {
namespace {

constexpr std::array<std::string_view, 3> SLICE_PARKS = {"fed.park", "warm.park", "cut.park"};
constexpr int CYCLES = 300;

World openPark(std::string_view name) {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / name, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

// The place of every node, and the midpoint of every edge, of the world's guest network.
std::vector<Place> sampledPlaces(const World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::vector<Place> places;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    places.push_back(network.nodePlace(node));
  }
  for (const NetworkEdge &edge : network.edges()) {
    places.push_back({edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0});
  }
  return places;
}

uint64_t bitsOf(double value) { return std::bit_cast<uint64_t>(value); }

TEST_CASE("Food availability at every sampled place of each slice park is its terms added in "
          "order, bit for bit, in every cycle of 300 ticks") {
  for (const std::string_view name : SLICE_PARKS) {
    INFO("park " << name);
    World world = openPark(name);
    std::vector<uint64_t> inexactTicks;
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
      stepWorld(world);
      for (const Place &place : sampledPlaces(world)) {
        const FoodAvailability availability = foodAvailability(world, place);
        double sum = 0.0;
        for (const FoodContribution &contribution : availability.Contributions) {
          sum += contribution.Term;
        }
        if (bitsOf(availability.Value) != bitsOf(sum)) {
          inexactTicks.push_back(world.Tick);
        }
      }
    }
    INFO("first tick with a value its terms do not give: "
         << (inexactTicks.empty() ? 0 : inexactTicks.front()));
    CHECK(inexactTicks.empty());
  }
}

TEST_CASE("Every sampled place of cut.park has no food availability in every cycle of 300 ticks") {
  World world = openPark("cut.park");
  std::vector<uint64_t> fedTicks;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    stepWorld(world);
    for (const Place &place : sampledPlaces(world)) {
      if (bitsOf(foodAvailability(world, place).Value) != bitsOf(0.0)) {
        fedTicks.push_back(world.Tick);
      }
    }
  }
  INFO("first tick with food available: " << (fedTicks.empty() ? 0 : fedTicks.front()));
  CHECK(fedTicks.empty());
}

TEST_CASE("Some sampled place of warm.park has food available in every cycle of 300 ticks") {
  World world = openPark("warm.park");
  std::vector<uint64_t> unfedTicks;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    stepWorld(world);
    bool fed = false;
    for (const Place &place : sampledPlaces(world)) {
      fed = fed || foodAvailability(world, place).Value > 0.0;
    }
    if (!fed) {
      unfedTicks.push_back(world.Tick);
    }
  }
  INFO("first tick with no food available: " << (unfedTicks.empty() ? 0 : unfedTicks.front()));
  CHECK(unfedTicks.empty());
}

// Checked on the opened world and after the last cycle only: hashing costs more than stepping.
TEST_CASE("Computing food availability at every sampled place of each slice park leaves its hash "
          "unchanged when opened and after 300 ticks") {
  const auto unchangedBySampling = [](const World &world) {
    const uint64_t before = hashWorld(world);
    for (const Place &place : sampledPlaces(world)) {
      static_cast<void>(foodAvailability(world, place));
    }
    return hashWorld(world) == before;
  };
  for (const std::string_view name : SLICE_PARKS) {
    INFO("park " << name);
    World world = openPark(name);
    CHECK(unchangedBySampling(world));
    for (int cycle = 0; cycle < CYCLES; ++cycle) {
      stepWorld(world);
    }
    CHECK(unchangedBySampling(world));
  }
}

} // namespace
} // namespace tpj
