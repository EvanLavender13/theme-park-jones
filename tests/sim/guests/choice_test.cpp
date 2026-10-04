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
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;
using test::recordOf;
using test::WALK_STEP;

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

// The hunger curve.

TEST_CASE("hungerCurve passes through each point of HUNGER_CURVE") {
  for (const CurvePoint &point : HUNGER_CURVE) {
    CAPTURE(point.X);
    CHECK(test::sameBits(hungerCurve(point.X), point.Y));
  }
}

TEST_CASE("hungerCurve never falls as hunger rises, and between consecutive points of "
          "HUNGER_CURVE lies between their Ys") {
  // Each point's X, and halfway between each two consecutive points, in ascending hunger.
  double previous = hungerCurve(HUNGER_CURVE.front().X);
  for (std::size_t index = 0; index + 1 < HUNGER_CURVE.size(); ++index) {
    const CurvePoint &a = HUNGER_CURVE.at(index);
    const CurvePoint &b = HUNGER_CURVE.at(index + 1);
    const double between = (a.X + b.X) / 2.0;
    CAPTURE(between);
    const double inside = hungerCurve(between);
    CHECK(inside >= a.Y);
    CHECK(inside <= b.Y);
    CHECK(inside >= previous);
    const double next = hungerCurve(b.X);
    CHECK(next >= inside);
    previous = next;
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

TEST_CASE("softmaxPick sets probabilities that add up to 1, never lower for a higher Score and "
          "equal for equal Scores, and changes nothing of an option but its Probability") {
  // Distinct scores, and a tie between options of different kinds, each with terms and a stale
  // probability, which the pick must leave and replace.
  std::vector<ChoiceOption> options{optionOf(ChoiceKind::Offer, EntityKey{7}, 1.3),
                                    optionOf(ChoiceKind::Offer, EntityKey{9}, CARRY_ON_SCORE),
                                    optionOf(ChoiceKind::CarryOn, NULL_KEY, CARRY_ON_SCORE),
                                    optionOf(ChoiceKind::Offer, EntityKey{11}, -0.2)};
  for (ChoiceOption &option : options) {
    option.Relief = 0.75;
    option.Distance = -0.125;
    option.Probability = 0.5;
  }
  const std::vector<ChoiceOption> before = options;
  static_cast<void>(
      softmaxPick(DrawKey{1, EntityKey{40}, hashName("guest-choice"), 100, 0}, options));

  // Adding the probabilities in another order than the pick's may differ from 1 by rounding.
  double total = 0.0;
  for (const ChoiceOption &option : options) {
    total += option.Probability;
  }
  CHECK_THAT(total, WithinAbs(1.0, 1e-12));
  for (std::size_t i = 0; i < options.size(); ++i) {
    const ChoiceOption &a = options.at(i);
    ChoiceOption unchanged = a;
    unchanged.Probability = before.at(i).Probability;
    CAPTURE(i);
    CHECK(sameOptions(std::span(&unchanged, 1), std::span(&before.at(i), 1)));
    for (std::size_t j = 0; j < options.size(); ++j) {
      const ChoiceOption &b = options.at(j);
      CAPTURE(j);
      if (a.Score > b.Score) {
        CHECK(a.Probability >= b.Probability);
      } else if (a.Score == b.Score) {
        CHECK(a.Probability == b.Probability);
      }
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

// Every run below takes far fewer cycles than this, so a run that never shows what it waits for
// fails rather than stepping for ever.
constexpr uint64_t LONGEST_RUN = 6000;

// Whether the guest after a cycle, none once it has left, does what the option says: an offer
// makes it head to the shop, or wait there when it chose at the shop's anchor, carrying on makes it
// wander, and heading home makes it head home, or leave when it chose at an entrance's anchor.
bool follows(const std::optional<GuestRecord> &now, const ChoiceOption &option) {
  if (!now.has_value()) {
    return option.Kind == ChoiceKind::HeadHome;
  }
  const GuestRecord &record = *now;
  switch (option.Kind) {
  case ChoiceKind::Offer:
    return record.Target == option.Shop && (record.Activity == GuestActivity::HeadingToShop ||
                                            record.Activity == GuestActivity::Waiting);
  case ChoiceKind::CarryOn:
    return record.Target == NULL_KEY && record.Activity == GuestActivity::Wandering;
  case ChoiceKind::HeadHome:
    return record.Target == NULL_KEY && record.Activity == GuestActivity::HeadingHome;
  }
  return false;
}

TEST_CASE("After a guest chooses, its Activity and Target are those of one of the options "
          "guestOptions lists where it chose") {
  World world = test::eatingWorld();
  // A guest whose stay is already over, at the node where the gate path meets the spine, so that
  // the run sees heading home picked without waiting out a stay.
  const Place junction{test::EATING_SPINE, 0.0};
  REQUIRE(test::isAtNode(world, junction));
  static_cast<void>(addGuest(world, junction, world.Tick));

  bool sawOffer = false;
  bool sawOfferAtAnchor = false;
  bool sawCarryOn = false;
  bool sawHeadHome = false;
  while (!(sawOffer && sawOfferAtAnchor && sawCarryOn && sawHeadHome)) {
    REQUIRE(world.Tick < LONGEST_RUN);
    // The guests that choose in this cycle where they stand: each that starts its walk at a node,
    // and each whose visit has come back. A cycle with no command leaves route distance and the
    // offers guests read as they are now, and a guest's hunger and target change only its options'
    // scores. Every edge of the eating park is longer than a cycle's walk, so none chooses again
    // elsewhere in the cycle.
    std::map<EntityKey, std::vector<ChoiceOption>> choosing;
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      const bool waiting = record.Activity == GuestActivity::Waiting;
      const bool visitBack = waiting && unitsHeld<GuestVisits>(world, guest, guest) > 0;
      if (visitBack || (!waiting && test::isAtNode(world, record.At))) {
        const std::optional<std::vector<ChoiceOption>> options = guestOptions(world, guest);
        REQUIRE(options.has_value());
        choosing.emplace(guest, options.value_or(std::vector<ChoiceOption>{}));
      }
    }
    const uint64_t tick = world.Tick;
    stepWorld(world);
    INFO("stepped " << tick);
    for (const auto &[guest, options] : choosing) {
      const std::optional<GuestRecord> now = guestRecord(world, guest);
      CAPTURE(guest, options.size());
      const auto picked = std::ranges::find_if(
          options, [&now](const ChoiceOption &option) { return follows(now, option); });
      REQUIRE(picked != options.end());
      const bool waits = now.has_value() && now->Activity == GuestActivity::Waiting;
      sawOffer = sawOffer || picked->Kind == ChoiceKind::Offer;
      sawOfferAtAnchor = sawOfferAtAnchor || (picked->Kind == ChoiceKind::Offer && waits);
      sawCarryOn = sawCarryOn || picked->Kind == ChoiceKind::CarryOn;
      sawHeadHome = sawHeadHome || picked->Kind == ChoiceKind::HeadHome;
    }
  }
}

// A guest heading to a shop along a path, not a connector, with more than two cycles' walk left,
// so that it still heads there after the cycle an edit applies in, or none.
std::optional<EntityKey> headingAlongPath(const World &world) {
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
  return std::nullopt;
}

// The edits of the lost-route run, in the order it makes them, and then its end.
enum class NextEdit : uint8_t { DeleteShop, CutSupply, CutHome, DrawHome, None };

TEST_CASE("A guest never keeps heading for a destination it has no route to, and takes heading "
          "home up again once a route exists") {
  World world = test::eatingWorld();
  // A guest whose stay is already over, at the spine's dead end, so that it heads home along the
  // whole spine while the run cuts the routes of guests heading to shops.
  const Carrier *spine = test::findCarrier(test::guestNetwork(world), test::EATING_SPINE);
  REQUIRE(spine != nullptr);
  const Place deadEnd{test::EATING_SPINE, spine->Points.back().Distance};
  REQUIRE(test::isAtNode(world, deadEnd));
  const EntityKey late = addGuest(world, deadEnd, world.Tick);

  // A guest heading to a shop is the run's cue to take that shop away: first by deleting the shop,
  // then, for the other supplied shop, by cutting its supply route. Then the run deletes the gate
  // path while the late guest heads home, leaving it no route to the entrance, and once that guest
  // has wandered on a cycle, draws the gate path again. Each edit is followed by a cycle with none,
  // in which the guest it cut off must drop where it was heading.
  NextEdit next = NextEdit::DeleteShop;
  std::optional<EntityKey> cutOff;
  bool headsHomeAgain = false;
  while (!headsHomeAgain) {
    REQUIRE(world.Tick < LONGEST_RUN);
    CommandQueue commands;
    if (!cutOff.has_value()) {
      const std::optional<EntityKey> heading = headingAlongPath(world);
      if (next == NextEdit::DeleteShop && heading.has_value()) {
        cutOff = heading;
        commands.push(DeleteBox{recordOf(world, heading.value_or(NULL_KEY)).Target});
        next = NextEdit::CutSupply;
      } else if (next == NextEdit::CutSupply && heading.has_value()) {
        cutOff = heading;
        commands.push(DeletePath{test::EATING_BACKSTAGE});
        next = NextEdit::CutHome;
      } else if (next == NextEdit::CutHome &&
                 recordOf(world, late).Activity == GuestActivity::HeadingHome) {
        cutOff = late;
        commands.push(DeletePath{test::EATING_GATE_PATH});
        next = NextEdit::DrawHome;
      } else if (next == NextEdit::DrawHome) {
        commands.push(AddPath{PathKind::Guest, test::eatingGatePoints()});
        next = NextEdit::None;
      }
    }

    if (!commands.empty()) {
      const std::optional<GuestRecord> was =
          cutOff.has_value() ? guestRecord(world, cutOff.value_or(NULL_KEY)) : std::nullopt;
      stepWorld(world, commands);
      // Guests step before a cycle's commands apply, so the guest the edit cuts off still heads
      // where it did.
      if (was.has_value()) {
        const GuestRecord record = recordOf(world, cutOff.value_or(NULL_KEY));
        REQUIRE(record.Activity == was->Activity);
        REQUIRE(record.Target == was->Target);
      }
      continue;
    }

    const test::ParkOffers offers = test::parkOffers(world);
    const uint64_t tick = world.Tick;
    const Place lateWas = recordOf(world, late).At;
    stepWorld(world);
    INFO("stepped " << tick);
    // A cycle with no command leaves route distance as it was, and guests read the offers as they
    // stood before it.
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(world, record.At);
      CAPTURE(guest);
      if (record.Activity == GuestActivity::HeadingToShop) {
        CHECK(test::isOfferReachable(offers, routes, record.Target));
      } else if (record.Activity == GuestActivity::HeadingHome) {
        CHECK(test::homeEntry(world, routes).has_value());
      }
    }
    if (cutOff == late) {
      // Past its stay with no way home, it wanders on.
      const GuestRecord record = recordOf(world, late);
      CHECK(record.Activity == GuestActivity::Wandering);
      CHECK(record.At != lateWas);
    }
    cutOff.reset();
    headsHomeAgain =
        next == NextEdit::None && recordOf(world, late).Activity == GuestActivity::HeadingHome;
  }
}

} // namespace
} // namespace tpj
