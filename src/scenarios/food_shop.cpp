#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/furniture/furniture.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/mix.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"
#include "sim/operations/seats.h"
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
// A new guest visits the furnished path's first bench with this chance, and otherwise a drawn box.
constexpr double SEAT_CHANCE = 0.5;
// A guest waits at least LEAST_PATIENCE ticks for its visit to come back, and up to EXTRA_PATIENCE
// more.
constexpr double LEAST_PATIENCE = 100.0;
constexpr double EXTRA_PATIENCE = 1500.0;
// A visit takes 1 to 1 + LONGEST_WALK ticks to reach its box.
constexpr double LONGEST_WALK = 60.0;
// The causes a guest consumes a drink, a rest, and a toilet use with.
constexpr std::string_view DRUNK_CAUSE = "drunk";
constexpr std::string_view RESTED_CAUSE = "rested";
constexpr std::string_view USED_CAUSE = "used";

// The guest consumes every unit of the kind it holds, with the cause.
template <typename K> void consumeHeld(World &world, EntityKey guest, std::string_view cause) {
  const int64_t units = unitsHeld<K>(world, guest, guest);
  if (units > 0) {
    consumeUnits<K>(world, guest, guest, units, cause);
  }
}

// The guest sends one visit to the box, arriving after the walk: to a bench for rest, to a toilet
// block for the toilet, and to a shop for food or drink, whichever it sells, drawn when it sells
// both.
void sendVisit(World &world, EntityKey key, const ParkBox &target, uint32_t walk) {
  if (target.Kind == BoxKind::Bench) {
    createUnits<RestVisits>(world, key, key, 1);
    sendUnits<RestVisits>(world, key, target.Key, key, 1, walk);
    return;
  }
  if (target.Kind == BoxKind::ToiletBlock) {
    createUnits<ToiletVisits>(world, key, key, 1);
    sendUnits<ToiletVisits>(world, key, target.Key, key, 1, walk);
    return;
  }
  const bool forFood =
      sells(target.Stock, Good::Food) && (!sells(target.Stock, Good::Drink) ||
                                          drawUniform(drawKey(world, key, GUEST_PURPOSE, 3)) < 0.5);
  if (forFood) {
    createUnits<GuestVisits>(world, key, key, 1);
    sendUnits<GuestVisits>(world, key, target.Key, key, 1, walk);
  } else {
    createUnits<DrinkVisits>(world, key, key, 1);
    sendUnits<DrinkVisits>(world, key, target.Key, key, 1, walk);
  }
}

// Guests whose visit has come back consume it, with any serving, and leave, and guests out of
// patience leave without it. Then a guest may appear and send a visit to a drawn shop, bench, or
// toilet block.
void stepGuests(World &world) {
  std::vector<EntityKey> leaving;
  for (const EntityKey key : world.keys()) {
    const auto *guest = world.tryGet<SyntheticGuest>(key);
    if (guest == nullptr) {
      continue;
    }
    const int64_t visits = unitsHeld<GuestVisits>(world, key, key);
    const int64_t drinkVisits = unitsHeld<DrinkVisits>(world, key, key);
    const int64_t restVisits = unitsHeld<RestVisits>(world, key, key);
    const int64_t toiletVisits = unitsHeld<ToiletVisits>(world, key, key);
    const int64_t seatVisits = unitsHeld<SeatVisits>(world, key, key);
    if (visits > 0 || drinkVisits > 0 || restVisits > 0 || toiletVisits > 0 || seatVisits > 0) {
      consumeHeld<GuestVisits>(world, key, FINISHED_CAUSE);
      consumeHeld<DrinkVisits>(world, key, FINISHED_CAUSE);
      consumeHeld<RestVisits>(world, key, FINISHED_CAUSE);
      consumeHeld<ToiletVisits>(world, key, FINISHED_CAUSE);
      consumeHeld<SeatVisits>(world, key, FINISHED_CAUSE);
      consumeHeld<Meals>(world, key, EATEN_CAUSE);
      consumeHeld<Drinks>(world, key, DRUNK_CAUSE);
      consumeHeld<Rests>(world, key, RESTED_CAUSE);
      consumeHeld<ToiletUses>(world, key, USED_CAUSE);
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
  std::vector<ParkBox> visited;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind != BoxKind::Depot) {
      visited.push_back(box);
    }
  }
  const EntityKey key = world.createEntity();
  const double patience =
      LEAST_PATIENCE + (EXTRA_PATIENCE * drawUniform(drawKey(world, key, GUEST_PURPOSE, 0)));
  world.emplace<SyntheticGuest>(key, SyntheticGuest{world.Tick + static_cast<uint64_t>(patience)});
  const auto walk = static_cast<uint32_t>(
      1.0 + (LONGEST_WALK * drawUniform(drawKey(world, key, GUEST_PURPOSE, 2))));
  const std::vector<FurniturePiece> furniture = parkFurniture(world);
  if (!furniture.empty() && drawUniform(drawKey(world, key, GUEST_PURPOSE, 4)) < SEAT_CHANCE) {
    createUnits<SeatVisits>(world, key, key, 1);
    sendUnits<SeatVisits>(world, key, furniture.front().Key, key, 1, walk);
    return;
  }
  if (visited.empty()) {
    return;
  }
  const auto pick = static_cast<size_t>(drawUniform(drawKey(world, key, GUEST_PURPOSE, 1)) *
                                        static_cast<double>(visited.size()));
  sendVisit(world, key, visited[pick], walk);
}

// The backstage segment is deleted CUT_TICK ticks into every CUT_PERIOD and drawn again at the
// start of the next.
constexpr uint64_t CUT_PERIOD = 1200;
constexpr uint64_t CUT_TICK = 600;

// tests/parks/routes.park's backstage segment, which joins its shop's back door to its depot.
AddPath supplyPath() {
  return AddPath{PathKind::Backstage, {NewPoint{{12.0, 122.0}}, NewPoint{{12.0, 87.0}}}};
}

// routes.park's shop, depot, and backstage segment, a shop far from any backstage segment, and a
// bench and a toilet block beside it. The first shop sells food and drink. The bench's footprint,
// x -41 to -39 and z 44.5 to 45.5, and the toilet block's, x -64 to -56 and z 57.5 to 62.5, clear
// the second shop's, x -44 to -36 and z 57 to 63. A guest path along z = -20, clear of every box,
// carries benches on both sides every 10 m.
void populateFoodShop(World &world) {
  applyCommand(world, AddBox{BoxKind::Shop, Pose{6.5, 115.0, -1.0, 0.0}});
  applyCommand(world, SetShopStock{parkBoxes(world).front().Key, ShopStock::Both});
  applyCommand(world, AddBox{BoxKind::Depot, Pose{12.0, 80.0, 0.0, 1.0}});
  applyCommand(world, AddBox{BoxKind::Shop, Pose{-40.0, 60.0, 0.0, 1.0}});
  applyCommand(world, AddBox{BoxKind::Bench, Pose{-40.0, 45.0, 0.0, 1.0}});
  applyCommand(world, AddBox{BoxKind::ToiletBlock, Pose{-60.0, 60.0, 0.0, 1.0}});
  applyCommand(world, supplyPath());
  applyCommand(world, AddPath{PathKind::Guest, {NewPoint{{-40.0, -20.0}}, NewPoint{{0.0, -20.0}}}});
  const SideFurnishing benches{FurnitureKind::Bench, 10.0};
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Guest) {
      applyCommand(world, SetFurnishing{segment.Key, Furnishing{benches, benches}});
    }
  }
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
