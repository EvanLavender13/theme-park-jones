// The food loop on the checked-in parks: tests/parks/fed.park feeds its guests, and cutting the
// backstage path, which turns warm.park into cut.park, withdraws the shop's offer, stops guests
// picking it, empties its queue, and leaves guests hungrier. These name parks and compare runs, so
// they are not laws that park_laws_test.cpp could check over every park file.

#include "support/park_files.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// Ticks the fed park runs: past its first meal, which waits for its shop's first shipment.
constexpr uint64_t FED_TICKS = 700;
// Ticks cut.park's offer is watched: its stepped offer is republished every tick.
constexpr uint64_t OFFER_TICKS = 10;
// Ticks warm.park and cut.park each run when their picks are compared: long enough for a guest of
// warm.park to pick its shop.
constexpr uint64_t PICK_TICKS = 120;
// The most ticks cut.park runs for its queue to empty, one guest per SERVICE_INTERVAL.
constexpr uint64_t QUEUE_TICKS = 600;
// Ticks warm.park and cut.park each run, from the same tick, when their guests' hunger is
// compared: long enough for warm.park to serve a guest that cut.park does not.
constexpr uint64_t HUNGER_TICKS = 540;

World openParkFile(std::string_view name) { return openPark(parkFile(name)); }

std::vector<EntityKey> shopBoxes(const World &world) {
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  return shops;
}

// The shop boxes with no route to a depot.
std::vector<EntityKey> starvedShops(const World &world) {
  std::vector<EntityKey> starved;
  for (const EntityKey shop : shopBoxes(world)) {
    if (!nearestDepot(world, shop).has_value()) {
      starved.push_back(shop);
    }
  }
  return starved;
}

// The shop's food offer as guests find it: its first own entry in the food offer at the place of
// its lowest anchored node on the guest network.
std::optional<OfferEntry> offerOf(const World &world, EntityKey shop) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<uint32_t> anchors = network.anchoredNodes(shop);
  if (anchors.empty()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, network, network.nodePlace(anchors.front()))) {
    if (entry.Source == shop) {
      return entry.Value;
    }
  }
  return std::nullopt;
}

// Whether the guest's last choice was made while stepping the tick and picked an offer of the
// shop.
bool pickedAt(const GuestRecord &record, EntityKey shop, uint64_t tick) {
  if (!record.LastChoice.has_value()) {
    return false;
  }
  const GuestChoice &choice = record.LastChoice.value_or(GuestChoice{});
  if (choice.Tick != tick || choice.Picked >= choice.Options.size()) {
    return false;
  }
  const ChoiceOption &picked = choice.Options.at(choice.Picked);
  return picked.Kind == ChoiceKind::Offer && picked.Shop == shop;
}

double meanHunger(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  REQUIRE_FALSE(guests.empty());
  double total = 0.0;
  for (const EntityKey guest : guests) {
    total += recordOf(world, guest).Hunger;
  }
  return total / static_cast<double>(guests.size());
}

// Catches a broken seam anywhere in the loop: supplies that never reach the shop over the
// backstage route, an offer guests cannot find or reach, a visit that never reaches the shop, or a
// meal that never reaches the guest it was made for.
TEST_CASE("Running fed.park serves meals to its guests") {
  World world = openParkFile("fed.park");
  for (uint64_t cycle = 0; cycle < FED_TICKS; ++cycle) {
    stepWorld(world);
  }
  CHECK(unitsConsumed<Meals>(world, EATEN_CAUSE) > 0);
}

// Catches a starved shop whose offer still says meals until a later step republishes it, so
// guests keep walking to a shop that cannot feed them.
TEST_CASE("In cut.park the starved shop's offer says no meals from its first tick on, where in "
          "warm.park the same shop's offer says meals") {
  const World warm = openParkFile("warm.park");
  World cut = openParkFile("cut.park");
  const std::vector<EntityKey> starved = starvedShops(cut);
  REQUIRE_FALSE(starved.empty());
  for (const EntityKey shop : starved) {
    INFO("shop " << static_cast<uint64_t>(shop));
    const std::optional<OfferEntry> warmOffer = offerOf(warm, shop);
    REQUIRE(warmOffer.has_value());
    CHECK(warmOffer.value_or(OfferEntry{}).Supplied);
  }
  bool saysNoMeals = true;
  for (uint64_t cycle = 0; cycle <= OFFER_TICKS && saysNoMeals; ++cycle) {
    if (cycle > 0) {
      stepWorld(cut);
    }
    for (const EntityKey shop : starved) {
      INFO("shop " << static_cast<uint64_t>(shop) << " at tick " << cut.Tick);
      const std::optional<OfferEntry> offer = offerOf(cut, shop);
      REQUIRE(offer.has_value());
      saysNoMeals = saysNoMeals && !offer.value_or(OfferEntry{}).Supplied;
      CHECK(saysNoMeals);
    }
  }
}

// The picks of an offer of the shop that guests make in a run of the world.
uint64_t picksInRun(World &world, EntityKey shop, uint64_t ticks) {
  uint64_t picks = 0;
  for (uint64_t cycle = 0; cycle < ticks; ++cycle) {
    const uint64_t stepping = world.Tick;
    stepWorld(world);
    for (const EntityKey guest : parkGuests(world)) {
      if (pickedAt(recordOf(world, guest), shop, stepping)) {
        ++picks;
      }
    }
  }
  return picks;
}

