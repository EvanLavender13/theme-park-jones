#include "support/guest_parks.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/routes/route_distance.h"
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

TEST_CASE("guestOptions gives none exactly when the key holds no guest standing on the guest "
          "network") {
  const CutUnderGuest park = cutUnderGuest();
  CHECK(guestOptions(park.Before, park.Guest).has_value());
  // A key that holds a shop, not a guest.
  CHECK_FALSE(guestOptions(park.Before, test::NEAR_SHOP).has_value());
  // The guest itself, once its place no longer resolves.
  CHECK_FALSE(guestOptions(park.After, park.Guest).has_value());
}

// An offer a guest weighs, with what its terms depend on: the guest's hunger, its route distance
// to the shop, and the shop's wait.
struct Weighed {
  ChoiceOption Option;
  double Hunger = 0.0;
  double Distance = 0.0;
  uint64_t Wait = 0;
};

TEST_CASE("An offer never scores lower when the guest is hungrier and scores lower when it is "
          "farther or its wait longer, its Score is its terms added in order, and heading home is "
          "listed only once the guest's stay is over, and then outscores every offer") {
  const World playing = eatingWithTarget();
  // Moving the tick past every stay ends them and changes nothing else.
  const World over = everyStayOver(playing);
  bool sawHungrier = false;
  bool sawFarther = false;
  bool sawLonger = false;
  bool sawHeadHome = false;
  bool sawNoHeadHome = false;
  for (const World *world : {&playing, &over}) {
    const test::ParkOffers offers = test::parkOffers(*world);
    std::vector<Weighed> weighed;
    for (const EntityKey guest : parkGuests(*world)) {
      CAPTURE(world->Tick, guest);
      const GuestRecord record = recordOf(*world, guest);
      const std::optional<std::vector<ChoiceOption>> listed = guestOptions(*world, guest);
      REQUIRE(listed.has_value());
      const std::vector<ChoiceOption> options = listed.value_or(std::vector<ChoiceOption>{});
      const auto home = std::ranges::find(options, ChoiceKind::HeadHome, &ChoiceOption::Kind);
      const bool headsHome = home != options.end();
      CHECK((!headsHome || world->Tick >= record.StayUntil));
      sawHeadHome = sawHeadHome || headsHome;
      sawNoHeadHome = sawNoHeadHome || !headsHome;
      const std::vector<SampledEntry<RouteEntry>> routes = test::routesAt(*world, record.At);
      for (const ChoiceOption &option : options) {
        if (option.Kind != ChoiceKind::Offer) {
          continue;
        }
        CAPTURE(option.Shop);
        CHECK(test::sameBits(option.Score, ((option.Relief + option.Distance) + option.Wait) +
                                               option.Commitment));
        if (headsHome) {
          CHECK(home->Score > option.Score);
        }
        const std::optional<RouteEntry> entry = test::routeFrom(routes, option.Shop);
        const std::optional<OfferEntry> offer = test::offerAmong(offers, option.Shop);
        REQUIRE(entry.has_value());
        REQUIRE(offer.has_value());
        weighed.push_back(Weighed{.Option = option,
                                  .Hunger = record.Hunger,
                                  .Distance = entry.value_or(RouteEntry{}).Distance,
                                  .Wait = offer.value_or(OfferEntry{}).Wait});
      }
    }
    for (const Weighed &a : weighed) {
      for (const Weighed &b : weighed) {
        CAPTURE(a.Option.Shop, a.Hunger, a.Distance, a.Wait, b.Option.Shop, b.Hunger, b.Distance,
                b.Wait);
        if (a.Option.Shop == b.Option.Shop && a.Hunger > b.Hunger) {
          CHECK(a.Option.Relief >= b.Option.Relief);
          sawHungrier = true;
        }
        if (a.Distance > b.Distance) {
          CHECK(a.Option.Distance < b.Option.Distance);
          sawFarther = true;
        }
        if (a.Wait > b.Wait) {
          CHECK(a.Option.Wait < b.Option.Wait);
          sawLonger = true;
        }
      }
    }
  }
  CHECK(sawHungrier);
  CHECK(sawFarther);
  CHECK(sawLonger);
  CHECK(sawHeadHome);
  CHECK(sawNoHeadHome);
}

} // namespace
} // namespace tpj
