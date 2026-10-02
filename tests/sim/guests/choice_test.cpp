#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/route_distance.h"
#include "sim/sim_math.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <stdint.h>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;
using test::recordOf;
using test::WALK_STEP;

// Route distance and a guest's walk add the same lengths in different orders.
constexpr double DISTANCE_TOLERANCE = 1e-9;

ChoiceOption optionOf(ChoiceKind kind, EntityKey shop, double score) {
  ChoiceOption option;
  option.Kind = kind;
  option.Shop = shop;
  option.Score = score;
  return option;
}

bool sameOptions(std::span<const ChoiceOption> left, std::span<const ChoiceOption> right) {
  return std::ranges::equal(left, right, [](const ChoiceOption &a, const ChoiceOption &b) {
    return a.Kind == b.Kind && a.Shop == b.Shop && test::sameBits(a.Relief, b.Relief) &&
           test::sameBits(a.Distance, b.Distance) && test::sameBits(a.Wait, b.Wait) &&
           test::sameBits(a.Commitment, b.Commitment) && test::sameBits(a.Score, b.Score) &&
           test::sameBits(a.Probability, b.Probability);
  });
}

// The softmax weights of the options' scores.
std::vector<double> weightsOf(std::span<const ChoiceOption> options) {
  double greatest = -std::numeric_limits<double>::infinity();
  for (const ChoiceOption &option : options) {
    greatest = std::max(greatest, option.Score);
  }
  std::vector<double> weights;
  for (const ChoiceOption &option : options) {
    weights.push_back(simExp((option.Score - greatest) / CHOICE_TEMPERATURE));
  }
  return weights;
}

// The hunger curve.

TEST_CASE("hungerCurve passes through each point of HUNGER_CURVE") {
  for (const CurvePoint &point : HUNGER_CURVE) {
    CAPTURE(point.X);
    CHECK(test::sameBits(hungerCurve(point.X), point.Y));
  }
}

TEST_CASE("Strictly between consecutive points a and b of HUNGER_CURVE, hungerCurve(h) is a.Y + "
          "(h - a.X) * (b.Y - a.Y) / (b.X - a.X), computed in that order") {
  // One hunger inside each stretch of the curve.
  constexpr std::array<double, 3> INSIDE{0.15, 0.5, 0.85};
  for (std::size_t index = 0; index + 1 < HUNGER_CURVE.size(); ++index) {
    const CurvePoint &a = HUNGER_CURVE.at(index);
    const CurvePoint &b = HUNGER_CURVE.at(index + 1);
    const double hunger = INSIDE.at(index);
    REQUIRE(hunger > a.X);
    REQUIRE(hunger < b.X);
    CAPTURE(hunger);
    const double expected = a.Y + ((hunger - a.X) * (b.Y - a.Y) / (b.X - a.X));
    CHECK(test::sameBits(hungerCurve(hunger), expected));
  }
}

TEST_CASE("hungerCurve gives its value at 0 for a hunger below 0, and its value at 1 for a hunger "
          "above 1") {
  CHECK(test::sameBits(hungerCurve(-0.25), hungerCurve(0.0)));
  CHECK(test::sameBits(hungerCurve(-std::numeric_limits<double>::infinity()), hungerCurve(0.0)));
  CHECK(test::sameBits(hungerCurve(1.25), hungerCurve(1.0)));
  CHECK(test::sameBits(hungerCurve(std::numeric_limits<double>::infinity()), hungerCurve(1.0)));
}

// The softmax pick.

TEST_CASE("softmaxPick sets each option's Probability to its weight simExp((Score - m) / "
          "CHOICE_TEMPERATURE) over the weights' total, changes nothing else, and returns "
          "drawPick's index over the weights") {
  const EntityKey shop{7};
  // Scores near one another; a score far above the rest, with a tie below it; and a lone option.
  const std::vector<std::vector<ChoiceOption>> lists{
      {optionOf(ChoiceKind::Offer, shop, 1.3), optionOf(ChoiceKind::Offer, EntityKey{9}, -0.2),
       optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE)},
      {optionOf(ChoiceKind::Offer, shop, CARRY_ON_SCORE),
       optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE),
       optionOf(ChoiceKind::HeadHome, NULL_KEY, HEAD_HOME_SCORE)},
      {optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE)}};
  for (const std::vector<ChoiceOption> &list : lists) {
    const std::vector<double> weights = weightsOf(list);
    double total = 0.0;
    for (const double weight : weights) {
      total += weight;
    }
    std::vector<ChoiceOption> expected = list;
    for (std::size_t index = 0; index < list.size(); ++index) {
      expected.at(index).Probability = weights.at(index) / total;
    }
    for (uint64_t index = 0; index < 3; ++index) {
      const DrawKey key{1, EntityKey{40}, hashName("guest-choice"), 100, index};
      std::vector<ChoiceOption> options = list;
      const size_t picked = softmaxPick(key, options);
      CAPTURE(list.size(), index);
      CHECK(sameOptions(options, expected));
      CHECK(picked == drawPick(key, std::span<const double>(weights)));
    }
  }
}