// Catches guests that choose from a stale or wrongly read offer, walking to a shop whose route is
// cut.
TEST_CASE("No guest picks the starved shop in a run of cut.park, where guests pick it in the same "
          "run of warm.park") {
  World warm = openParkFile("warm.park");
  World cut = openParkFile("cut.park");
  const std::vector<EntityKey> starved = starvedShops(cut);
  REQUIRE_FALSE(starved.empty());
  const EntityKey shop = starved.front();
  CHECK(picksInRun(warm, shop, PICK_TICKS) > 0);
  CHECK(picksInRun(cut, shop, PICK_TICKS) == 0);
}

// Catches visits a starved shop keeps forever, or loses, leaving their guests waiting at its door.
TEST_CASE("In a run of cut.park the starved shop's queue empties, and every guest queued at the "
          "cut is served or returned unserved") {
  World world = openParkFile("cut.park");
  const std::vector<EntityKey> starved = starvedShops(world);
  REQUIRE_FALSE(starved.empty());
  const EntityKey shop = starved.front();
  // The guests queued are those whose visits the shop holds.
  std::vector<EntityKey> queued;
  for (const FlowHolding &holding : stockOf<GuestVisits>(world, shop)) {
    REQUIRE(guestRecord(world, holding.Handle).has_value());
    queued.push_back(holding.Handle);
  }
  REQUIRE_FALSE(queued.empty());
  // A guest stops waiting at the shop only when its visit comes back, served or not.
  std::set<EntityKey> released;
  for (uint64_t cycle = 0; cycle < QUEUE_TICKS && released.size() < queued.size(); ++cycle) {
    stepWorld(world);
    for (const EntityKey guest : queued) {
      if (released.contains(guest)) {
        continue;
      }
      INFO("guest " << static_cast<uint64_t>(guest) << " at tick " << world.Tick);
      // A waiting guest never leaves the park, so one that left while queued lost its visit.
      const std::optional<GuestRecord> record = guestRecord(world, guest);
      REQUIRE(record.has_value());
      const GuestRecord current = record.value_or(GuestRecord{});
      if (current.Activity != GuestActivity::Waiting || current.Target != shop) {
        released.insert(guest);
      }
    }
  }
  for (const EntityKey guest : queued) {
    INFO("guest " << static_cast<uint64_t>(guest) << " queued at the cut");
    CHECK(released.contains(guest));
  }
  const std::optional<ShopRecord> record = shopRecord(world, shop);
  REQUIRE(record.has_value());
  CHECK(record.value_or(ShopRecord{}).Queue == 0);
}

// Each guest's MealsEaten, by key, for the guests of the world.
std::map<EntityKey, uint64_t> mealsEatenBy(const World &world) {
  std::map<EntityKey, uint64_t> eaten;
  for (const EntityKey guest : parkGuests(world)) {
    // Every key parkGuests gives holds a guest, so it has a record.
    eaten.emplace(guest, guestRecord(world, guest).value_or(GuestRecord{}).MealsEaten);
  }
  return eaten;
}

// Adds to the set each guest of the world that has eaten more meals than when the run started, a
// guest that arrived since counting as having eaten none.
void addServed(const World &world, const std::map<EntityKey, uint64_t> &atStart,
               std::set<EntityKey> &served) {
  for (const auto &[guest, eaten] : mealsEatenBy(world)) {
    const auto start = atStart.find(guest);
    if (eaten > (start == atStart.end() ? 0 : start->second)) {
      served.insert(guest);
    }
  }
}

// Catches a cut that never reaches the guests: meals made with no supply, or guests fed from an
// offer the cut should have withdrawn.
TEST_CASE("Once warm.park has served a guest that cut.park does not, mean guest hunger is higher "
          "in cut.park than in warm.park at the same tick") {
  World warm = openParkFile("warm.park");
  World cut = openParkFile("cut.park");
  // cut.park is warm.park after one more cycle, so warm.park steps that cycle first and both end
  // at the same tick, where cut.park's guests have not had a tick longer to grow hungry.
  stepWorld(warm);
  REQUIRE(warm.Tick == cut.Tick);
  const std::map<EntityKey, uint64_t> warmAtStart = mealsEatenBy(warm);
  const std::map<EntityKey, uint64_t> cutAtStart = mealsEatenBy(cut);
  std::set<EntityKey> servedInWarm;
  std::set<EntityKey> servedInCut;
  for (uint64_t cycle = 0; cycle < HUNGER_TICKS; ++cycle) {
    stepWorld(warm);
    stepWorld(cut);
    addServed(warm, warmAtStart, servedInWarm);
    addServed(cut, cutAtStart, servedInCut);
  }
  REQUIRE(warm.Tick == cut.Tick);
  CHECK(std::ranges::any_of(servedInWarm,
                            [&](EntityKey guest) { return !servedInCut.contains(guest); }));
  CHECK(meanHunger(cut) > meanHunger(warm));
}

} // namespace
} // namespace tpj
