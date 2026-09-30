#include "support/guest_parks.h"
#include "support/park_worlds.h"
#include "support/route_edits.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::recordOf;

TEST_CASE("A guest whose place stops resolving leaves the park in the next cycle, and guests whose "
          "places still resolve stay") {
  World world = test::legsWorld();
  const EntityKey farGuest = test::walkOntoFarLeg(world);
  CommandQueue cut;
  cut.push(DeletePath{test::FAR_LEG});
  stepWorld(world, cut);

  std::vector<EntityKey> stranded;
  std::vector<EntityKey> kept;
  for (const EntityKey guest : parkGuests(world)) {
    const bool resolves = test::guestNetwork(world).resolve(recordOf(world, guest).At).has_value();
    (resolves ? kept : stranded).push_back(guest);
  }
  REQUIRE(std::ranges::find(stranded, farGuest) != stranded.end());
  REQUIRE_FALSE(kept.empty());

  stepWorld(world);
  for (const EntityKey guest : stranded) {
    CAPTURE(guest);
    CHECK_FALSE(guestRecord(world, guest).has_value());
    CHECK_FALSE(test::isLive(world, guest));
  }
  for (const EntityKey guest : kept) {
    CAPTURE(guest);
    CHECK(guestRecord(world, guest).has_value());
  }
}

// Two entrances, north and south, each with a guest path from its door to the middle of the park,
// where random edits gather, so edits add paths joining the guests' network and delete paths under
// walking guests.
World twoGatesWorld() {
  return test::resolvedWorld(test::ParkIntent{
      .Entrances = {test::northGate(EntityKey{1}, 0.0),
                    ParkEntrance{.Key = EntityKey{2}, .At = Pose{0.0, -126.5, 0.0, 1.0}}},
      .Paths = {test::guestPath(EntityKey{3}, {{0.0, 123.0}, {0.0, 0.0}}),
                test::guestPath(EntityKey{4}, {{0.0, -123.0}, {0.0, 0.0}})},
      .Boxes = {}});
}

constexpr int EDITS = 40;
// Each edit is followed by fewer cycles than this with no command.
constexpr uint64_t IDLE_CYCLES = 20;
// Five guests arrive at each entrance, and walk, before the first edit.
constexpr uint64_t WARM_UP_CYCLES = 5 * ARRIVAL_INTERVAL;

// Runs a random edit sequence from the two-gates park: warm-up cycles with no command, then each
// edit applied by one cycle and followed by a drawn number of cycles with no command. beforeCycle
// sees the world and the edit the cycle about to step it applies, if any. afterCycle sees the world
// after every cycle, with whether that cycle applied an edit.
template <typename BeforeCycle, typename AfterCycle>
void runGuestEdits(uint64_t seed, BeforeCycle beforeCycle, AfterCycle afterCycle) {
  test::RouteEditDraws draws(seed);
  World world = twoGatesWorld();
  const auto runCycle = [&](bool edited) {
    INFO("tick " << world.Tick);
    CommandQueue queue;
    std::optional<ParkEdit> edit;
    if (edited) {
      edit = test::routeEdit(draws, world);
      queueEdit(queue, edit.value_or(ParkEdit{}));
    }
    beforeCycle(world, edit);
    REQUIRE_NOTHROW(stepWorld(world, queue));
    afterCycle(world, edited);
  };
  for (uint64_t cycle = 0; cycle < WARM_UP_CYCLES; ++cycle) {
    runCycle(false);
  }
  for (int edit = 0; edit < EDITS; ++edit) {
    INFO("edit " << edit);
    runCycle(true);
    const uint64_t idle = draws.below(IDLE_CYCLES);
    for (uint64_t cycle = 0; cycle < idle; ++cycle) {
      runCycle(false);
    }
  }
}

template <typename AfterCycle> void runGuestEdits(uint64_t seed, AfterCycle afterCycle) {
  runGuestEdits(
      seed, [](const World & /*world*/, const std::optional<ParkEdit> & /*edit*/) {}, afterCycle);
}

bool allResolve(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::all_of(guests, [&](EntityKey guest) {
    return test::guestNetwork(world).resolve(recordOf(world, guest).At).has_value();
  });
}

TEST_CASE("No cycle of randomized park edits with guests walking throws") {
  bool sawEditAmongGuests = false;
  bool sawGuestCutOff = false;
  runGuestEdits(
      71,
      [&](const World &world, const std::optional<ParkEdit> &edit) {
        sawEditAmongGuests = sawEditAmongGuests || (edit.has_value() && !parkGuests(world).empty());
      },
      [&](const World &world, bool edited) {
        sawGuestCutOff = sawGuestCutOff || (edited && !allResolve(world));
      });
  CHECK(sawEditAmongGuests);
  CHECK(sawGuestCutOff);
}

TEST_CASE("After every cycle of randomized park edits with guests walking that applies no edit, "
          "every guest's place resolves on the guest network") {
  bool sawGuests = false;
  runGuestEdits(72, [&](const World &world, bool edited) {
    if (!edited) {
      REQUIRE(allResolve(world));
      sawGuests = sawGuests || !parkGuests(world).empty();
    }
  });
  CHECK(sawGuests);
}

TEST_CASE("Every world randomized park edits with guests walking reach equals its copy") {
  bool sawGuests = false;
  runGuestEdits(73, [&](const World &world, bool /*edited*/) {
    REQUIRE(worldsEqual(copyWorld(world), world));
    sawGuests = sawGuests || !parkGuests(world).empty();
  });
  CHECK(sawGuests);
}

TEST_CASE("Every world randomized park edits with guests walking reach equals its save loaded and "
          "resolved") {
  bool sawGuests = false;
  runGuestEdits(74, [&](const World &world, bool /*edited*/) {
    World loaded = loadWorld(makeParkSchema(), saveWorld(world));
    resolveWorld(loaded);
    REQUIRE(worldsEqual(loaded, world));
    sawGuests = sawGuests || !parkGuests(world).empty();
  });
  CHECK(sawGuests);
}

TEST_CASE("A candidate made with an edit from a world of randomized park edits with guests "
          "walking, once it has stepped a cycle, equals the world that queues the edit for that "
          "cycle") {
  std::optional<World> candidate;
  int compared = 0;
  bool sawGuests = false;
  runGuestEdits(
      75,
      [&](const World &world, const std::optional<ParkEdit> &edit) {
        if (edit.has_value()) {
          World previewed = copyWorld(world);
          stepWorld(previewed);
          CommandQueue queue;
          queueEdit(queue, edit.value_or(ParkEdit{}));
          candidate.emplace(makeCandidate(previewed, queue));
          sawGuests = sawGuests || !parkGuests(world).empty();
        }
      },
      [&](const World &world, bool edited) {
        if (edited) {
          REQUIRE(candidate.has_value());
          REQUIRE(worldsEqual(candidate.value(), world));
          candidate.reset();
          ++compared;
        }
      });
  CHECK(compared == EDITS);
  CHECK(sawGuests);
}

} // namespace
} // namespace tpj