TEST_CASE("Over 10000 keys differing only in index, softmaxPick picks each option within 4.5 "
          "standard deviations and 1 of its Probability's share") {
  constexpr uint64_t KEYS = 10000;
  std::vector<ChoiceOption> options{optionOf(ChoiceKind::Offer, EntityKey{7}, 0.8),
                                    optionOf(ChoiceKind::Offer, EntityKey{9}, 0.3),
                                    optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE)};
  std::vector<uint64_t> counts(options.size(), 0);
  for (uint64_t index = 0; index < KEYS; ++index) {
    const size_t picked =
        softmaxPick(DrawKey{1, EntityKey{40}, hashName("guest-choice"), 100, index}, options);
    REQUIRE(picked < options.size());
    ++counts.at(picked);
  }
  for (std::size_t index = 0; index < options.size(); ++index) {
    const double p = options.at(index).Probability;
    const double expected = static_cast<double>(KEYS) * p;
    const double tolerance = (4.5 * std::sqrt(static_cast<double>(KEYS) * p * (1.0 - p))) + 1.0;
    CAPTURE(index, p, counts.at(index));
    CHECK(std::abs(static_cast<double>(counts.at(index)) - expected) <= tolerance);
  }
}

TEST_CASE("softmaxPick throws std::invalid_argument, leaving the options unchanged, for no options "
          "or a Score that is not finite") {
  const DrawKey key{1, EntityKey{40}, hashName("guest-choice"), 100, 0};
  std::vector<ChoiceOption> none;
  CHECK_THROWS_AS(softmaxPick(key, none), std::invalid_argument);

  for (const double bad :
       {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()}) {
    CAPTURE(bad);
    // The bad score comes after a good one, so a pick that sets probabilities as it goes would
    // already have changed the first.
    std::vector<ChoiceOption> options{optionOf(ChoiceKind::Offer, EntityKey{7}, 1.0),
                                      optionOf(ChoiceKind::Offer, EntityKey{9}, bad),
                                      optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE)};
    for (ChoiceOption &option : options) {
      option.Probability = 0.25;
    }
    const std::vector<ChoiceOption> before = options;
    CHECK_THROWS_AS(softmaxPick(key, options), std::invalid_argument);
    CHECK(sameOptions(options, before));
  }
}

// Choices in the eating park.

// The cycle that closes the gate, once six guests have arrived.
constexpr uint64_t CLOSE_GATE = 6 * ARRIVAL_INTERVAL;
// Long enough for guests to head to shops, wait, and eat.
constexpr uint64_t EATING_CYCLES = 20 * ARRIVAL_INTERVAL;
// More cycles than the last of those guests' stays can last, and its walk to a node.
constexpr uint64_t LAST_CYCLE = CLOSE_GATE + STAY_MAX + 1000;

// What a check reads of the eating park before a cycle: the tick it steps, its guests' records,
// the offers, which shops publish as they step, and the guests whose visits have come back.
struct BeforeCycle {
  uint64_t Tick = 0;
  std::map<EntityKey, GuestRecord> Guests;
  test::ParkOffers Offers;
  std::set<EntityKey> VisitsBack;
};

