#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// State: a guest that has sent one visit, and leaves at LeavesAt if the visit has not come back.
struct SyntheticGuest {
  uint64_t LeavesAt = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, SyntheticGuest &guest) {
  visitor.field("leaves-at", guest.LeavesAt);
}

constexpr uint64_t SPAWN_PURPOSE = hashName("food-shop-spawn");
constexpr uint64_t GUEST_PURPOSE = hashName("food-shop-guest");
// A guest appears in a cycle with this chance.
constexpr double SPAWN_CHANCE = 1.0 / 45.0;
// A guest waits at least LEAST_PATIENCE ticks for its visit to come back, and up to EXTRA_PATIENCE
// more.
constexpr double LEAST_PATIENCE = 100.0;
constexpr double EXTRA_PATIENCE = 1500.0;
// A visit takes 1 to 1 + LONGEST_WALK ticks to reach its shop.
constexpr double LONGEST_WALK = 60.0;
// The cause a guest consumes a drink with.
constexpr std::string_view DRUNK_CAUSE = "drunk";

// Guests whose visit has come back consume it, with any meal or drink, and leave, and guests out of
// patience leave without it. Then a guest may appear and send a visit to a drawn shop, for food or
// drink, whichever the shop sells, drawn when it sells both.
void stepGuests(World &world) {
  std::vector<EntityKey> leaving;
  for (const EntityKey key : world.keys()) {
    const auto *guest = world.tryGet<SyntheticGuest>(key);
    if (guest == nullptr) {
      continue;
    }
    const int64_t visits = unitsHeld<GuestVisits>(world, key, key);
    const int64_t drinkVisits = unitsHeld<DrinkVisits>(world, key, key);
    if (visits > 0 || drinkVisits > 0) {
      if (visits > 0) {
        consumeUnits<GuestVisits>(world, key, key, visits, FINISHED_CAUSE);
      }
      if (drinkVisits > 0) {
        consumeUnits<DrinkVisits>(world, key, key, drinkVisits, FINISHED_CAUSE);
      }
      const int64_t meals = unitsHeld<Meals>(world, key, key);
      if (meals > 0) {
        consumeUnits<Meals>(world, key, key, meals, EATEN_CAUSE);
      }
      const int64_t drinks = unitsHeld<Drinks>(world, key, key);
      if (drinks > 0) {
        consumeUnits<Drinks>(world, key, key, drinks, DRUNK_CAUSE);
      }
      leaving.push_back(key);
    } else if (world.Tick >= guest->LeavesAt) {
      leaving.push_back(key);
    }
  }
  for (const EntityKey key : leaving) {
    world.destroyEntity(key);
  }
  if (drawUniform(drawKey(world, NULL_KEY, SPAWN_PURPOSE, 0)) >= SPAWN_CHANCE) {
    return;
  }
  std::vector<ParkBox> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box);
    }
  }
  const EntityKey key = world.createEntity();
  const double patience =
      LEAST_PATIENCE + (EXTRA_PATIENCE * drawUniform(drawKey(world, key, GUEST_PURPOSE, 0)));
  world.emplace<SyntheticGuest>(key, SyntheticGuest{world.Tick + static_cast<uint64_t>(patience)});
  if (shops.empty()) {
    return;
  }
  const auto pick = static_cast<size_t>(drawUniform(drawKey(world, key, GUEST_PURPOSE, 1)) *
                                        static_cast<double>(shops.size()));
  const auto walk = static_cast<uint32_t>(
      1.0 + (LONGEST_WALK * drawUniform(drawKey(world, key, GUEST_PURPOSE, 2))));
  const ParkBox &shop = shops[pick];
  const bool forFood =
      sells(shop.Stock, Good::Food) &&
      (!sells(shop.Stock, Good::Drink) || drawUniform(drawKey(world, key, GUEST_PURPOSE, 3)) < 0.5);
  if (forFood) {
    createUnits<GuestVisits>(world, key, key, 1);
    sendUnits<GuestVisits>(world, key, shop.Key, key, 1, walk);
  } else {
    createUnits<DrinkVisits>(world, key, key, 1);
    sendUnits<DrinkVisits>(world, key, shop.Key, key, 1, walk);
  }
}

// The backstage segment is deleted CUT_TICK ticks into every CUT_PERIOD and drawn again at the
// start of the next.
constexpr uint64_t CUT_PERIOD = 1200;
constexpr uint64_t CUT_TICK = 600;

// tests/parks/routes.park's backstage segment, which joins its shop's back door to its depot.
AddPath supplyPath() {
  return AddPath{PathKind::Backstage, {NewPoint{{12.0, 122.0}}, NewPoint{{12.0, 87.0}}}};
}

// routes.park's shop, depot, and backstage segment, and a shop far from any backstage segment.
// The first shop sells food and drink.
void populateFoodShop(World &world) {
  applyCommand(world, AddBox{BoxKind::Shop, Pose{6.5, 115.0, -1.0, 0.0}});
  applyCommand(world, SetShopStock{parkBoxes(world).front().Key, ShopStock::Both});
  applyCommand(world, AddBox{BoxKind::Depot, Pose{12.0, 80.0, 0.0, 1.0}});
  applyCommand(world, AddBox{BoxKind::Shop, Pose{-40.0, 60.0, 0.0, 1.0}});
  applyCommand(world, supplyPath());
}

// Deletes the backstage segment CUT_TICK ticks into each CUT_PERIOD, and draws it again at the
// start of the next.
void queueCut(const World &world, CommandQueue &commands) {
  const uint64_t phase = world.Tick % CUT_PERIOD;
  if (phase != CUT_TICK && (phase != 0 || world.Tick == 0)) {
    return;
  }
  std::vector<EntityKey> backstage;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Backstage) {
      backstage.push_back(segment.Key);
    }
  }
  if (phase == CUT_TICK && !backstage.empty()) {
    commands.push(DeleteSegment{backstage.front()});
  } else if (phase == 0 && backstage.empty()) {
    commands.push(supplyPath());
  }
}

std::shared_ptr<const WorldSchema> makeFoodShopSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addPark(*schema);
  schema->addComponent<SyntheticGuest>("synthetic-guest", DataKind::State);
  schema->addSystem(&stepGuests);
  return schema;
}

} // namespace

Scenario foodShopScenario() {
  return Scenario{
      .Name = "food-shop",
      .Seed = 5005,
      .MakeSchema = makeFoodShopSchema,
      .Populate = populateFoodShop,
      .QueueCommands = queueCut,
  };
}

} // namespace tpj
