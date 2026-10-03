#include "support/guest_parks.h"
#include "support/ledger_writes.h"
#include "support/park_worlds.h"
#include "support/route_edits.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
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

TEST_CASE("Every world randomized park edits with guests walking, waiting, and eating reach equals "
          "its copy") {
  bool sawGuests = false;
  runGuestEdits(73, [&](const World &world, bool /*edited*/) {
    REQUIRE(worldsEqual(copyWorld(world), world));
    sawGuests = sawGuests || !parkGuests(world).empty();
  });
  CHECK(sawGuests);
}

TEST_CASE("Every world randomized park edits with guests walking, waiting, and eating reach equals "
          "its save loaded and "
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
          "walking, waiting, and eating, once it has stepped a cycle, equals the world that queues "
          "the edit for that "
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

// Where the carrying rule puts a place held on the network before: carried over when that gives a
// place, else the nearest place to where it stood, else where it was.
Place carriedPlace(const Place &place, const Network &before, const Network &after) {
  if (const std::optional<Place> carried = carryOver(place, before, after)) {
    return *carried;
  }
  if (const std::optional<GroundPoint> ground = before.groundPoint(place)) {
    if (const std::optional<Place> nearest = after.nearestPlace(*ground)) {
      return *nearest;
    }
  }
  return place;
}

// Checks every guest of the candidate made from the world with the commands against the carrying
// rule, and that nothing else of its record but its Position changed.
void checkCarried(const World &world, const CommandQueue &commands) {
  const World candidate = makeCandidate(world, commands);
  REQUIRE(parkGuests(candidate) == parkGuests(world));
  const Network &before = test::guestNetwork(world);
  const Network &after = test::guestNetwork(candidate);
  for (const EntityKey guest : parkGuests(world)) {
    CAPTURE(guest);
    GuestRecord expected = recordOf(world, guest);
    expected.At = carriedPlace(expected.At, before, after);
    GuestRecord carried = recordOf(candidate, guest);
    carried.Position = expected.Position;
    CHECK(carried == expected);
  }
}

TEST_CASE("A candidate holds exactly its world's guests, each carried over to the new guest "
          "network, or else moved to its nearest place to where it stood, or else left where it "
          "was, with nothing else of its record changed but its Position") {
  const World &world = waitingWorld();
  CommandQueue commands;

  SECTION("a path crossing the spine splits edges guests walk, leaving every carrier's line") {
    REQUIRE(anyOn(world, test::EATING_SPINE));
    const AddPath crossing{PathKind::Guest, {{-10.0, 80.0}, {10.0, 80.0}}};
    REQUIRE(isAccepted(world, crossing));
    commands.push(crossing);
  }
  SECTION("deleting the spine retires the places of the guests on it, while other guest paths "
          "are left") {
    REQUIRE(anyOn(world, test::EATING_SPINE));
    commands.push(DeletePath{test::EATING_SPINE});
  }
  SECTION("moving the shop a guest waits at moves its connector's line") {
    const std::vector<EntityKey> guests = parkGuests(world);
    const auto waiting = std::ranges::find_if(guests, [&](EntityKey guest) {
      return recordOf(world, guest).Activity == GuestActivity::Waiting;
    });
    REQUIRE(waiting != guests.end());
    const EntityKey shop = recordOf(world, *waiting).Target;
    const std::vector<ParkBox> boxes = parkBoxes(world);
    const auto box = std::ranges::find(boxes, shop, &ParkBox::Key);
    REQUIRE(box != boxes.end());
    const MoveBox move{shop, Pose{box->At.X, box->At.Z + 1.0, box->At.FacingX, box->At.FacingZ}};
    REQUIRE(isAccepted(world, move));
    commands.push(move);
  }
  SECTION("deleting every guest path leaves no place to move to") {
    for (const ParkPath &path : parkPaths(world)) {
      if (path.Kind == PathKind::Guest) {
        commands.push(DeletePath{path.Key});
      }
    }
    REQUIRE(test::guestNetwork(makeCandidate(world, commands)).carriers().empty());
  }

  checkCarried(world, commands);
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