// Steps the eating park until isDone(world), with no command except in two cycles: the one
// stepping CLOSE_GATE deletes the gate path, so the park admits no more guests and steps quickly,
// and the first in which a guest's stay is over draws it again, so guests can head home. check
// sees what was read before every other cycle and the world after it. A cycle with no command
// runs no resolver, so the networks and route distance after it are those before it.
template <typename Done, typename Check> void runEatingPark(Done isDone, Check check) {
  World world = test::eatingWorld();
  bool reopened = false;
  while (!isDone(world)) {
    REQUIRE(world.Tick < LAST_CYCLE);
    CommandQueue commands;
    if (world.Tick == CLOSE_GATE) {
      commands.push(DeletePath{test::EATING_GATE_PATH});
    } else if (!reopened && world.Tick > CLOSE_GATE) {
      const std::vector<EntityKey> guests = parkGuests(world);
      reopened = std::ranges::any_of(
          guests, [&](EntityKey guest) { return world.Tick >= recordOf(world, guest).StayUntil; });
      if (reopened) {
        commands.push(AddPath{PathKind::Guest, test::eatingGatePoints()});
      }
    }
    if (!commands.empty()) {
      stepWorld(world, commands);
      REQUIRE(test::eatingGatePath(world).has_value() == reopened);
      continue;
    }
    BeforeCycle before{
        .Tick = world.Tick, .Guests = {}, .Offers = test::parkOffers(world), .VisitsBack = {}};
    for (const EntityKey guest : parkGuests(world)) {
      before.Guests.emplace(guest, recordOf(world, guest));
      if (unitsHeld<GuestVisits>(world, guest, guest) > 0) {
        before.VisitsBack.insert(guest);
      }
    }
    stepWorld(world);
    INFO("stepped " << before.Tick);
    check(before, world);
  }
}

bool isEatingRunOver(const World &world) { return world.Tick >= EATING_CYCLES; }

bool anyHeadingHome(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(guests, [&](EntityKey guest) {
    return recordOf(world, guest).Activity == GuestActivity::HeadingHome;
  });
}

// Whether the guest, before a cycle, must choose because it has lost its way: it is heading to a
// shop whose offer is not reachable at its place, or heading home with no entrance entry there.
bool losesWay(const World &after, const test::ParkOffers &offers, const GuestRecord &was) {
  const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(after, was.At);
  return (was.Activity == GuestActivity::HeadingToShop &&
          !test::isOfferReachable(offers, routes, was.Target)) ||
         (was.Activity == GuestActivity::HeadingHome &&
          !test::homeEntry(after, routes).has_value());
}

// The entity anchored to the node at the place, or NULL_KEY.
EntityKey anchorAt(const World &world, const Place &place) {
  const std::optional<NetworkPosition> position = test::guestNetwork(world).resolve(place);
  REQUIRE(position.has_value());
  const NetworkPosition resolved = position.value_or(NetworkPosition{});
  const auto *node = std::get_if<NodePosition>(&resolved);
  REQUIRE(node != nullptr);
  return test::guestNetwork(world).nodeAnchor(node->Node);
}

TEST_CASE("A guest chooses where it stands when it starts its walk at a node or its visit has come "
          "back, picking by softmaxPick keyed by its first choice of the cycle over the options "
          "Choice lists, and its Activity and Target follow the pick") {
  bool sawOfferPicked = false;
  bool sawCarryOnPicked = false;
  bool sawHeadHomePicked = false;
  bool sawPickAtAnchor = false;
  runEatingPark(anyHeadingHome, [&](const BeforeCycle &before, const World &after) {
    const uint64_t tick = before.Tick;
    for (const auto &[guest, was] : before.Guests) {
      const bool visitBack = before.VisitsBack.contains(guest);
      if (!visitBack &&
          (was.Activity == GuestActivity::Waiting || !test::isAtNode(after, was.At))) {
        continue;
      }
      CAPTURE(guest, visitBack);
      // A guest whose visit came back, or that dropped its target, chooses with none.
      const EntityKey target =
          visitBack || losesWay(after, before.Offers, was) ? NULL_KEY : was.Target;
      const std::optional<GuestRecord> now = guestRecord(after, guest);
      if (!now.has_value()) {
        // It chose, and picked heading home at an entrance's anchor, where it left.
        CHECK(tick >= was.StayUntil);
        CHECK(test::isEntrance(after, anchorAt(after, was.At)));
        sawHeadHomePicked = true;
        continue;
      }
      const GuestRecord record = now.value_or(GuestRecord{});
      const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(after, was.At);
      // Hunger changes in a step only before the guest chooses, by its rise and a meal, so its
      // hunger after the cycle is the hunger it chose with. Every edge of the eating park is
      // longer than a cycle's walk, so a guest chooses at most once in a cycle with no edit, and
      // its draw has index 0.
      const ChoiceOption picked =
          test::checkPick(after, guest, tick,
                          test::choiceOptions(after, before.Offers, routes, record.Hunger, target,
                                              tick >= was.StayUntil),
                          routes);
      switch (picked.Kind) {
      case ChoiceKind::Offer:
        sawOfferPicked = true;
        sawPickAtAnchor = sawPickAtAnchor || record.Activity == GuestActivity::Waiting;
        break;
      case ChoiceKind::CarryOn:
        sawCarryOnPicked = true;
        break;
      case ChoiceKind::HeadHome:
        sawHeadHomePicked = true;
        break;
      }
    }
  });

  // A guest standing at the junction of the gate path and the spine once its stay is over, which
  // the eating run's guests rarely are when they start a walk.
  World world = test::eatingWorld();
  test::stepUntil(world, test::FIRST_ARRIVAL + 1);
  const Place junction{test::EATING_GATE_PATH, 4.0};
  REQUIRE(test::isAtNode(world, junction));
  const EntityKey late = addGuest(world, junction, world.Tick);
  const uint64_t tick = world.Tick;
  const test::ParkOffers offers = test::parkOffers(world);
  stepWorld(world);
  const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(world, junction);
  const ChoiceOption picked = test::checkPick(
      world, late, tick,
      test::choiceOptions(world, offers, routes, recordOf(world, late).Hunger, NULL_KEY, true),
      routes);
  sawHeadHomePicked = sawHeadHomePicked || picked.Kind == ChoiceKind::HeadHome;

  CHECK(sawOfferPicked);
  CHECK(sawCarryOnPicked);
  CHECK(sawHeadHomePicked);
  CHECK(sawPickAtAnchor);
}

