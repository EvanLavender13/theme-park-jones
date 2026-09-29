# Implementation Plan: Meal Service

## Goal

Make shops queue guest visits, serve them one per SERVICE_INTERVAL by converting supplies into meals sent with the visit, return what a starved shop cannot cover, and abandon what gone guests leave, with a food-shop scenario that puts service in the cross-build check.

## Approach

A private state component, ShopService, on each shop box's entity holds the queue and the tick the shop is next free. stepShops, after ordering, abandons gone guests' units, reconciles the queue with the visits the shop holds, returns what a starved shop cannot cover, and serves the front guest, so each decision stays a function of the world between cycles. makeParkSchema's registrations move into addPark, and the food-shop scenario registers synthetic guests after them.

## Tasks

### Task 1: Describe service in the operations spec

Files:
- Modify: `src/sim/operations/SPEC.md`

Step 1: Replace the Registration section's paragraph with:

```
addOperations registers the flow kinds SupplyOrders, named supply-orders, Supplies, named supplies, GuestVisits, named guest-visits, and Meals, named meals, in that order, then the state component type ShopService, named shop-service, and then the systems stepShops and stepDepots, in that order, so shops step before depots. addPark calls it after addRoutes. GuestVisits and Meals are declared in operations.h for believable-guests, which sends visits and receives meals. ShopService holds a shop's queue. It is private to the module, declared in operations/internal/shop_service.h. Everything else a shop or depot knows between cycles is in the ledgers, intent, and the fields.
```

Step 2: In the Orders and supplies section, at the end of the paragraph that begins "Each cycle, stepShops takes each shop", after "A starved shop sends no orders.", add:

```
Then the shop serves, as the Service section describes.
```

Step 3: In that section's last paragraph, replace "every consumed supplies unit as returned, undeliverable, or discarded" with "every consumed supplies unit as returned, served, undeliverable, or discarded".

Step 4: Append this section at the end of the file:

```markdown
## Service

A guest sends a shop a visit: one guest-visits unit, with the guest's key as its handle. The shop sends every visit it holds back to its handle, the guest, unless it consumes it as abandoned. A meal is one meals unit the shop creates under the guest's handle when it serves the guest and sends to the guest with the visit, in the same tick and with the same delay. So a returned visit that arrives with a meal was served, and one that arrives without one was not.

A shop's ShopService, on its box's entity, holds Queue, one guest key for each visit unit the shop holds, in the order it took them in, and FreeAt, the first tick at which it may serve. A shop box without one has an empty queue and a FreeAt of 0. The shop's step stores one once its queue is not empty or its FreeAt is above 0, and it stays while the box lives. Only a shop box's ShopService is read.

After ordering, stepShops serves for the shop, with t the tick being stepped:

1. The shop consumes every meals unit it holds, with the cause abandoned, since only a meal whose guest is gone comes back to it. It consumes every guest-visits unit it holds under a handle that names no live entity, with the cause abandoned. Keys are never reused, so such a unit could never be returned.
2. It brings its queue up to date. For each guest, it keeps that guest's earliest entries, up to the visit units it now holds under the guest, and then appends one entry for each further unit it holds, guests in ascending key order. So visits are queued in the order they arrived, and visits that arrived at the same swap in guest key order.
3. A starved shop's cover is the supplies it holds under its own handle plus the units of the supplies packets addressed to it whose destination is the shop. It keeps the queue's first cover entries, removes the rest, and sends each removed guest's visit units back to it in one packet, with the guest as handle and the delay RETURN_DELAY, 1 tick, and no meal. Shipments in transit always arrive, since the ledger never reads a network, so a starved shop turns away only the guests it could never serve. A supplied shop returns no visit, however long its queue.
4. When the queue is not empty, t is FreeAt or later, and the shop holds at least one supplies unit under its own handle, it serves the guest at the queue's front. It removes that entry, consumes one supplies unit with the cause served, creates one meal under the guest's handle, and sends the meal and one of the guest's visit units to the guest, each as one unit with the guest as handle and the delay SERVICE_INTERVAL, 90 ticks, the time service takes. FreeAt becomes t + SERVICE_INTERVAL. So a shop serves at most one guest per SERVICE_INTERVAL, and its meals are limited by the scarcest of demand, supply, and the service rate.

Serving reads only the shop's own stocks and ShopService, the supplies addressed to it, nearestDepot, whether its guests' keys are live, and intent, so like ordering it is a function of the world between cycles.

The ledger settles the rest. A deleted shop's held visits go to their guests' stocks with no meal, so they read as unserved, and its ShopService goes with its entity. A visit or meal sent to a guest that is gone comes back to the shop, which consumes it as abandoned, or is consumed as undeliverable when the shop is gone too. So the meals created equal the supplies consumed as served, and a shop consumes guest-visits and meals only as abandoned.
```

