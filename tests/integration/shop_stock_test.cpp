// A shop's stock setting decides which goods it orders and serves.

#include "support/park_files.h"

#include "sim/clock.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

bool isOpenNow(const World &world, const OpeningHours &hours) {
  return isOpenAt(hours, parkTime(world.Tick).Second);
}

double meanThirst(const World &world) {
  const std::vector<EntityKey> guests = parkGuests(world);
  REQUIRE_FALSE(guests.empty());
  double total = 0.0;
  for (const EntityKey guest : guests) {
    total += recordOf(world, guest).Thirst;
  }
  return total / static_cast<double>(guests.size());
}

// The park's one shop set to stock drink, which must be the only shop selling drink.
EntityKey onlyDrinkShop(const World &world) {
  std::vector<ParkBox> drinkSellers;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop && sells(box.Stock, Good::Drink)) {
      drinkSellers.push_back(box);
    }
  }
  REQUIRE(drinkSellers.size() == 1);
  REQUIRE(drinkSellers.front().Stock == ShopStock::Drink);
  return drinkSellers.front().Key;
}

ShopStock shopStock(const World &world, EntityKey shop) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == shop) {
      return box.Stock;
    }
  }
  FAIL("no box holds the shop's key");
  return ShopStock::Food;
}

// Whether the shop's drink offer, as guests sample it, says it can serve drink.
bool offersDrink(const World &world, EntityKey shop) {
  const FieldSlot<OfferEntry> *slot = sourceSlot<DrinkOffer>(world, shop);
  return slot != nullptr &&
         std::ranges::any_of(slot->Entries, [](const PlacedEntry<OfferEntry> &entry) {
           return entry.Value.Supplied;
         });
}

TEST_CASE("Switching the drink shop to stock food only leaves guests thirstier at closing") {
  World unchanged = openPark(parkFile("noon.park"));
  std::vector<EntityKey> drinkShops;
  for (const ParkBox &box : parkBoxes(unchanged)) {
    if (box.Kind == BoxKind::Shop && box.Stock == ShopStock::Drink) {
      drinkShops.push_back(box.Key);
    }
  }
  REQUIRE(drinkShops.size() == 1);
  const EntityKey drinkShop = drinkShops.front();

  World switched = copyWorld(unchanged);
  CommandQueue switching;
  switching.push(SetShopStock{drinkShop, ShopStock::Food});
  // Both step the cycle that applies the switch, so they stand at the same tick from then on.
  stepWorld(unchanged);
  stepWorld(switched, switching);
  for (const ParkBox &box : parkBoxes(switched)) {
    if (box.Key == drinkShop) {
      REQUIRE(box.Stock == ShopStock::Food);
    }
  }

  const OpeningHours hours = parkHours(unchanged);
  REQUIRE(isOpenNow(unchanged, hours));
  // A hang guard only: an open park closes within a day.
  const uint64_t start = unchanged.Tick;
  while (isOpenNow(unchanged, hours)) {
    REQUIRE(unchanged.Tick < start + DAY_TICKS);
    stepWorld(unchanged);
    stepWorld(switched);
  }
  REQUIRE(unchanged.Tick == switched.Tick);
  CHECK(meanThirst(switched) > meanThirst(unchanged));
}

TEST_CASE("Switching the drink shop to stock food only stops its drink once it has served what it "
          "had") {
  World world = openPark(parkFile("noon.park"));
  const EntityKey shop = onlyDrinkShop(world);
  CommandQueue switching;
  switching.push(SetShopStock{shop, ShopStock::Food});
  stepWorld(world, switching);
  REQUIRE(shopStock(world, shop) == ShopStock::Food);

  // No shop sells drink from here on, so the park's drink orders and drinks are this shop's.
  const int64_t ordersAtSwitch = unitsCreated<DrinkOrders>(world);
  bool orderedAfterSwitch = false;
  std::optional<uint64_t> soldOutAt;
  int64_t drinksAtSoldOut = 0;
  bool offeredAfterSoldOut = false;
  bool servedAfterSoldOut = false;
  // Steps until the shop has served the drink it held and the drink on its way, then a whole day
  // more, so a day's guests have the chance to be served drink again.
  // A hang guard only: thirsty guests come every day the park opens, so the drink runs out.
  const uint64_t giveUp = world.Tick + (4 * DAY_TICKS);
  while (!soldOutAt || world.Tick < *soldOutAt + DAY_TICKS) {
    REQUIRE(world.Tick < giveUp);
    stepWorld(world);
    orderedAfterSwitch = orderedAfterSwitch || unitsCreated<DrinkOrders>(world) != ordersAtSwitch;
    if (!soldOutAt) {
      const std::optional<ShopRecord> record = shopRecord(world, shop);
      REQUIRE(record.has_value());
      const GoodRecord drink =
          record.value_or(ShopRecord{}).Goods.at(static_cast<size_t>(Good::Drink));
      if (drink.Stock == 0 && drink.OnOrder == 0) {
        soldOutAt = world.Tick;
        drinksAtSoldOut = unitsCreated<Drinks>(world);
      }
    }
    if (soldOutAt) {
      offeredAfterSoldOut = offeredAfterSoldOut || offersDrink(world, shop);
      servedAfterSoldOut = servedAfterSoldOut || unitsCreated<Drinks>(world) != drinksAtSoldOut;
    }
  }
  CHECK_FALSE(orderedAfterSwitch);
  CHECK_FALSE(offeredAfterSoldOut);
  CHECK_FALSE(servedAfterSoldOut);
}

} // namespace
} // namespace tpj
