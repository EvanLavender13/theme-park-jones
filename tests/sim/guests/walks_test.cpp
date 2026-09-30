#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;
using test::FIRST_ARRIVAL;
using test::GuestStep;
using test::recordOf;
using test::WALK_STEP;

// Route distance and a guest's walk add the same lengths in different orders.
constexpr double DISTANCE_TOLERANCE = 1e-9;
constexpr double NOT_A_DISTANCE = std::numeric_limits<double>::quiet_NaN();

constexpr EntityKey GATE{1};

const Carrier &carrierOf(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  REQUIRE(found != network.carriers().end());
  return *found;
}

// The first guest of the new park, from its arrival until it leaves. The new park is a line: the
// entrance's connector and then a path ending in a dead end, so the least entrance distance at a
// guest's place is how far along the line it stands.
struct NewParkRun {
  EntityKey Guest = NULL_KEY;
  GuestRecord Arrived;
  double ArrivedHome = 0.0;
  // The entrance distance at the path's far end.
  double End = 0.0;
  std::vector<GuestStep> Trace;
};

const NewParkRun &newParkRun() {
  static const NewParkRun run = [] {
    World world = makeNewPark(1);
    resolveWorld(world);
    test::stepUntil(world, FIRST_ARRIVAL);
    NewParkRun result;
    result.Guest = EntityKey{world.nextKey()};
    stepWorld(world);
    result.Arrived = recordOf(world, result.Guest);
    result.ArrivedHome = test::homeDistance(world, result.Arrived.At).value_or(NOT_A_DISTANCE);
    const std::vector<ParkPath> paths = parkPaths(world);
    REQUIRE(paths.size() == 1);
    const Carrier &path = carrierOf(test::guestNetwork(world), paths.front().Key);
    REQUIRE_FALSE(path.Points.empty());
    result.End = test::homeDistance(world, Place{path.Key, path.Points.back().Distance})
                     .value_or(NOT_A_DISTANCE);
    result.Trace = test::traceGuest(world, result.Guest, STAY_MAX + 2000);
    return result;
  }();
  return run;
}

double homeOf(const GuestStep &step) {
  REQUIRE(step.Home.has_value());
  return step.Home.value_or(NOT_A_DISTANCE);
}

TEST_CASE("A guest's Hunger rises each cycle by its own rate, drawn when it arrived, and never "
          "exceeds 1") {
  // A path so long that a guest walking out until its stay ends, and back, stays longer than
  // hunger takes to rise from 0 to 1 at the least rate.
  World world = test::resolvedWorld(
      test::ParkIntent{.Entrances = {test::northGate(GATE, 0.0)},
                       .Paths = {test::guestPath(EntityKey{2}, {{0.0, 123.0}, {0.0, -120.0}})},
                       .Boxes = {}});
  test::stepUntil(world, FIRST_ARRIVAL);
  const EntityKey guest{world.nextKey()};
  stepWorld(world);
  const double rate = test::drawnHungerRate(world, guest, FIRST_ARRIVAL);
  double hunger = recordOf(world, guest).Hunger;
  const auto cycles = static_cast<uint64_t>(1.0 / HUNGER_RATE_MIN) + 10;
  for (uint64_t cycle = 0; cycle < cycles; ++cycle) {
    stepWorld(world);
    const double next = recordOf(world, guest).Hunger;
    CAPTURE(world.Tick, hunger);
    REQUIRE(next == std::min(1.0, hunger + rate));
    hunger = next;
  }
  CHECK(hunger == 1.0);
}

TEST_CASE("A wandering guest walks WALK_STEP each cycle along the carriers, keeping its direction "
          "until a dead end, and turns back at the end of its path and at its entrance's anchor") {
  const NewParkRun &run = newParkRun();
  double previous = run.ArrivedHome;
  bool outward = true;
  bool turnedAtEnd = false;
  bool turnedAtAnchor = false;
  for (const GuestStep &step : run.Trace) {
    if (step.Stepped >= run.Arrived.StayUntil) {
      break;
    }
    CAPTURE(step.Stepped, previous, outward);
    // Walking on past a dead end would take it beyond the line, so it turns there and walks the
    // rest of the step back.
    double expected = outward ? previous + WALK_STEP : previous - WALK_STEP;
    if (expected > run.End) {
      expected = (2.0 * run.End) - expected;
      outward = false;
      turnedAtEnd = true;
    } else if (expected < 0.0) {
      expected = -expected;
      outward = true;
      turnedAtAnchor = true;
    }
    const double home = homeOf(step);
    REQUIRE_THAT(home, WithinAbs(expected, DISTANCE_TOLERANCE));
    previous = home;
  }
  CHECK(turnedAtEnd);
  CHECK(turnedAtAnchor);
}

