#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

// The boxes-and-tubes slice's park files, each run for the shortest length that shows its
// property.
namespace tpj {
namespace {

constexpr std::array<std::string_view, 3> SLICE_PARKS = {"fed.park", "warm.park", "cut.park"};

std::string readPark(std::string_view name) {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / name, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

World openText(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

World openPark(std::string_view name) {
  const std::string text = readPark(name);
  REQUIRE_FALSE(text.empty());
  return openText(text);
}

template <typename T> size_t countOfKind(const std::vector<T> &items, auto kind) {
  return static_cast<size_t>(
      std::ranges::count_if(items, [kind](const T &item) { return item.Kind == kind; }));
}

// The park's one shop box.
EntityKey theShop(const World &world) {
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  REQUIRE(shops.size() == 1);
  return shops.front();
}

// The entries the shop publishes into the food offer, sampled at its guest anchor.
std::vector<OfferEntry> shopOffer(const World &world, EntityKey shop) {
  const Network &guests = parkNetwork(world, PathKind::Guest);
  const std::vector<uint32_t> anchors = guests.anchoredNodes(shop);
  REQUIRE_FALSE(anchors.empty());
  std::vector<OfferEntry> offer;
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, guests, guests.nodePlace(anchors.front()))) {
    if (entry.Source == shop) {
      offer.push_back(entry.Value);
    }
  }
  return offer;
}

GuestRecord recordOf(const World &world, EntityKey guest) {
  const std::optional<GuestRecord> record = guestRecord(world, guest);
  REQUIRE(record.has_value());
  return record.value_or(GuestRecord{});
}

bool waitsAt(const GuestRecord &record, EntityKey shop) {
  return record.Activity == GuestActivity::Waiting && record.Target == shop;
}

std::vector<EntityKey> guestsWaitingAt(const World &world, EntityKey shop) {
  std::vector<EntityKey> waiting;
  for (const EntityKey guest : parkGuests(world)) {
    if (waitsAt(recordOf(world, guest), shop)) {
      waiting.push_back(guest);
    }
  }
  return waiting;
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

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

TEST_CASE("fed.park opens at tick 0 with one entrance, shop, depot, and backstage path, guest "
          "paths, and no guests") {
  const World world = openPark("fed.park");
  CHECK(world.Tick == 0);
  CHECK(parkEntrances(world).size() == 1);
  CHECK(countOfKind(parkBoxes(world), BoxKind::Shop) == 1);
  CHECK(countOfKind(parkBoxes(world), BoxKind::Depot) == 1);
  CHECK(countOfKind(parkPaths(world), PathKind::Backstage) == 1);
  CHECK(countOfKind(parkPaths(world), PathKind::Guest) >= 1);
  CHECK(parkGuests(world).empty());
}

TEST_CASE("cut.park opens with no backstage path, a guest waiting at the shop, and supplies in "
          "transit") {
  const World world = openPark("cut.park");
  CHECK(countOfKind(parkPaths(world), PathKind::Backstage) == 0);
  CHECK_FALSE(guestsWaitingAt(world, theShop(world)).empty());
  const Ledger *supplies = ledgerOf<Supplies>(world);
  REQUIRE(supplies != nullptr);
  CHECK_FALSE(supplies->Packets.empty());
}

TEST_CASE("fed.park serves meals within 1800 ticks, and every served guest's hunger fell when it "
          "ate") {
  World world = openPark("fed.park");
  size_t unfallen = 0;
  std::optional<uint64_t> firstUnfallenTick;
  for (int cycle = 0; cycle < 1800; ++cycle) {
    stepWorld(world);
    for (const EntityKey guest : parkGuests(world)) {
      const GuestRecord record = recordOf(world, guest);
      if (record.LastMeal && !(record.LastMeal->After < record.LastMeal->Before)) {
        ++unfallen;
        firstUnfallenTick = firstUnfallenTick.value_or(world.Tick);
      }
    }
  }
  INFO("first tick with a meal that did not lower hunger: " << firstUnfallenTick.value_or(0));
  CHECK(unfallen == 0);
  CHECK(unitsConsumed<Meals>(world, EATEN_CAUSE) > 0);
}

TEST_CASE("supplies and meals are conserved in every cycle of 300-tick runs of each slice park") {
  for (const std::string_view name : SLICE_PARKS) {
    INFO("park " << name);
    World world = openPark(name);
    std::vector<uint64_t> unconservedTicks;
    for (int cycle = 0; cycle < 300; ++cycle) {
      stepWorld(world);
      if (!conserved<Supplies>(world) || !conserved<Meals>(world)) {
        unconservedTicks.push_back(world.Tick);
      }
    }
    INFO("first unconserved tick: " << (unconservedTicks.empty() ? 0 : unconservedTicks.front()));
    CHECK(unconservedTicks.empty());
  }
}

TEST_CASE("cut.park's shop offers no meals from its opened world through 600 ticks") {
  World world = openPark("cut.park");
  const EntityKey shop = theShop(world);
  const auto offersNoMeals = [&] {
    const std::vector<OfferEntry> offer = shopOffer(world, shop);
    return !offer.empty() &&
           std::ranges::none_of(offer, [](const OfferEntry &entry) { return entry.Supplied; });
  };
  CHECK(offersNoMeals());
  std::vector<uint64_t> suppliedTicks;
  for (int cycle = 0; cycle < 600; ++cycle) {
    stepWorld(world);
    if (!offersNoMeals()) {
      suppliedTicks.push_back(world.Tick);
    }
  }
  INFO("first tick offering meals or no entry: "
       << (suppliedTicks.empty() ? 0 : suppliedTicks.front()));
  CHECK(suppliedTicks.empty());
}

TEST_CASE("no guest picks cut.park's shop in 600 ticks") {
  World world = openPark("cut.park");
  const EntityKey shop = theShop(world);
  // A choice's tick is the tick its cycle stepped, so one at the opened world's tick or later was
  // made in cut.park's own cycles, all of which saw the shop's offer say no meals.
  const uint64_t start = world.Tick;
  std::vector<uint64_t> pickedTicks;
  for (int cycle = 0; cycle < 600; ++cycle) {
    stepWorld(world);
    for (const EntityKey guest : parkGuests(world)) {
      const std::optional<GuestChoice> &choice = recordOf(world, guest).LastChoice;
      if (!choice || choice->Tick < start) {
        continue;
      }
      REQUIRE(choice->Picked < choice->Options.size());
      const ChoiceOption &picked = choice->Options.at(choice->Picked);
      if (picked.Kind == ChoiceKind::Offer && picked.Shop == shop) {
        pickedTicks.push_back(choice->Tick);
      }
    }
  }
  INFO("first tick the shop was picked: " << (pickedTicks.empty() ? 0 : pickedTicks.front()));
  CHECK(pickedTicks.empty());
}

TEST_CASE("every guest waiting at cut.park's shop has left or stopped waiting within 600 ticks") {
  World world = openPark("cut.park");
  const EntityKey shop = theShop(world);
  const std::vector<EntityKey> waiting = guestsWaitingAt(world, shop);
  REQUIRE_FALSE(waiting.empty());
  for (int cycle = 0; cycle < 600; ++cycle) {
    stepWorld(world);
  }
  const std::vector<EntityKey> guests = parkGuests(world);
  for (const EntityKey guest : waiting) {
    INFO("guest " << static_cast<uint64_t>(guest));
    const bool left = std::ranges::find(guests, guest) == guests.end();
    CHECK((left || !waitsAt(recordOf(world, guest), shop)));
  }
}

TEST_CASE("cut.park's guests end 600 ticks hungrier on average than warm.park's") {
  World cut = openPark("cut.park");
  World warm = openPark("warm.park");
  for (int cycle = 0; cycle < 600; ++cycle) {
    stepWorld(cut);
    stepWorld(warm);
  }
  CHECK(meanHunger(cut) > meanHunger(warm));
}

TEST_CASE("saving each opened slice park gives its text") {
  for (const std::string_view name : SLICE_PARKS) {
    INFO("park " << name);
    const std::string text = readPark(name);
    REQUIRE_FALSE(text.empty());
    CHECK(saveWorld(openText(text)) == text);
  }
}

TEST_CASE("opening the save of each opened slice park gives an equal world") {
  for (const std::string_view name : SLICE_PARKS) {
    INFO("park " << name);
    const World opened = openPark(name);
    CHECK(worldsEqual(openText(saveWorld(opened)), opened));
  }
}

TEST_CASE("two runs of fed.park have equal hashes after every cycle of 600 ticks") {
  World first = openPark("fed.park");
  World second = openPark("fed.park");
  std::vector<uint64_t> differingTicks;
  for (int cycle = 0; cycle < 600; ++cycle) {
    stepWorld(first);
    stepWorld(second);
    if (hashWorld(first) != hashWorld(second)) {
      differingTicks.push_back(first.Tick);
    }
  }
  INFO("first differing tick: " << (differingTicks.empty() ? 0 : differingTicks.front()));
  CHECK(differingTicks.empty());
}

} // namespace
} // namespace tpj