TEST_CASE("A guest that walks to a node with no distance left does not choose there in that "
          "cycle: it keeps its Activity and Target") {
  runEatingPark(isEatingRunOver, [&](const BeforeCycle &before, const World &after) {
    for (const auto &[guest, was] : before.Guests) {
      if (was.Activity == GuestActivity::Waiting || test::isAtNode(after, was.At) ||
          losesWay(after, before.Offers, was)) {
        continue;
      }
      const std::optional<GuestRecord> now = guestRecord(after, guest);
      if (!now.has_value() || now.value_or(GuestRecord{}).Activity == GuestActivity::Waiting ||
          !test::isAtNode(after, now.value_or(GuestRecord{}).At)) {
        continue;
      }
      CAPTURE(guest);
      CHECK(now.value_or(GuestRecord{}).Activity == was.Activity);
      CHECK(now.value_or(GuestRecord{}).Target == was.Target);
    }
  });
}

TEST_CASE("A guest heading to a shop that keeps its target comes WALK_STEP nearer the shop by "
          "route distance each cycle, until it waits at the shop's anchor") {
  bool sawApproach = false;
  bool sawArrival = false;
  runEatingPark(isEatingRunOver, [&](const BeforeCycle &before, const World &after) {
    for (const auto &[guest, was] : before.Guests) {
      const std::optional<GuestRecord> now = guestRecord(after, guest);
      if (was.Activity != GuestActivity::HeadingToShop || !now.has_value() ||
          now.value().Target != was.Target ||
          (now.value().Activity != GuestActivity::HeadingToShop &&
           now.value().Activity != GuestActivity::Waiting)) {
        continue;
      }
      CAPTURE(guest);
      const std::optional<RouteEntry> from =
          test::routeFrom(test::routesAt(after, was.At), was.Target);
      const std::optional<RouteEntry> to =
          test::routeFrom(test::routesAt(after, now.value().At), was.Target);
      REQUIRE(from.has_value());
      REQUIRE(to.has_value());
      const double previous = from.value_or(RouteEntry{}).Distance;
      const double distance = to.value_or(RouteEntry{}).Distance;
      CAPTURE(previous);
      if (previous > WALK_STEP) {
        CHECK(now.value().Activity == GuestActivity::HeadingToShop);
        CHECK_THAT(distance, WithinAbs(previous - WALK_STEP, DISTANCE_TOLERANCE));
        sawApproach = true;
      } else {
        CHECK(now.value().Activity == GuestActivity::Waiting);
        CHECK(distance == 0.0);
        sawArrival = true;
      }
    }
  });
  CHECK(sawApproach);
  CHECK(sawArrival);
}

// Steps the eating park until a guest is heading to a shop along a path, not a connector, with more
// than two cycles' walk left, and gives that guest's key.
EntityKey headingAlongPath(World &world) {
  for (int cycle = 0; cycle < 3000; ++cycle) {
    stepWorld(world);
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      if (record.Activity != GuestActivity::HeadingToShop ||
          (record.At.Carrier != test::EATING_SPINE && record.At.Carrier != test::EATING_CROSS)) {
        continue;
      }
      const std::optional<RouteEntry> entry =
          test::routeFrom(test::routesAt(world, record.At), record.Target);
      if (entry.has_value() && entry.value_or(RouteEntry{}).Distance > 2.0 * WALK_STEP) {
        return guest;
      }
    }
  }
  FAIL("no guest headed to a shop along a path");
  return NULL_KEY;
}