// The homeward run: the legs park, stepped by step(world, commands), strands a guest on the far leg
// past its stay, draws the near leg again, and steps on until that guest has gone home.
template <typename Step> void runHomeward(Step step) {
  World world = test::legsWorld();
  const EntityKey stranded = test::strandPastStay(world, step);
  for (int cycle = 0; cycle < 3000 && test::isLive(world, stranded); ++cycle) {
    CommandQueue none;
    step(world, none);
  }
  CHECK_FALSE(test::isLive(world, stranded));
}

TEST_CASE("A guest wanders until its StayUntil, and heads home only by a choice made at or after "
          "it that picks heading home") {
  bool sawHeadingHome = false;
  runHomeward([&](World &world, CommandQueue &commands) {
    const uint64_t tick = world.Tick;
    stepWorld(world, commands);
    INFO("stepped " << tick);
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      CAPTURE(guest);
      if (tick < record.StayUntil) {
        CHECK(record.Activity == GuestActivity::Wandering);
      }
      if (record.Activity != GuestActivity::HeadingHome) {
        continue;
      }
      REQUIRE(record.LastChoice.has_value());
      const GuestChoice choice = record.LastChoice.value_or(GuestChoice{});
      CHECK(choice.Tick >= record.StayUntil);
      REQUIRE(choice.Picked < choice.Options.size());
      CHECK(choice.Options.at(choice.Picked).Kind == ChoiceKind::HeadHome);
      sawHeadingHome = true;
    }
  });
  CHECK(sawHeadingHome);
}

TEST_CASE(
    "A guest heading home with an entrance entry at its place comes WALK_STEP nearer the "
    "entrance each cycle, and leaves the park in the cycle it reaches the entrance's anchor") {
  int approached = 0;
  int left = 0;
  runHomeward([&](World &world, CommandQueue &commands) {
    // An edit changes route distance, so only a cycle with none compares distances across it.
    const bool edited = !commands.empty();
    std::map<EntityKey, double> homeBefore;
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      if (record.Activity == GuestActivity::HeadingHome) {
        CAPTURE(guest);
        const std::optional<double> home = test::homeDistance(world, record.At);
        REQUIRE(home.has_value());
        homeBefore.emplace(guest, home.value_or(NOT_A_DISTANCE));
      }
    }
    const uint64_t tick = world.Tick;
    stepWorld(world, commands);
    if (edited) {
      return;
    }
    INFO("stepped " << tick);
    for (const auto &[guest, previous] : homeBefore) {
      CAPTURE(guest, previous);
      if (previous > WALK_STEP) {
        const GuestRecord now = recordOf(world, guest);
        REQUIRE_THAT(test::homeDistance(world, now.At).value_or(NOT_A_DISTANCE),
                     WithinAbs(previous - WALK_STEP, DISTANCE_TOLERANCE));
        ++approached;
      } else {
        CHECK_FALSE(guestRecord(world, guest).has_value());
        CHECK_FALSE(test::isLive(world, guest));
        ++left;
      }
    }
  });
  CHECK(approached > 0);
  CHECK(left > 0);
}

// The crossing park: a spine from the entrance's door, crossed halfway along by a second path, so
// a guest walking up the spine reaches a node with three steps besides its way back.
constexpr EntityKey SPINE{2};
constexpr EntityKey CROSSING{3};

World crossingWorld() {
  return test::resolvedWorld(
      test::ParkIntent{.Entrances = {test::northGate(GATE, 0.0)},
                       .Paths = {test::guestPath(SPINE, {{0.0, 123.0}, {0.0, 83.0}}),
                                 test::guestPath(CROSSING, {{-20.0, 103.0}, {20.0, 103.0}})},
                       .Boxes = {}});
}

struct Junction {
  uint32_t Node = 0;
  double OnSpine = 0.0;
};

Junction junctionOf(const Network &network) {
  for (const CarrierStop &spine : carrierOf(network, SPINE).Stops) {
    for (const CarrierStop &crossing : carrierOf(network, CROSSING).Stops) {
      if (spine.Node == crossing.Node) {
        return Junction{.Node = spine.Node, .OnSpine = spine.Distance};
      }
    }
  }
  FAIL("the spine and the crossing share no node");
  return {};
}

// The node's steps in the order a guest lists them: for each edge in edges() order, the step from
// its From stop when it starts at the node, and then the step from its To stop when it ends there.
std::vector<RouteStep> stepsAt(const Network &network, uint32_t node) {
  std::vector<RouteStep> steps;
  for (const NetworkEdge &edge : network.edges()) {
    if (edge.From == node) {
      steps.push_back(RouteStep{edge.Carrier, edge.FromDistance, edge.ToDistance});
    }
    if (edge.To == node) {
      steps.push_back(RouteStep{edge.Carrier, edge.ToDistance, edge.FromDistance});
    }
  }
  return steps;
}

