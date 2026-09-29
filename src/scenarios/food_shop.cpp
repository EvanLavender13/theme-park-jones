#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
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
constexpr std::string_view FINISHED_CAUSE = "finished";
constexpr std::string_view EATEN_CAUSE = "eaten";

// Guests whose visit has come back consume it, with any meal, and leave, and guests out of
// patience leave without it. Then a guest may appear and send a visit to a drawn shop.
void stepGuests(World &world) {
  std::vector<EntityKey> leaving;
  for (const EntityKey key : world.keys()) {
    const auto *guest = world.Registry.try_get<SyntheticGuest>(world.findEntity(key));
    if (guest == nullptr) {
      continue;
    }
    const int64_t visits = unitsHeld<GuestVisits>(world, key, key);
    if (visits > 0) {
      consumeUnits<GuestVisits>(world, key, key, visits, FINISHED_CAUSE);
      const int64_t meals = unitsHeld<Meals>(world, key, key);
      if (meals > 0) {
        consumeUnits<Meals>(world, key, key, meals, EATEN_CAUSE);
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
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  const EntityKey key = world.createEntity();
  const double patience =
      LEAST_PATIENCE + (EXTRA_PATIENCE * drawUniform(drawKey(world, key, GUEST_PURPOSE, 0)));
  world.Registry.emplace<SyntheticGuest>(
      world.findEntity(key), SyntheticGuest{world.Tick + static_cast<uint64_t>(patience)});
  if (shops.empty()) {
    return;
  }
  const auto pick = static_cast<size_t>(drawUniform(drawKey(world, key, GUEST_PURPOSE, 1)) *
                                        static_cast<double>(shops.size()));
  const auto walk = static_cast<uint32_t>(
      1.0 + (LONGEST_WALK * drawUniform(drawKey(world, key, GUEST_PURPOSE, 2))));
  createUnits<GuestVisits>(world, key, key, 1);
  sendUnits<GuestVisits>(world, key, shops[pick], key, 1, walk);
}

// The backstage path is deleted CUT_TICK ticks into every CUT_PERIOD and drawn again at the start
// of the next.
constexpr uint64_t CUT_PERIOD = 1200;
constexpr uint64_t CUT_TICK = 600;

// tests/parks/routes.park's backstage path, which joins its shop's back door to its depot.
AddPath supplyPath() { return AddPath{PathKind::Backstage, {{12.0, 122.0}, {12.0, 87.0}}}; }

// routes.park's shop, depot, and backstage path, and a shop far from any backstage path.
void populateFoodShop(World &world) {
  applyCommand(world, AddBox{BoxKind::Shop, Pose{6.5, 115.0, -1.0, 0.0}});
  applyCommand(world, AddBox{BoxKind::Depot, Pose{12.0, 80.0, 0.0, 1.0}});
  applyCommand(world, AddBox{BoxKind::Shop, Pose{-40.0, 60.0, 0.0, 1.0}});
  applyCommand(world, supplyPath());
}

// Deletes the backstage path CUT_TICK ticks into each CUT_PERIOD, and draws it again at the start
// of the next.
void queueCut(const World &world, CommandQueue &commands) {
  const uint64_t phase = world.Tick % CUT_PERIOD;
  if (phase != CUT_TICK && (phase != 0 || world.Tick == 0)) {
    return;
  }
  std::vector<EntityKey> backstage;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Backstage) {
      backstage.push_back(path.Key);
    }
  }
  if (phase == CUT_TICK && !backstage.empty()) {
    commands.push(DeletePath{backstage.front()});
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
