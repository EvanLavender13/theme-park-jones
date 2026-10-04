#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
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
using test::recordOf;
using test::WALK_STEP;

// Route distance and a guest's walk add the same lengths in different orders.
constexpr double DISTANCE_TOLERANCE = 1e-9;
constexpr double NOT_A_DISTANCE = std::numeric_limits<double>::quiet_NaN();

// A stay that ends long after every test here has finished stepping.
constexpr uint64_t LONG_STAY = 100'000;

constexpr EntityKey GATE{1};

const Carrier &carrierOf(const Network &network, EntityKey key) {
  const Carrier *carrier = test::findCarrier(network, key);
  REQUIRE(carrier != nullptr);
  return *carrier;
}

TEST_CASE("An unfed guest's hunger never falls and never exceeds 1, and until it reaches 1 rises "
          "each cycle by the guest's one rate, which lies between HUNGER_RATE_MIN and "
          "HUNGER_RATE_MAX") {
  // A path so long that a guest walking out along it reaches no node, so chooses nothing and
  // stays, for longer than hunger takes to rise from 0 to 1 at the least rate. No shop feeds it.
  World world = test::resolvedWorld(test::ParkIntent{
      .Entrances = {test::northGate(GATE, 0.0)},
      .Paths = {test::guestPath(EntityKey{2}, {{0.0, test::northGateLineZ()}, {0.0, -120.0}})},
      .Boxes = {}});
  while (parkGuests(world).empty()) {
    REQUIRE(world.Tick < 1000);
    stepWorld(world);
  }
  const EntityKey guest = parkGuests(world).front();
  // A rise is the difference of two hungers, each rounded, so it differs from the rate by rounding.
  constexpr double RATE_TOLERANCE = 1e-12;
  double hunger = recordOf(world, guest).Hunger;
  std::optional<double> rate;
  bool full = false;
  for (uint64_t cycle = 0; cycle < static_cast<uint64_t>(2.0 / HUNGER_RATE_MIN) && !full; ++cycle) {
    stepWorld(world);
    const double next = recordOf(world, guest).Hunger;
    CAPTURE(world.Tick, hunger, next);
    REQUIRE(next >= hunger);
    REQUIRE(next <= 1.0);
    const double rise = next - hunger;
    if (!rate.has_value()) {
      rate = rise;
    }
    full = next == 1.0;
    if (full) {
      REQUIRE(rise <= rate.value_or(0.0) + RATE_TOLERANCE);
    } else {
      REQUIRE_THAT(rise, WithinAbs(rate.value_or(0.0), RATE_TOLERANCE));
    }
    hunger = next;
  }
  CHECK(full);
  CHECK(rate.value_or(0.0) >= HUNGER_RATE_MIN - RATE_TOLERANCE);
  CHECK(rate.value_or(0.0) <= HUNGER_RATE_MAX + RATE_TOLERANCE);
  stepWorld(world);
  CHECK(recordOf(world, guest).Hunger == 1.0);
}

// The shop line: an entrance's door joins a guest path running straight into the park, whose far
// end a shop's front door joins, so the guest network is one line from the entrance's anchor to the
// shop's, cut into three edges: the two connectors and the path. A depot supplies the shop over a
// backstage path behind it, so the shop offers meals.
constexpr EntityKey LINE_PATH{2};
constexpr EntityKey LINE_SHOP{3};
constexpr EntityKey LINE_BACKSTAGE{4};
constexpr EntityKey LINE_DEPOT{5};
constexpr double LINE_PATH_LENGTH = 6.0;
constexpr double LINE_DEPOT_X = 20.0;

