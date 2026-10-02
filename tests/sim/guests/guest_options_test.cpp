#include "support/guest_parks.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::recordOf;

bool isSupplied(const test::ParkOffers &offers, EntityKey shop) {
  return test::offerAmong(offers, shop).value_or(OfferEntry{}).Supplied;
}

bool anyGuestWithTarget(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  return std::ranges::any_of(
      guests, [&](EntityKey guest) { return recordOf(world, guest).Target != NULL_KEY; });
}

// The eating park stepped until both supplied shops' offers say meals and some guest has a target,
// so its guests weigh two offers and one of them is committed to its target.
World eatingWithTarget() {
  World world = test::eatingWorld();
  const auto ready = [&]() {
    const test::ParkOffers offers = test::parkOffers(world);
    return isSupplied(offers, test::NEAR_SHOP) && isSupplied(offers, test::FAR_SHOP) &&
           anyGuestWithTarget(world);
  };
  for (int cycle = 0; cycle < 3000 && !ready(); ++cycle) {
    stepWorld(world);
  }
  REQUIRE(ready());
  return world;
}

// The world with its tick moved to the end of the latest stay, so every guest's stay is over and
// only the tick differs.
World everyStayOver(const World &world) {
  uint64_t latest = world.Tick;
  for (const EntityKey guest : parkGuests(world)) {
    latest = std::max(latest, recordOf(world, guest).StayUntil);
  }
  World moved = copyWorld(world);
  moved.Tick = latest;
  return moved;
}

// The eating park once its first guest has arrived, and the same park after a cycle deleting every
// guest path, which leaves its guests standing at places that no longer resolve until they next
// step.
struct CutUnderGuest {
  World Before;
  World After;
  EntityKey Guest = NULL_KEY;
};

CutUnderGuest cutUnderGuest() {
  World world = test::eatingWorld();
  const EntityKey guest{world.nextKey()};
  test::stepUntil(world, test::FIRST_ARRIVAL + 1);
  REQUIRE(guestRecord(world, guest).has_value());
  World cut = copyWorld(world);
  CommandQueue commands;
  for (const EntityKey path : {test::EATING_GATE_PATH, test::EATING_SPINE, test::EATING_CROSS}) {
    commands.push(DeletePath{path});
  }
  stepWorld(cut, commands);
  const std::optional<GuestRecord> record = guestRecord(cut, guest);
  REQUIRE(record.has_value());
  REQUIRE_FALSE(record.value_or(GuestRecord{}).Position.has_value());
  return CutUnderGuest{.Before = std::move(world), .After = std::move(cut), .Guest = guest};
}

TEST_CASE("guestOptions gives none for a key that holds no guest") {
  const CutUnderGuest park = cutUnderGuest();
  const World &world = park.Before;
  // A shop, an entrance, a path, the null key, and a key no entity has held yet.
  for (const EntityKey key : {test::NEAR_SHOP, test::EATING_GATE, test::EATING_SPINE, NULL_KEY,
                              EntityKey{world.nextKey()}}) {
    CAPTURE(key);
    CHECK_FALSE(guestOptions(world, key).has_value());
  }
}

TEST_CASE("guestOptions gives none for a guest whose place does not resolve on the guest network, "
          "where it gave options while the place resolved") {
  const CutUnderGuest park = cutUnderGuest();
  CHECK(guestOptions(park.Before, park.Guest).has_value());
  CHECK_FALSE(guestOptions(park.After, park.Guest).has_value());
}

TEST_CASE("guestOptions gives the options Choice lists at the guest's place, in order, with the "
          "world's tick and the guest's Hunger and Target, each with Choice's terms and Score and "
          "softmaxPick's Probability") {
  const World playing = eatingWithTarget();
  // Moving the tick past every stay adds heading home and changes nothing else.
  const World over = everyStayOver(playing);
  bool sawTwoOffers = false;
  bool sawCommitment = false;
  bool sawHeadHome = false;
  bool sawNoHeadHome = false;
  for (const World *world : {&playing, &over}) {
    const test::ParkOffers offers = test::parkOffers(*world);
    for (const EntityKey guest : parkGuests(*world)) {
      CAPTURE(world->Tick, guest);
      const GuestRecord record = recordOf(*world, guest);
      const std::vector<ChoiceOption> expected =
          test::choiceOptions(*world, offers, test::routesAt(*world, record.At), record.Hunger,
                              record.Target, world->Tick >= record.StayUntil);
      const std::optional<std::vector<ChoiceOption>> options = guestOptions(*world, guest);
      REQUIRE(options.has_value());
      CHECK(options.value_or(std::vector<ChoiceOption>{}) == expected);
      sawTwoOffers =
          sawTwoOffers || std::ranges::count(expected, ChoiceKind::Offer, &ChoiceOption::Kind) >= 2;
      sawCommitment = sawCommitment ||
                      std::ranges::count(expected, COMMITMENT_BONUS, &ChoiceOption::Commitment) > 0;
      const bool headsHome =
          std::ranges::count(expected, ChoiceKind::HeadHome, &ChoiceOption::Kind) > 0;
      sawHeadHome = sawHeadHome || headsHome;
      sawNoHeadHome = sawNoHeadHome || !headsHome;
    }
  }
  CHECK(sawTwoOffers);
  CHECK(sawCommitment);
  CHECK(sawHeadHome);
  CHECK(sawNoHeadHome);
}

TEST_CASE("guestOptions changes nothing: the world after calls for every guest and for keys "
          "holding none is worldsEqual to a copy taken before them") {
  const World playing = eatingWithTarget();
  const World copy = copyWorld(playing);
  for (const EntityKey guest : parkGuests(playing)) {
    (void)guestOptions(playing, guest);
  }
  (void)guestOptions(playing, test::NEAR_SHOP);
  (void)guestOptions(playing, NULL_KEY);
  CHECK(worldsEqual(playing, copy));

  // A guest whose place does not resolve.
  const CutUnderGuest park = cutUnderGuest();
  const World cutCopy = copyWorld(park.After);
  (void)guestOptions(park.After, park.Guest);
  CHECK(worldsEqual(park.After, cutCopy));
}

} // namespace
} // namespace tpj