// A guest heading to a shop, just after an edit took its target's offer away.
struct LostTarget {
  World Park;
  EntityKey Guest = NULL_KEY;
  EntityKey Target = NULL_KEY;
};

// Cuts the eating park under a guest heading to a shop, deleting its target or the supply route.
LostTarget loseTarget(bool deleteShop) {
  World world = test::eatingWorld();
  const EntityKey guest = headingAlongPath(world);
  const EntityKey target = recordOf(world, guest).Target;
  CommandQueue cut;
  if (deleteShop) {
    cut.push(DeleteBox{target});
  } else {
    cut.push(DeletePath{test::EATING_BACKSTAGE});
  }
  stepWorld(world, cut);
  REQUIRE_FALSE(
      test::isOfferReachable(world, test::routesAt(world, recordOf(world, guest).At), target));
  REQUIRE(recordOf(world, guest).Activity == GuestActivity::HeadingToShop);
  REQUIRE(recordOf(world, guest).Target == target);
  return LostTarget{.Park = std::move(world), .Guest = guest, .Target = target};
}

// Checks that the guest that lost its target chooses in the next cycle, where it stands, as a guest
// with none.
void checkDrop(bool deleteShop) {
  LostTarget lost = loseTarget(deleteShop);
  CAPTURE(lost.Guest, lost.Target);
  const uint64_t tick = lost.Park.Tick;
  const GuestRecord was = recordOf(lost.Park, lost.Guest);
  const test::ParkOffers offers = test::parkOffers(lost.Park);
  stepWorld(lost.Park);
  const GuestRecord record = recordOf(lost.Park, lost.Guest);
  CHECK(record.Target != lost.Target);
  // A cycle with no command leaves route distance as it was. The guest's hunger after the cycle is
  // the hunger it chose with, since it rises before the guest walks.
  const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(lost.Park, was.At);
  (void)test::checkPick(lost.Park, lost.Guest, tick,
                        test::choiceOptions(lost.Park, offers, routes, record.Hunger, NULL_KEY,
                                            tick >= was.StayUntil),
                        routes);
}

TEST_CASE("A guest heading to a shop whose offer stops being reachable chooses in the next cycle "
          "with no target, and its Activity and Target follow the pick") {
  SECTION("the shop's supply route is cut, so its offer says no meals") { checkDrop(false); }
  SECTION("the shop is deleted, so route distance has no entry for it") { checkDrop(true); }
}

TEST_CASE("A guest heading home that has no entrance entry at its place chooses in that cycle, and "
          "with carrying on its only option, wanders with no target") {
  World world = test::legsWorld();
  const EntityKey guest = test::strandPastStay(world, test::stepWith);
  // Stranded past its stay with the near leg drawn again, it heads home from the far leg's dead
  // end, walking back toward the junction.
  const auto isHeadingHomeOnFarLeg = [&]() {
    const GuestRecord record = recordOf(world, guest);
    return record.Activity == GuestActivity::HeadingHome && record.At.Carrier == test::FAR_LEG &&
           record.At.Distance > 2.0 * WALK_STEP;
  };
  for (int cycle = 0; cycle < 2000 && !isHeadingHomeOnFarLeg(); ++cycle) {
    stepWorld(world);
  }
  REQUIRE(isHeadingHomeOnFarLeg());
  // The near leg drawn again has a new key.
  const std::vector<ParkPath> paths = parkPaths(world);
  const auto nearLeg =
      std::ranges::find_if(paths, [](const ParkPath &path) { return path.Key != test::FAR_LEG; });
  REQUIRE(nearLeg != paths.end());
  CommandQueue cut;
  cut.push(DeletePath{nearLeg->Key});
  stepWorld(world, cut);
  REQUIRE(recordOf(world, guest).Activity == GuestActivity::HeadingHome);
  REQUIRE_FALSE(test::homeDistance(world, recordOf(world, guest).At).has_value());

  // With no offer in the legs park and no entrance entry, carrying on is all it would weigh.
  const std::optional<std::vector<ChoiceOption>> options = guestOptions(world, guest);
  REQUIRE(options.has_value());
  REQUIRE(options.value_or(std::vector<ChoiceOption>{}).size() == 1);
  REQUIRE(options.value_or(std::vector<ChoiceOption>{}).front().Kind == ChoiceKind::CarryOn);

  stepWorld(world);
  const GuestRecord record = recordOf(world, guest);
  CHECK(record.Activity == GuestActivity::Wandering);
  CHECK(record.Target == NULL_KEY);
}

} // namespace
} // namespace tpj