World shopLineWorld() {
  const double pathEnd = test::northGateLineZ() - LINE_PATH_LENGTH;
  const double shopDepth = boxSize(BoxKind::Shop).Depth;
  const double backstageGap = test::gapWithinReach(PathKind::Backstage);
  // The shop faces the path's end from beyond it, its front door within reach of the path's end and
  // its back door within reach of the backstage path's start.
  const double shopZ = pathEnd - test::gapWithinReach(PathKind::Guest) - (shopDepth / 2.0);
  const double backstageZ = shopZ - (shopDepth / 2.0) - backstageGap;
  // The depot faces the backstage path's end from beyond it, its front door within reach of it.
  const double depotZ = backstageZ - backstageGap - (boxSize(BoxKind::Depot).Depth / 2.0);
  return test::resolvedWorld(test::ParkIntent{
      .Entrances = {test::northGate(GATE, 0.0)},
      .Paths = {test::guestPath(LINE_PATH, {{0.0, test::northGateLineZ()}, {0.0, pathEnd}}),
                ParkPath{.Key = LINE_BACKSTAGE,
                         .Kind = PathKind::Backstage,
                         .Points = {{0.0, backstageZ}, {LINE_DEPOT_X, backstageZ}}}},
      .Boxes = {ParkBox{.Key = LINE_SHOP, .Kind = BoxKind::Shop, .At = Pose{0.0, shopZ, 0.0, 1.0}},
                ParkBox{.Key = LINE_DEPOT,
                        .Kind = BoxKind::Depot,
                        .At = Pose{LINE_DEPOT_X, depotZ, 0.0, 1.0}}}});
}

// What the walking test reads of a guest before a cycle: its record, and its route distances from
// the entrance and from the shop.
struct LineGuest {
  GuestRecord Record;
  double Home = 0.0;
  double Shop = 0.0;
};

double shopDistance(const World &world, const Place &place) {
  const std::optional<RouteEntry> entry = test::routeFrom(test::routesAt(world, place), LINE_SHOP);
  REQUIRE(entry.has_value());
  return entry.value_or(RouteEntry{}).Distance;
}

double homeOf(const World &world, const Place &place) {
  const std::optional<double> home = test::homeDistance(world, place);
  REQUIRE(home.has_value());
  return home.value_or(NOT_A_DISTANCE);
}

// A wanderer on the line: the way its last whole step went, +1 toward the shop's end and -1 toward
// the entrance's, or 0 before its first, and the end it has since come within a step of, if any.
struct Wanderer {
  int Heading = 0;
  std::optional<int> NearEnd;
};

