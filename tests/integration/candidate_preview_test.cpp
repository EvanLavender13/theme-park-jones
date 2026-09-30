#include "legible/food.h"
#include "legible/preview.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

// Candidates of edits to warm.park one cycle in, as the app previews them.
namespace tpj {
namespace {

// A second shop between warm.park's shop 7 and guest path 5, facing -x: its front door lies 3.5 m
// from guest path 2 and its back door 2.5 m from backstage path 6, and its footprint clears the
// curve of guest path 5 by 0.27 m beyond its half width and shop 7 by 0.2 m.
constexpr AddBox ADD_SHOP{BoxKind::Shop, Pose{6.5, 106.8, -1.0, 0.0}};
// warm.park's only backstage path, which carries every supply its shop gets.
constexpr DeletePath DELETE_BACKSTAGE{EntityKey{6}};

World openWarm() {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / "warm.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

// warm.park opened and stepped one cycle, so a candidate is made from a world that has just
// finished a cycle, as the app's are.
World steppedWarm() {
  World world = openWarm();
  stepWorld(world);
  return world;
}

// A candidate is exact only if it is what committing gives: principle 8 and decision 0025.
TEST_CASE("The candidate of adding a shop to, or deleting the backstage path of, warm.park one "
          "cycle in equals the park stepped one cycle with the edit queued") {
  const World world = steppedWarm();
  const std::vector<std::pair<const char *, ParkEdit>> edits = {
      {"adding a shop touching both paths", ADD_SHOP},
      {"deleting the backstage path", DELETE_BACKSTAGE}};
  for (const auto &[name, edit] : edits) {
    INFO(name);
    REQUIRE(isAccepted(world, edit));
    CommandQueue candidateQueue;
    queueEdit(candidateQueue, edit);
    const World candidate = makeCandidate(world, candidateQueue);

    World committed = openWarm();
    CommandQueue committedQueue;
    queueEdit(committedQueue, edit);
    stepWorld(committed, committedQueue);
    CHECK(worldsEqual(candidate, committed));
  }
}

TEST_CASE("In warm.park one cycle in, a shop added touching both paths has a connection and a "
          "supply, and raises food availability at its connection") {
  const World world = steppedWarm();
  const Preview preview = previewEdit(world, ADD_SHOP);
  REQUIRE(preview.Candidate.has_value());
  REQUIRE(preview.Shop.has_value());
  const ShopContext context = preview.Shop.value_or(ShopContext{});
  CHECK(context.Supply.has_value());
  REQUIRE(context.Connection.has_value());
  if (preview.Candidate.has_value()) {
    const Place connection = context.Connection.value_or(Place{});
    CHECK(foodAvailability(*preview.Candidate, connection).Value >
          foodAvailability(world, connection).Value);
  }
}

// Principle 8: cutting the park's only supply route shows as no food anywhere.
TEST_CASE("In warm.park one cycle in, deleting the backstage path leaves no food available at any "
          "node of the candidate's guest network, where the world has some") {
  const World world = steppedWarm();
  const Network &committed = parkNetwork(world, PathKind::Guest);
  bool fed = false;
  for (uint32_t node = 0; node < committed.nodeCount(); ++node) {
    fed = fed || foodAvailability(world, committed.nodePlace(node)).Value > 0.0;
  }
  CHECK(fed);

  const Preview preview = previewEdit(world, DELETE_BACKSTAGE);
  REQUIRE(preview.Candidate.has_value());
  if (preview.Candidate.has_value()) {
    const World &candidate = *preview.Candidate;
    const Network &network = parkNetwork(candidate, PathKind::Guest);
    REQUIRE(network.nodeCount() > 0);
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      INFO("node " << node);
      CHECK(foodAvailability(candidate, network.nodePlace(node)).Value == 0.0);
    }
  }
}

} // namespace
} // namespace tpj