TEST_CASE("At a node, a wandering guest leaves by the step drawPick picks from a weight of 1 for "
          "each of the node's steps other than its way back") {
  World world = crossingWorld();
  const Network &network = test::guestNetwork(world);
  const Junction junction = junctionOf(network);
  std::vector<RouteStep> others = stepsAt(network, junction.Node);
  // A guest comes up the spine from the entrance, so its way back runs down the spine.
  const auto back = std::ranges::find_if(others, [&](const RouteStep &step) {
    return step.Carrier == SPINE && step.From == junction.OnSpine && step.To < step.From;
  });
  REQUIRE(back != others.end());
  others.erase(back);
  REQUIRE(others.size() == 3);
  const std::vector<uint64_t> weights(others.size(), 1);
  const EntityKey connector = connectorKey(GATE, Face::Front);

  // The first two guests, each checked in the cycle it leaves the junction.
  std::vector<EntityKey> approaching;
  for (const uint64_t arrival : {FIRST_ARRIVAL, FIRST_ARRIVAL + ARRIVAL_INTERVAL}) {
    test::stepUntil(world, arrival);
    approaching.push_back(EntityKey{world.nextKey()});
    stepWorld(world);
  }
  for (int cycle = 0; cycle < 1500 && !approaching.empty(); ++cycle) {
    const uint64_t stepped = world.Tick;
    stepWorld(world);
    std::erase_if(approaching, [&](EntityKey guest) {
      const Place at = recordOf(world, guest).At;
      if (at.Carrier == connector || (at.Carrier == SPINE && at.Distance <= junction.OnSpine)) {
        return false;
      }
      CAPTURE(guest, stepped, at.Carrier, at.Distance);
      const DrawKey key{world.Seed, guest, hashName("guest-wander"), stepped, 0};
      const RouteStep &taken = others.at(drawPick(key, weights));
      CHECK(at.Carrier == taken.Carrier);
      CHECK((at.Distance - taken.From) * (taken.To - taken.From) > 0.0);
      CHECK(std::abs(at.Distance - taken.From) <= WALK_STEP + DISTANCE_TOLERANCE);
      return true;
    });
  }
  CHECK(approaching.empty());
}

TEST_CASE("A guest with no entrance entry at its place wanders on past its stay, and once an entry "
          "appears heads for the entrance and leaves") {
  World world = test::legsWorld();
  const EntityKey guest = test::walkOntoFarLeg(world);
  CommandQueue cut;
  cut.push(DeletePath{test::NEAR_LEG});
  stepWorld(world, cut);

  // Cut off from the entrance, it walks the far leg past the end of its stay.
  const uint64_t stayUntil = recordOf(world, guest).StayUntil;
  while (world.Tick <= stayUntil + ARRIVAL_INTERVAL) {
    const Place before = recordOf(world, guest).At;
    stepWorld(world);
    const GuestRecord record = recordOf(world, guest);
    CAPTURE(world.Tick);
    REQUIRE(record.At.Carrier == test::FAR_LEG);
    REQUIRE(record.At != before);
    REQUIRE_FALSE(test::homeDistance(world, record.At).has_value());
  }
  CHECK(recordOf(world, guest).Activity == GuestActivity::Wandering);

  CommandQueue rejoin;
  rejoin.push(AddPath{PathKind::Guest, test::nearLegPoints()});
  stepWorld(world, rejoin);
  REQUIRE(test::homeDistance(world, recordOf(world, guest).At).has_value());
  // It picks heading home at the next node it reaches.
  bool headingHome = false;
  for (int cycle = 0; cycle < 2000 && !headingHome; ++cycle) {
    stepWorld(world);
    headingHome = recordOf(world, guest).Activity == GuestActivity::HeadingHome;
  }
  REQUIRE(headingHome);
  std::optional<double> home = test::homeDistance(world, recordOf(world, guest).At);
  REQUIRE(home.has_value());
  bool left = false;
  for (int cycle = 0; cycle < 2000 && !left; ++cycle) {
    const double previous = home.value_or(NOT_A_DISTANCE);
    stepWorld(world);
    CAPTURE(world.Tick, previous);
    if (previous > WALK_STEP) {
      home = test::homeDistance(world, recordOf(world, guest).At);
      REQUIRE_THAT(home.value_or(NOT_A_DISTANCE),
                   WithinAbs(previous - WALK_STEP, DISTANCE_TOLERANCE));
    } else {
      CHECK_FALSE(guestRecord(world, guest).has_value());
      left = true;
    }
  }
  CHECK(left);
}

} // namespace
} // namespace tpj