TEST_CASE("A walking guest covers WALK_STEP of route distance each cycle however the network is "
          "cut into edges, so a guest heading to a shop or home comes nearer it each cycle until "
          "it waits at the shop's anchor or leaves at the entrance's, and a wanderer on a line "
          "keeps going, turning at each end") {
  World world = shopLineWorld();
  const std::optional<Place> shopAnchor = test::anchorPlace(world, LINE_SHOP);
  REQUIRE(shopAnchor.has_value());
  const Place anchor = shopAnchor.value_or(Place{});
  // The line's length: the shop's anchor's distance from the entrance's.
  const double end = homeOf(world, anchor);
  // A guest whose stay is already over, at the shop's anchor, so that it heads home along the
  // whole line without waiting out a stay.
  static_cast<void>(addGuest(world, anchor, world.Tick));

  // The end of the line within a step of the distance from the entrance, -1 for the entrance's and
  // +1 for the shop's, or none.
  const auto nearEnd = [end](double home) -> std::optional<int> {
    if (home <= WALK_STEP + DISTANCE_TOLERANCE) {
      return -1;
    }
    if (home >= end - WALK_STEP - DISTANCE_TOLERANCE) {
      return 1;
    }
    return std::nullopt;
  };

  std::map<EntityKey, Wanderer> wanderers;
  bool sawShopApproach = false;
  bool sawShopArrival = false;
  bool sawHomeApproach = false;
  bool sawLeaving = false;
  bool sawTurnAtEntrance = false;
  bool sawTurnAtShop = false;
  while (!(sawShopApproach && sawShopArrival && sawHomeApproach && sawLeaving &&
           sawTurnAtEntrance && sawTurnAtShop)) {
    REQUIRE(world.Tick < 6000);
    std::map<EntityKey, LineGuest> before;
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      before.emplace(guest, LineGuest{.Record = record,
                                      .Home = homeOf(world, record.At),
                                      .Shop = shopDistance(world, record.At)});
    }
    const uint64_t tick = world.Tick;
    stepWorld(world);
    INFO("stepped " << tick);

    for (const auto &[guest, was] : before) {
      const std::optional<GuestRecord> now = guestRecord(world, guest);
      const GuestActivity activity = now.has_value() ? now->Activity : GuestActivity::Wandering;
      CAPTURE(guest, was.Home, was.Shop);
      if (was.Record.Activity != GuestActivity::Wandering || activity != GuestActivity::Wandering ||
          !now.has_value()) {
        wanderers.erase(guest);
      }
      switch (was.Record.Activity) {
      case GuestActivity::HeadingToShop:
        // A guest that chose otherwise at a node no longer heads there.
        if (!now.has_value() || now->Target != was.Record.Target ||
            (activity != GuestActivity::HeadingToShop && activity != GuestActivity::Waiting)) {
          break;
        }
        if (was.Shop > WALK_STEP) {
          CHECK(activity == GuestActivity::HeadingToShop);
          CHECK_THAT(shopDistance(world, now->At),
                     WithinAbs(was.Shop - WALK_STEP, DISTANCE_TOLERANCE));
          sawShopApproach = true;
        } else {
          CHECK(activity == GuestActivity::Waiting);
          CHECK(now->At == anchor);
          sawShopArrival = true;
        }
        break;
      case GuestActivity::HeadingHome:
        if (now.has_value() && activity != GuestActivity::HeadingHome) {
          break;
        }
        if (was.Home > WALK_STEP) {
          REQUIRE(now.has_value());
          CHECK_THAT(homeOf(world, now->At), WithinAbs(was.Home - WALK_STEP, DISTANCE_TOLERANCE));
          sawHomeApproach = true;
        } else {
          CHECK_FALSE(now.has_value());
          CHECK_FALSE(test::isLive(world, guest));
          sawLeaving = true;
        }
        break;
      case GuestActivity::Wandering: {
        if (!now.has_value() || activity != GuestActivity::Wandering) {
          break;
        }
        const double home = homeOf(world, now->At);
        Wanderer &wanderer = wanderers[guest];
        const std::optional<int> endWas = nearEnd(was.Home);
        // A cycle that starts and ends within a step of one end may turn there, covering its step
        // partly toward the end and partly back.
        if (endWas.has_value() && endWas == nearEnd(home)) {
          wanderer.NearEnd = endWas;
          break;
        }
        CAPTURE(home);
        CHECK_THAT(std::abs(home - was.Home), WithinAbs(WALK_STEP, DISTANCE_TOLERANCE));
        const int heading = home > was.Home ? 1 : -1;
        if (wanderer.Heading != 0 && heading != wanderer.Heading) {
          // It turned, which it does only at the end it was walking toward.
          CHECK(wanderer.NearEnd == wanderer.Heading);
          sawTurnAtShop = sawTurnAtShop || wanderer.Heading == 1;
          sawTurnAtEntrance = sawTurnAtEntrance || wanderer.Heading == -1;
        }
        wanderer.Heading = heading;
        wanderer.NearEnd.reset();
        break;
      }
      case GuestActivity::Waiting:
        break;
      }
    }
  }
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

