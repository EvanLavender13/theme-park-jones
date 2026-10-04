#include "support/guest_parks.h"
#include "support/ledger_writes.h"
#include "support/route_edits.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::recordOf;

constexpr int EDITS = 40;
// Sampling the footfall at every stretch each cycle is costly on the networks edits grow, so the
// footfall test runs fewer edits.
constexpr int FOOTFALL_EDITS = 12;
// Each edit is followed by fewer cycles than this with no command.
constexpr uint64_t IDLE_CYCLES = 10;
// More cycles than the eating park takes to have a guest eat.
constexpr uint64_t WARM_UP_LIMIT = 30 * ARRIVAL_INTERVAL;

bool allResolve(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::all_of(guests, [&](EntityKey guest) {
    return test::guestNetwork(world).resolve(recordOf(world, guest).At).has_value();
  });
}

bool anyWaiting(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(guests, [&](EntityKey guest) {
    return recordOf(world, guest).Activity == GuestActivity::Waiting;
  });
}

// Runs a random edit sequence from the eating park once guests are walking, waiting, and eating:
// the park is stepped with no command until a guest has eaten and a guest waits, and then each
// edit is applied by one cycle and followed by a drawn number of cycles with no command.
// beforeCycle sees the world and the edit the cycle about to step it applies, if any, and
// afterCycle the world after every cycle of the sequence, with whether that cycle applied an edit.
template <typename BeforeCycle, typename AfterCycle>
void runGuestEdits(uint64_t seed, BeforeCycle beforeCycle, AfterCycle afterCycle,
                   int edits = EDITS) {
  test::RouteEditDraws draws(seed);
  World world = test::eatingWorld();
  while (unitsConsumed<Meals>(world, EATEN_CAUSE) == 0 || !anyWaiting(world)) {
    REQUIRE(world.Tick < WARM_UP_LIMIT);
    stepWorld(world);
  }
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
  for (int edit = 0; edit < edits; ++edit) {
    INFO("edit " << edit);
    runCycle(true);
    const uint64_t idle = draws.below(IDLE_CYCLES);
    for (uint64_t cycle = 0; cycle < idle; ++cycle) {
      runCycle(false);
    }
  }
}

template <typename AfterCycle>
void runGuestEdits(uint64_t seed, AfterCycle afterCycle, int edits = EDITS) {
  runGuestEdits(
      seed, [](const World & /*world*/, const std::optional<ParkEdit> & /*edit*/) {}, afterCycle,
      edits);
}

// Watches the edited cycles of a run for one that retires a guest's place, taking its path or
// connector away, while the guest network keeps a carrier.
struct RetireWatch {
  Network Before;
  std::vector<Place> Places;
  bool Retired = false;

  // The systems walk, admit, and remove guests before the cycle's commands apply, so the places
  // the resolution carries are those of the world stepped a cycle with no commands.
  void before(const World &world) {
    Before = test::guestNetwork(world);
    World stepped = copyWorld(world);
    stepWorld(stepped);
    Places.clear();
    for (const EntityKey guest : parkGuests(stepped)) {
      Places.push_back(recordOf(stepped, guest).At);
    }
  }

  void after(const World &world) {
    const Network &after = test::guestNetwork(world);
    if (after.carriers().empty()) {
      return;
    }
    Retired =
        Retired || std::ranges::any_of(Places, [&](const Place &place) {
          return Before.resolve(place).has_value() && !carryOver(place, Before, after).has_value();
        });
  }
};

TEST_CASE("After every cycle of randomized park edits with guests walking, waiting, and eating "
          "that leaves the guest network a carrier, every guest's place resolves on it, including "
          "cycles whose edit takes a guest's path away") {
  bool sawGuests = false;
  RetireWatch retire;
  runGuestEdits(
      72,
      [&](const World &world, const std::optional<ParkEdit> &edit) {
        if (edit.has_value()) {
          retire.before(world);
        }
      },
      [&](const World &world, bool edited) {
        if (edited) {
          retire.after(world);
        }
        if (!test::guestNetwork(world).carriers().empty()) {
          REQUIRE(allResolve(world));
          sawGuests = sawGuests || !parkGuests(world).empty();
        }
      });
  CHECK(sawGuests);
  CHECK(retire.Retired);
}

TEST_CASE("In every cycle of randomized park edits with guests walking, waiting, and eating, "
          "hungry footfall inside every stretch is finite and not below 0") {
  bool sawFootfall = false;
  runGuestEdits(
      77,
      [&](const World &world, bool /*edited*/) {
        for (const NetworkEdge &edge : test::guestNetwork(world).edges()) {
          const double value = test::footfallAt(world, test::midpointOf(edge));
          CAPTURE(edge.Carrier, edge.FromDistance, edge.ToDistance, value);
          REQUIRE(std::isfinite(value));
          REQUIRE(value >= 0.0);
          sawFootfall = sawFootfall || value > 0.0;
        }
      },
      FOOTFALL_EDITS);
  CHECK(sawFootfall);
}

// The eating park stepped until a guest waits at a shop, with guests walking the spine behind it.
const World &waitingWorld() {
  static const World world = [] {
    World stepped = test::eatingWorld();
    while (!anyWaiting(stepped)) {
      REQUIRE(stepped.Tick < WARM_UP_LIMIT);
      stepWorld(stepped);
    }
    return stepped;
  }();
  return world;
}

bool anyOn(const World &world, EntityKey carrier) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(
      guests, [&](EntityKey guest) { return recordOf(world, guest).At.Carrier == carrier; });
}

TEST_CASE("An edit changes nothing of a guest but its place, and a guest whose carrier survives "
          "the edit keeps its ground point") {
  const World &world = waitingWorld();
  // Deleting the spine takes away the places of the guests on it and of those waiting at the shops
  // beside it, whose connectors reached only the spine, and leaves the other guest paths.
  REQUIRE(anyOn(world, test::EATING_SPINE));
  CommandQueue commands;
  commands.push(DeletePath{test::EATING_SPINE});
  const World candidate = makeCandidate(world, commands);
  REQUIRE(parkGuests(candidate) == parkGuests(world));

  bool sawKept = false;
  bool sawMoved = false;
  for (const EntityKey guest : parkGuests(world)) {
    const GuestRecord was = recordOf(world, guest);
    GuestRecord record = recordOf(candidate, guest);
    CAPTURE(guest, was.At.Carrier, was.At.Distance, record.At.Carrier, record.At.Distance);
    if (test::findCarrier(test::guestNetwork(candidate), was.At.Carrier) != nullptr) {
      CHECK(record.Position == was.Position);
      sawKept = true;
    } else {
      sawMoved = true;
    }
    record.At = was.At;
    record.Position = was.Position;
    CHECK(record == was);
  }
  CHECK(sawKept);
  CHECK(sawMoved);
}

TEST_CASE("A cycle stepped from a world whose guest network has no carrier leaves it holding no "
          "guest") {
  World world = test::legsWorld();
  static_cast<void>(test::walkOntoFarLeg(world));
  CommandQueue cut;
  cut.push(DeletePath{test::NEAR_LEG});
  cut.push(DeletePath{test::FAR_LEG});
  stepWorld(world, cut);
  REQUIRE(test::guestNetwork(world).carriers().empty());
  REQUIRE_FALSE(parkGuests(world).empty());

  stepWorld(world);
  CHECK(parkGuests(world).empty());
}

} // namespace
} // namespace tpj