### Task 2: Describe addPark in the sim spec

Files:
- Modify: `src/sim/SPEC.md` (the paragraph that begins "Component types are registered with a WorldSchema")

Step 1: Replace "The park's schema comes from one function, makeParkSchema in sim/park_schema.h, where each capability's component types, systems, swap functions, resolvers, and commands are registered in a written order. Park files are loaded with it. It registers the medium's types first" with:

```
The park's schema comes from makeParkSchema in sim/park_schema.h, which returns a schema holding exactly what addPark registers: each capability's component types, systems, swap functions, resolvers, and commands, in a written order. Park files are loaded with it, and a scenario that adds synthetic entities to the park, such as food-shop's guests, calls addPark and registers them after. addPark registers the medium's types first
```

### Task 3: Describe the food-shop scenario

Files:
- Modify: `src/scenarios/SPEC.md` (the Contract section's first paragraph after the tpj_scenarios_lib paragraph, which begins "A Scenario has a name")

Step 1: After "So the physical-validity check's answers, and the orders and shipments of the shops and depots it places, are compared across builds.", add:

```
food-shop makes its schema with addPark, then the state component synthetic-guest and a system that steps guests after the park's systems. It populates its world by applying park commands: a shop, a depot, and a backstage path joining them, as in tests/parks/routes.park, and a second shop far from any backstage path. Each cycle, every guest whose visit has come back consumes it, with any meal that came with it, and leaves, and every guest whose drawn patience has run out leaves without it. Then, at a keyed draw's chance of 1 in 45, a guest appears with a drawn patience and sends one visit to a drawn shop box, with a drawn walking delay. Before the cycle 600 ticks into every 1200, it deletes the backstage path, and before the cycle at the start of each later 1200 it draws the path again. So service, unserved returns, starving, and abandoned visits are compared across builds.
```

### Task 4: Declare the service constants

Files:
- Modify: `src/sim/operations/operations.h` (after `SUPPLY_SPEED`, and after `RETURNED_CAUSE`)

Step 1: After the `SUPPLY_SPEED` declaration, add:

```cpp
// Ticks service takes: a served guest's meal and visit arrive this long after the shop takes it,
// and the shop takes no other guest until then.
inline constexpr uint32_t SERVICE_INTERVAL = 90;
// Ticks an unserved visit takes to go back to its guest.
inline constexpr uint32_t RETURN_DELAY = 1;
```

Step 2: After `RETURNED_CAUSE`, add:

```cpp
inline constexpr std::string_view SERVED_CAUSE = "served";
inline constexpr std::string_view ABANDONED_CAUSE = "abandoned";
```

Step 3: Change the comment above `addOperations` to:

```cpp
// Registers the flow kinds supply-orders, supplies, guest-visits, and meals, the shop-service
// state, then the systems that step shops and then depots.
```

### Task 5: Move the park's registrations into addPark

Files:
- Modify: `src/sim/park_schema.h`
- Modify: `src/sim/park_schema.cpp`

Step 1: In park_schema.h, after the `makeParkSchema` declaration, add:

```cpp
// Registers every capability's component types, systems, swap functions, resolvers, and commands
// for the park, in a written order. A scenario that adds synthetic entities to the park registers
// them after these.
void addPark(WorldSchema &schema);
```

and change makeParkSchema's comment to:

```cpp
// The park's schema: exactly what addPark registers. Park files are loaded with it.
```

Step 2: Replace park_schema.cpp's function with:

```cpp
void addPark(WorldSchema &schema) {
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them, in dependency order, starting with park intent and its commands,
  // then the routes that derive the park's networks, then the operations that run its shops and
  // depots.
  addNetworkComponent(schema);
  addParkIntent(schema);
  addParkEdits(schema);
  addRoutes(schema);
  addOperations(schema);
}

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addPark(*schema);
  return schema;
}
```

### Task 6: Declare the food-shop scenario with a stub

Files:
- Create: `src/scenarios/food_shop.cpp`
- Modify: `src/scenarios/synthetic.h`
- Modify: `src/scenarios/scenarios.cpp`
- Modify: `src/scenarios/CMakeLists.txt`

Step 1: In synthetic.h, after `Scenario parkEditsScenario();`, add `Scenario foodShopScenario();`.

Step 2: Create food_shop.cpp:

```cpp
#include "scenarios/synthetic.h"
#include "sim/park_schema.h"
#include "sim/world.h"

namespace tpj {

Scenario foodShopScenario() {
  return Scenario{
      .Name = "food-shop",
      .Seed = 5005,
      .MakeSchema = makeParkSchema,
      .Populate = [](World & /*world*/) {},
      .QueueCommands = nullptr,
  };
}

} // namespace tpj
```

Step 3: In scenarios.cpp, make the array `std::array<Scenario, 5>` and append `foodShopScenario()` after `parkEditsScenario()`.

Step 4: In src/scenarios/CMakeLists.txt, add `food_shop.cpp` to tpj_scenarios_lib's sources, before `park_edits.cpp`.

Step 5: Build.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i; cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output. (Use a 600000 ms timeout.)

### Task 7: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, src/sim/operations/SPEC.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, src/scenarios/SPEC.md, and the headers src/sim/operations/operations.h, src/sim/medium/flow.h, src/sim/park_schema.h, src/scenarios/scenarios.h, and src/scenarios/synthetic.h.

### Task 8: Declare and register ShopService

Files:
- Create: `src/sim/operations/internal/shop_service.h`
- Modify: `src/sim/operations/operations.cpp`

Step 1: Create shop_service.h:

```cpp
#ifndef TPJ_SIM_OPERATIONS_INTERNAL_SHOP_SERVICE_H
#define TPJ_SIM_OPERATIONS_INTERNAL_SHOP_SERVICE_H

#include "sim/entity_key.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// State, on a shop box's entity: one guest key for each visit unit the shop holds, in the order it
// took them in, and the first tick at which it may serve.
struct ShopService {
  std::vector<EntityKey> Queue;
  uint64_t FreeAt = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, ShopService &service) {
  visitor.field("queue", service.Queue);
  visitor.field("free-at", service.FreeAt);
}

} // namespace tpj

#endif
```

Step 2: In operations.cpp, include `"sim/operations/internal/shop_service.h"` with the project headers, and in addOperations, after `addFlow<Meals>(schema);`, add:

```cpp
  schema.addComponent<ShopService>("shop-service", DataKind::State);
```

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no warnings. The registration test of criterion 1 passes; the service tests still fail.

### Task 9: Split ordering out of stepShops

Files:
- Modify: `src/sim/operations/operations.cpp:47-65`

Step 1: Replace stepShops with an ordering function and a stepShops that calls it, so serving can follow:

```cpp
// The shop takes back the orders returned to it, and when it has a nearest depot and its
// inventory position is at the reorder point or below, orders up to ORDER_UP_TO from that depot.
void orderSupplies(World &world, EntityKey shop, const std::optional<DepotRoute> &route) {
  const int64_t returned = unitsHeld<SupplyOrders>(world, shop, shop);
  if (returned > 0) {
    consumeUnits<SupplyOrders>(world, shop, shop, returned, UNFILLED_CAUSE);
  }
  const int64_t position = inventoryPosition(world, shop);
  if (!route || position > REORDER_POINT) {
    return;
  }
  const int64_t wanted = ORDER_UP_TO - position;
  createUnits<SupplyOrders>(world, shop, shop, wanted);
  sendUnits<SupplyOrders>(world, shop, route->Depot, shop, wanted, ORDER_DELAY);
}

// Each shop, in ascending key order, orders supplies and then serves its guests.
void stepShops(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    const std::optional<DepotRoute> route = nearestDepot(world, shop);
    orderSupplies(world, shop, route);
    serveGuests(world, shop, route.has_value());
  }
}
```

with an empty `void serveGuests(World & /*world*/, EntityKey /*shop*/, bool /*supplied*/) {}` above stepShops. nearestDepot is declared in operations.h, so it can be called from the anonymous namespace.

Step 2: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no warnings; every supply-chain test still passes.

### Task 10: Abandon the units of gone guests

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Add `<map>` to the system includes. Above serveGuests, add:

```cpp
bool isLive(const World &world, EntityKey key) {
  return key != NULL_KEY && world.findEntity(key) != entt::null;
}

// Consumes, as abandoned, every meal the shop holds, which only a gone guest's returned meal gives
// it, and every visit it holds for a guest that is gone.
void abandonGone(World &world, EntityKey shop) {
  for (const FlowHolding &held : stockOf<Meals>(world, shop)) {
    consumeUnits<Meals>(world, shop, held.Handle, held.Units, ABANDONED_CAUSE);
  }
  for (const FlowHolding &held : stockOf<GuestVisits>(world, shop)) {
    if (!isLive(world, held.Handle)) {
      consumeUnits<GuestVisits>(world, shop, held.Handle, held.Units, ABANDONED_CAUSE);
    }
  }
}
```

Step 2: Make serveGuests call `abandonGone(world, shop);`, keeping its other parameters unnamed.

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no warnings; criterion 3's tests pass.

### Task 11: Keep the queue

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Above serveGuests, add:

```cpp
// The queue brought up to the visits the shop holds: each guest's earliest entries, up to its
// held units, then one entry for each further unit, guests in ascending key order.
std::vector<EntityKey> reconciledQueue(const World &world, EntityKey shop,
                                       const std::vector<EntityKey> &queue) {
  const std::vector<FlowHolding> held = stockOf<GuestVisits>(world, shop);
  const auto heldUnder = [&held](EntityKey guest) -> int64_t {
    const auto at = std::ranges::lower_bound(held, guest, {}, &FlowHolding::Handle);
    return at != held.end() && at->Handle == guest ? at->Units : 0;
  };
  std::map<EntityKey, int64_t> kept;
  std::vector<EntityKey> reconciled;
  for (const EntityKey guest : queue) {
    if (kept[guest] < heldUnder(guest)) {
      ++kept[guest];
      reconciled.push_back(guest);
    }
  }
  for (const FlowHolding &holding : held) {
    for (int64_t unit = kept[holding.Handle]; unit < holding.Units; ++unit) {
      reconciled.push_back(holding.Handle);
    }
  }
  return reconciled;
}
```

Step 2: Replace serveGuests with:

```cpp
// The shop abandons what gone guests left, queues the visits it holds in arrival order, returns
// what a starved shop cannot cover, and serves the guest at the front when it can.
void serveGuests(World &world, EntityKey shop, bool /*supplied*/) {
  abandonGone(world, shop);
  const entt::entity entity = world.findEntity(shop);
  const ShopService *stored = world.Registry.try_get<ShopService>(entity);
  ShopService service = stored == nullptr ? ShopService{} : *stored;
  service.Queue = reconciledQueue(world, shop, service.Queue);
  if (stored != nullptr || !service.Queue.empty() || service.FreeAt != 0) {
    world.Registry.emplace_or_replace<ShopService>(entity, std::move(service));
  }
}
```

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no warnings; criterion 3's tests still pass. Queue order is observable only once shops serve or return, so criterion 2's tests pass after Task 13.

### Task 12: Return what a starved shop cannot cover

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Above serveGuests, add:

```cpp
// The supplies the shop holds under its own handle and those on their way to it.
int64_t supplyCover(const World &world, EntityKey shop) {
  int64_t cover = unitsHeld<Supplies>(world, shop, shop);
  for (const FlowPacket &packet : addressedTo<Supplies>(world, shop).Packets) {
    if (packet.To == shop) {
      cover += packet.Units;
    }
  }
  return cover;
}

// Keeps the queue's first entries up to the shop's cover and sends the rest back unserved, one
// packet per guest.
void returnUncovered(World &world, EntityKey shop, std::vector<EntityKey> &queue) {
  const int64_t cover = supplyCover(world, shop);
  if (std::cmp_less_equal(queue.size(), cover)) {
    return;
  }
  std::map<EntityKey, int64_t> returned;
  for (auto at = queue.begin() + cover; at != queue.end(); ++at) {
    ++returned[*at];
  }
  queue.resize(static_cast<size_t>(cover));
  for (const auto &[guest, units] : returned) {
    sendUnits<GuestVisits>(world, shop, guest, guest, units, RETURN_DELAY);
  }
}
```

Step 2: In serveGuests, name the parameter `supplied`, and after the reconciledQueue line add:

```cpp
  if (!supplied) {
    returnUncovered(world, shop, service.Queue);
  }
```

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no warnings; criterion 5's tests pass except those that also need serving.

### Task 13: Serve the front guest

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Above serveGuests, add:

```cpp
// When the shop is free and holds a supply, it converts one into a meal for the guest at the
// front, and sends the meal and the visit to the guest after the time service takes.
void serveFront(World &world, EntityKey shop, ShopService &service) {
  if (service.Queue.empty() || world.Tick < service.FreeAt ||
      unitsHeld<Supplies>(world, shop, shop) < 1) {
    return;
  }
  const EntityKey guest = service.Queue.front();
  service.Queue.erase(service.Queue.begin());
  consumeUnits<Supplies>(world, shop, shop, 1, SERVED_CAUSE);
  createUnits<Meals>(world, shop, guest, 1);
  sendUnits<Meals>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
  sendUnits<GuestVisits>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
  service.FreeAt = world.Tick + SERVICE_INTERVAL;
}
```

Step 2: In serveGuests, after the starved return, add `serveFront(world, shop, service);`.

Step 3: Build and run the operations tests, one file per run.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; for f in $(ls tests/sim/operations/*_test.cpp | xargs -n1 basename | sed 's/\.cpp$//'); do build/linux-debug/tpj_sim_tests -# "[#$f]" | tail -2; done`
Expected: no warnings; every test in tests/sim/operations passes, covering criteria 1 to 7.

### Task 14: Give food-shop its schema and guests

Files:
- Modify: `src/scenarios/food_shop.cpp`

Step 1: Replace the file with the scenario below.

```cpp
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
      .Populate = [](World & /*world*/) {},
      .QueueCommands = nullptr,
  };
}

} // namespace tpj
```

Step 2: Build.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i; cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output. (600000 ms timeout.)

### Task 15: Populate food-shop and cut its route

Files:
- Modify: `src/scenarios/food_shop.cpp`

Step 1: In the anonymous namespace, after stepGuests, add:

```cpp
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
```

Step 2: Set `.Populate = populateFoodShop` and `.QueueCommands = queueCut`.

Step 3: Build and run the scenarios tests and the runner.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_scenarios_tests | tail -3`
Expected: no warnings; every scenarios test passes, including criterion 8's. If a food-shop test finds no abandoned visits or no unserved returns in the default run, that is a Minor deviation: adjust LEAST_PATIENCE or SPAWN_CHANCE, note it, and rerun.

### Task 16: Confirm the acceptance criteria

Run, from WSL at the repository root, with 600000 ms timeouts:

- `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
- `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`: expected no output.
- `ctest --preset linux-debug`: expected 100% tests passed.
- `cmake.exe --build --preset windows-debug`: expected to build.
- `ctest.exe --preset windows-debug`: expected 100% tests passed.
- `scripts/cross-build-check.sh`: expected to pass, with food-shop's lines in the compared output.

### Task 17: Commit

Review as implementing-features describes, then commit via commit-hygiene, staging these paths only (never parks/ or image.png):

- src/sim/operations/SPEC.md, src/sim/operations/operations.h, src/sim/operations/operations.cpp, src/sim/operations/internal/shop_service.h
- src/sim/park_schema.h, src/sim/park_schema.cpp, src/sim/SPEC.md
- src/scenarios/food_shop.cpp, src/scenarios/synthetic.h, src/scenarios/scenarios.cpp, src/scenarios/CMakeLists.txt, src/scenarios/SPEC.md
- the test pass's files under tests/

Subject: `Operations: Serve guest visits with meals from supplies`