TEST_CASE("A guest wanders until its StayUntil, and starts heading home only in a cycle stepping "
          "its StayUntil or later") {
  bool sawHeadingHome = false;
  runHomeward([&](World &world, CommandQueue &commands) {
    const uint64_t tick = world.Tick;
    std::map<EntityKey, GuestActivity> activities;
    for (const EntityKey guest : parkGuests(world)) {
      activities.emplace(guest, recordOf(world, guest).Activity);
    }
    stepWorld(world, commands);
    INFO("stepped " << tick);
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      CAPTURE(guest);
      if (tick < record.StayUntil) {
        CHECK(record.Activity == GuestActivity::Wandering);
      }
      const auto was = activities.find(guest);
      if (record.Activity != GuestActivity::HeadingHome ||
          (was != activities.end() && was->second == GuestActivity::HeadingHome)) {
        continue;
      }
      CHECK(tick >= record.StayUntil);
      sawHeadingHome = true;
    }
  });
  CHECK(sawHeadingHome);
}

// The crossing park: a spine leading in from the entrance's door, crossed halfway along by a second
// path, so a guest walking up the spine reaches a node with three steps besides its way back.
constexpr EntityKey SPINE{2};
constexpr EntityKey CROSSING{3};
constexpr double SPINE_LENGTH = 40.0;

World crossingWorld() {
  const double start = test::northGateLineZ();
  const double middle = start - (SPINE_LENGTH / 2.0);
  return test::resolvedWorld(test::ParkIntent{
      .Entrances = {test::northGate(GATE, 0.0)},
      .Paths = {test::guestPath(SPINE, {{0.0, start}, {0.0, start - SPINE_LENGTH}}),
                test::guestPath(CROSSING, {{-20.0, middle}, {20.0, middle}})},
      .Boxes = {}});
}

// The node the spine and the crossing share, and its distance along each.
struct Junction {
  uint32_t Node = 0;
  double OnSpine = 0.0;
  double OnCrossing = 0.0;
};

Junction junctionOf(const Network &network) {
  for (const CarrierStop &spine : carrierOf(network, SPINE).Stops) {
    for (const CarrierStop &crossing : carrierOf(network, CROSSING).Stops) {
      if (spine.Node == crossing.Node) {
        return Junction{
            .Node = spine.Node, .OnSpine = spine.Distance, .OnCrossing = crossing.Distance};
      }
    }
  }
  FAIL("the spine and the crossing share no node");
  return {};
}

TEST_CASE("At a node, a wandering guest never takes its way back when another step exists, and "
          "takes each other step about equally often") {
  World world = crossingWorld();
  const Junction junction = junctionOf(test::guestNetwork(world));
  // The fewest guests for which each step's expected count exceeds its tolerance of 4.5 standard
  // deviations and 1, so that a step never taken, or taken by every guest, lies outside it.
  constexpr uint64_t GUESTS = 47;
  constexpr double SHARE = 1.0 / 3.0;
  const double expected = static_cast<double>(GUESTS) * SHARE;
  const double tolerance =
      (4.5 * std::sqrt(static_cast<double>(GUESTS) * SHARE * (1.0 - SHARE))) + 1.0;
  REQUIRE(expected > tolerance);

  // Each guest stands on the spine half a cycle's walk short of the junction, walking toward it, so
  // its way back runs down the spine behind it. Each draws on its own key.
  const Place approach{SPINE, junction.OnSpine - (WALK_STEP / 2.0)};
  std::vector<EntityKey> guests;
  for (uint64_t index = 0; index < GUESTS; ++index) {
    guests.push_back(addGuest(world, approach, LONG_STAY));
  }
  stepWorld(world);

  uint64_t onward = 0;
  uint64_t west = 0;
  uint64_t east = 0;
  for (const EntityKey guest : guests) {
    const Place at = recordOf(world, guest).At;
    CAPTURE(guest, at.Carrier, at.Distance);
    if (at.Carrier == SPINE) {
      CHECK(at.Distance > junction.OnSpine);
      ++onward;
    } else {
      REQUIRE(at.Carrier == CROSSING);
      ++(at.Distance < junction.OnCrossing ? west : east);
    }
  }
  for (const uint64_t count : {onward, west, east}) {
    CAPTURE(onward, west, east);
    CHECK(std::abs(static_cast<double>(count) - expected) <= tolerance);
  }
}

} // namespace
} // namespace tpj
