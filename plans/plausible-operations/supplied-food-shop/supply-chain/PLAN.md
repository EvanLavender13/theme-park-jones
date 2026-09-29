# Implementation Plan: Supply Chain

## Goal

Create src/sim/operations with its four flow kinds, (s, S) ordering by shops and shipping by depots over backstage route distance, and add the ledger's addressedTo query.

## Approach

The module keeps no component of its own. Orders and supplies carry the shop's key as their handle, so a shop's inventory position is computed from addressedTo, and a depot acts on its own stock. Two systems, shops before depots, make each decision a function of the world between cycles. Supply routes come from sampling backstage-route-distance at anchored nodes, and depot and shop identities come from parkBoxes.

## Tasks

### Task 1: Write the operations spec

Files:
- Create: `src/sim/operations/SPEC.md`

Step 1: Write the file with this content.

```markdown
# operations

Operations: the shops and depots that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Its shops and depots meet only through the medium, the flow ledger and route distance, and neither reads the other's state (principles 3 and 6).

## Registration

addOperations registers the flow kinds SupplyOrders, named supply-orders, Supplies, named supplies, GuestVisits, named guest-visits, and Meals, named meals, in that order, and then the systems stepShops and stepDepots, in that order, so shops step before depots. makeParkSchema calls it after addRoutes. GuestVisits and Meals are declared in operations.h for believable-guests, which sends visits and receives meals. The module registers no component type: everything a shop or depot knows between cycles is in the ledgers, intent, and the fields.

## Supply routes

Every shop box of parkBoxes is a shop and every depot box a depot. supplyRouteLength(world, at, source) is the least Distance among the entries of the source that sampleField of backstage-route-distance on parkNetwork(world, Backstage) gives at the nodePlace of each node anchored to at, or none when there is none. nearestDepot(world, shop) is, among the depot boxes with a supplyRouteLength at the shop, the one with the least, ties to the lower key, as a DepotRoute of its key and that length, or none. A shop with a nearest depot is supplied, and one without is starved: a shop with no backstage connector, or whose connector reaches no depot, is placed and simply gets no supplies (principle 5).

shipmentDelay(distance) is a shipment's delay in ticks: ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS)), computed in that order, with SUPPLY_SPEED 2 m/s. A result below 1, or a NaN, gives 1, and a result above the largest uint32_t gives the largest uint32_t.

## Orders and supplies

A shop's orders, and the supplies shipped for them, carry the shop's key as their handle, so the units addressed to it are what it has on order. inventoryPosition(world, shop) adds, from addressedTo with the shop's key, the supplies stocks the shop holds, the supplies packets whose destination is the shop, the supply-orders packets whose destination is a depot box, and the supply-orders stocks held by depot boxes. So orders on their way to a depot, or held by one, count, and orders on their way back, back at the shop, or held by or heading to a deleted depot do not. The shop reorders from another depot as soon as its depot is deleted. Orders to a depot it can no longer reach still count until that depot sends them back, since a route restored before they arrive fills them, so the position never exceeds ORDER_UP_TO.

Each cycle, stepShops takes each shop in ascending key order. The shop consumes every order unit it holds under its own handle with the cause unfilled. Then, when it has a nearest depot and its inventoryPosition is REORDER_POINT, 8, or below, it creates ORDER_UP_TO, 24, minus that position order units under its own handle, and sends them to the nearest depot with its own handle and the delay ORDER_DELAY, 30 ticks. A starved shop sends no orders.

Then stepDepots takes each depot in ascending key order. The depot consumes every supplies unit it holds, which only a returned shipment gives it, with the cause returned. Then, for each handle it holds order units under, in ascending order, it acts on all of them. When the handle names no shop box, it consumes them with the cause cancelled. When supplyRouteLength(world, depot, handle) is none, it sends them back to the handle with that handle and the delay ORDER_DELAY. Otherwise it consumes them with the cause fulfilled, creates as many supplies under the handle, and sends them to the handle with that handle and the delay shipmentDelay of that length. A depot is an unlimited source, and its supplies are created as it ships.

Every ledger operation changes only its own endpoint's stock, and shops step before depots, so each decision is a function of the world as it stood between cycles: its ledgers, intent, and fields. An order sent while stepping tick t reaches its depot's stock at t + ORDER_DELAY, the depot ships it while stepping that tick, and the supplies reach the shop's stock at t + ORDER_DELAY plus the shipment's delay.

The ledger settles what the systems leave. A deleted depot's held orders go to their shop, and orders in transit to it return to their shop, which consumes both as unfilled. A deleted shop's supplies are discarded, and shipments in transit to it return to their depot, which consumes them as returned, or are consumed as undeliverable when the depot is gone too. So every consumed order unit was consumed as fulfilled, unfilled, cancelled, undeliverable, or discarded, every consumed supplies unit as returned, undeliverable, or discarded, and the supplies created equal the orders consumed as fulfilled.
```

### Task 2: Describe addressedTo in the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md` (the last paragraph of the Flows section, which begins "The queries take the world by const reference")

Step 1: After the sentence "unitsInTransit, unitsCreated, and unitsConsumed give the kind's totals, and unitsConsumed with a cause gives the units consumed with it, 0 for a cause never used.", insert:

```
addressedTo gives the units addressed to an entity, as a FlowAddressed: the kind's packets whose handle is its key, in the ledger's packet order, and its stocks whose handle is its key, in the ledger's stock order.
```

### Task 3: Register operations in the sim spec and the scenarios spec

Files:
- Modify: `src/sim/SPEC.md` (the paragraph beginning "Component types are registered")
- Modify: `src/scenarios/SPEC.md` (the paragraph beginning "A Scenario has a name")

Step 1: In src/sim/SPEC.md, replace "then the routes module's resolvers and fields, which derive the park's networks and route distance (sim/routes/SPEC.md)." with:

```
then the routes module's resolvers and fields, which derive the park's networks and route distance (sim/routes/SPEC.md), and then the operations module's flow kinds and systems, which run its shops and depots (sim/operations/SPEC.md).
```

Step 2: In src/scenarios/SPEC.md, replace "So the physical-validity check's answers are compared across builds." with:

```
So the physical-validity check's answers, and the orders and shipments of the shops and depots it places, are compared across builds.
```

### Task 4: Declare addressedTo with a stub

Files:
- Modify: `src/sim/medium/flow.h`

Step 1: Add `#include <iterator>` to the system includes, in sorted position after `<concepts>`.

Step 2: After the definition of `unitsConsumed(const World &world, std::string_view cause)`, before the closing `} // namespace tpj`, add:

```cpp
// The units of a kind addressed to an entity: the packets and stocks whose handle is its key.
struct FlowAddressed {
  std::vector<FlowPacket> Packets;
  std::vector<FlowStock> Stocks;
};

// Packets and stocks each in the ledger's order.
template <FlowDefinition K>
FlowAddressed addressedTo(const World & /*world*/, EntityKey /*handle*/) {
  return {};
}
```

### Task 5: Create the operations header and stubs

Files:
- Create: `src/sim/operations/operations.h`
- Create: `src/sim/operations/operations.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`

Step 1: Write src/sim/operations/operations.h:

```cpp
#ifndef TPJ_SIM_OPERATIONS_OPERATIONS_H
#define TPJ_SIM_OPERATIONS_OPERATIONS_H

#include "sim/entity_key.h"

#include <optional>
#include <stdint.h>
#include <string_view>

namespace tpj {

class World;
class WorldSchema;

// Orders a shop sends a depot, one unit for each supply wanted, addressed to the shop.
struct SupplyOrders {
  static constexpr std::string_view Name = "supply-orders";
};

// Supplies a depot ships to a shop, addressed to the shop.
struct Supplies {
  static constexpr std::string_view Name = "supplies";
};

// A guest's visit to a shop, addressed to the guest, which the shop returns to it.
struct GuestVisits {
  static constexpr std::string_view Name = "guest-visits";
};

// A meal a shop sends the guest it served.
struct Meals {
  static constexpr std::string_view Name = "meals";
};

// A shop orders when its inventory position falls to the reorder point, up to the order-up-to
// level.
inline constexpr int64_t REORDER_POINT = 8;
inline constexpr int64_t ORDER_UP_TO = 24;
// Ticks an order takes to reach its depot.
inline constexpr uint32_t ORDER_DELAY = 30;
// Meters per second supplies move along the backstage route.
inline constexpr double SUPPLY_SPEED = 2.0;

inline constexpr std::string_view FULFILLED_CAUSE = "fulfilled";
inline constexpr std::string_view UNFILLED_CAUSE = "unfilled";
inline constexpr std::string_view CANCELLED_CAUSE = "cancelled";
inline constexpr std::string_view RETURNED_CAUSE = "returned";

// A shop's nearest depot and the backstage route length to it.
struct DepotRoute {
  EntityKey Depot = NULL_KEY;
  double Distance = 0.0;

  bool operator==(const DepotRoute &) const = default;
};

// The least backstage route distance from the nodes anchored to at to the source, or none.
std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source);
// The depot box with the least supply route length from the shop, ties to the lower key, or none
// for a starved shop.
std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop);
// The ticks a shipment takes over a backstage route of the distance, at least 1.
uint32_t shipmentDelay(double distance);
// The supplies the shop holds or has coming, and the orders on their way to or held by a depot.
int64_t inventoryPosition(const World &world, EntityKey shop);
// Registers the flow kinds supply-orders, supplies, guest-visits, and meals, then the systems that
// step shops and then depots.
void addOperations(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 2: Write src/sim/operations/operations.cpp with stubs:

```cpp
#include "sim/operations/operations.h"

#include "sim/schema.h"
#include "sim/world.h"

namespace tpj {

std::optional<double> supplyRouteLength(const World & /*world*/, EntityKey /*at*/,
                                        EntityKey /*source*/) {
  return std::nullopt;
}

std::optional<DepotRoute> nearestDepot(const World & /*world*/, EntityKey /*shop*/) {
  return std::nullopt;
}

uint32_t shipmentDelay(double /*distance*/) { return 1; }

int64_t inventoryPosition(const World & /*world*/, EntityKey /*shop*/) { return 0; }

void addOperations(WorldSchema & /*schema*/) {}

} // namespace tpj
```

Step 3: In src/sim/CMakeLists.txt, add `operations/operations.cpp` to tpj_sim's sources, after `musl_log.cpp`.

Step 4: In src/sim/park_schema.cpp, include `"sim/operations/operations.h"` in sorted position, call `addOperations(*schema);` after `addRoutes(*schema);`, and end the comment's last sentence with "then the routes that derive the park's networks, then the operations that run its shops and depots."

Step 5: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 6: Run the test pass

Dispatch the test-writer agent per implementing-features, with FEATURE.md, the specs src/sim/operations/SPEC.md, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, and src/sim/SPEC.md, and the public headers src/sim/operations/operations.h and src/sim/medium/flow.h.

### Task 7: Implement addressedTo

Files:
- Modify: `src/sim/medium/flow.h`

Step 1: Replace the stub with:

```cpp
// Packets and stocks each in the ledger's order.
template <FlowDefinition K> FlowAddressed addressedTo(const World &world, EntityKey handle) {
  FlowAddressed addressed;
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return addressed;
  }
  std::ranges::copy_if(ledger->Packets, std::back_inserter(addressed.Packets),
                       [handle](const FlowPacket &packet) { return packet.Handle == handle; });
  std::ranges::copy_if(ledger->Stocks, std::back_inserter(addressed.Stocks),
                       [handle](const FlowStock &stock) { return stock.Handle == handle; });
  return addressed;
}
```

Step 2: Build and run the flow tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#flow_test]"`
Expected: no warnings, and all flow tests pass, including the addressedTo tests.

### Task 8: Register the flow kinds and systems

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Add the includes `"sim/medium/flow.h"`, `"sim/medium/field.h"`, `"sim/medium/network.h"`, `"sim/park/intent.h"`, `"sim/routes/networks.h"`, and `"sim/routes/route_distance.h"` in sorted order with the project includes, and `<algorithm>`, `<cmath>`, `<limits>`, `<utility>`, and `<vector>` as system includes.

Step 2: Above the public functions, add an anonymous namespace with empty systems, and replace addOperations:

```cpp
namespace {

void stepShops(World & /*world*/) {}

void stepDepots(World & /*world*/) {}

} // namespace
```

```cpp
void addOperations(WorldSchema &schema) {
  addFlow<SupplyOrders>(schema);
  addFlow<Supplies>(schema);
  addFlow<GuestVisits>(schema);
  addFlow<Meals>(schema);
  schema.addSystem(&stepShops);
  schema.addSystem(&stepDepots);
}
```

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests "[operations]"`
Expected: no warnings. The tests for criteria 1, 2, 7, and 8 pass, since with empty systems nothing moves; the tests for criteria 3 to 6 still fail. Use whatever tag or file filter the test pass gave the operations tests.

### Task 9: Implement supply routes and the shipment delay

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: In the anonymous namespace, before the systems, add:

```cpp
// The keys of the boxes of the kind, ascending.
std::vector<EntityKey> boxKeys(const World &world, BoxKind kind) {
  std::vector<EntityKey> keys;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == kind) {
      keys.push_back(box.Key);
    }
  }
  return keys;
}

// Each backstage route distance entry sampled at the nodes anchored to the entity, as its source
// and distance, in node order and then source order.
std::vector<std::pair<EntityKey, double>> routeLengthsAt(const World &world, EntityKey at) {
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::vector<std::pair<EntityKey, double>> lengths;
  for (const uint32_t node : network.anchoredNodes(at)) {
    for (const SampledEntry<RouteEntry> &entry : sampleField<RouteDistance<PathKind::Backstage>>(
             world, network, network.nodePlace(node))) {
      lengths.emplace_back(entry.Source, entry.Value.Distance);
    }
  }
  return lengths;
}
```

Step 2: Replace the stubs of supplyRouteLength, nearestDepot, and shipmentDelay:

```cpp
std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source) {
  std::optional<double> least;
  for (const auto &[from, distance] : routeLengthsAt(world, at)) {
    if (from == source && (!least || distance < *least)) {
      least = distance;
    }
  }
  return least;
}

std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  std::optional<DepotRoute> nearest;
  for (const auto &[source, distance] : routeLengthsAt(world, shop)) {
    if (!std::ranges::binary_search(depots, source)) {
      continue;
    }
    if (!nearest || distance < nearest->Distance ||
        (distance == nearest->Distance && source < nearest->Depot)) {
      nearest = DepotRoute{source, distance};
    }
  }
  return nearest;
}

uint32_t shipmentDelay(double distance) {
  const double ticks = std::ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS));
  // The negated test also takes a NaN to 1.
  if (!(ticks >= 1.0)) {
    return 1;
  }
  constexpr double LONGEST = static_cast<double>(std::numeric_limits<uint32_t>::max());
  return ticks >= LONGEST ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(ticks);
}
```

Step 3: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests "[operations]"`
Expected: no warnings. The tests for criteria 1 to 4, 7, and 8 pass.

### Task 10: Implement the inventory position

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Replace the stub of inventoryPosition:

```cpp
int64_t inventoryPosition(const World &world, EntityKey shop) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  const auto isDepot = [&depots](EntityKey key) { return std::ranges::binary_search(depots, key); };
  int64_t position = 0;
  const FlowAddressed supplies = addressedTo<Supplies>(world, shop);
  for (const FlowStock &stock : supplies.Stocks) {
    if (stock.Endpoint == shop) {
      position += stock.Units;
    }
  }
  for (const FlowPacket &packet : supplies.Packets) {
    if (packet.To == shop) {
      position += packet.Units;
    }
  }
  const FlowAddressed orders = addressedTo<SupplyOrders>(world, shop);
  for (const FlowPacket &packet : orders.Packets) {
    if (isDepot(packet.To)) {
      position += packet.Units;
    }
  }
  for (const FlowStock &stock : orders.Stocks) {
    if (isDepot(stock.Endpoint)) {
      position += stock.Units;
    }
  }
  return position;
}
```

Step 2: Build.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 11: Step shops

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Replace the empty stepShops:

```cpp
// Each shop, in ascending key order, takes back the orders returned to it, and when it has a
// nearest depot and its inventory position is at the reorder point or below, orders up to
// ORDER_UP_TO from that depot.
void stepShops(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    const int64_t returned = unitsHeld<SupplyOrders>(world, shop, shop);
    if (returned > 0) {
      consumeUnits<SupplyOrders>(world, shop, shop, returned, UNFILLED_CAUSE);
    }
    const std::optional<DepotRoute> route = nearestDepot(world, shop);
    const int64_t position = inventoryPosition(world, shop);
    if (!route || position > REORDER_POINT) {
      continue;
    }
    const int64_t wanted = ORDER_UP_TO - position;
    createUnits<SupplyOrders>(world, shop, shop, wanted);
    sendUnits<SupplyOrders>(world, shop, route->Depot, shop, wanted, ORDER_DELAY);
  }
}
```

stepShops and stepDepots use nearestDepot and inventoryPosition, which are declared in operations.h, so the anonymous namespace may stay above their definitions.

Step 2: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests "[operations]"`
Expected: no warnings. Every test passes except criterion 6's.

### Task 12: Step depots

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Replace the empty stepDepots:

```cpp
// Each depot, in ascending key order, takes back the supplies returned to it, and then, for each
// shop it holds orders for, cancels them when the shop is gone, sends them back when it cannot
// reach the shop, and otherwise ships as many supplies over the backstage route.
void stepDepots(World &world) {
  const std::vector<EntityKey> shops = boxKeys(world, BoxKind::Shop);
  for (const EntityKey depot : boxKeys(world, BoxKind::Depot)) {
    for (const FlowHolding &held : stockOf<Supplies>(world, depot)) {
      consumeUnits<Supplies>(world, depot, held.Handle, held.Units, RETURNED_CAUSE);
    }
    for (const FlowHolding &held : stockOf<SupplyOrders>(world, depot)) {
      const EntityKey shop = held.Handle;
      if (!std::ranges::binary_search(shops, shop)) {
        consumeUnits<SupplyOrders>(world, depot, shop, held.Units, CANCELLED_CAUSE);
        continue;
      }
      const std::optional<double> length = supplyRouteLength(world, depot, shop);
      if (!length) {
        sendUnits<SupplyOrders>(world, depot, shop, shop, held.Units, ORDER_DELAY);
        continue;
      }
      consumeUnits<SupplyOrders>(world, depot, shop, held.Units, FULFILLED_CAUSE);
      createUnits<Supplies>(world, depot, shop, held.Units);
      sendUnits<Supplies>(world, depot, shop, shop, held.Units, shipmentDelay(*length));
    }
  }
}
```

Step 2: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests "[operations]"`
Expected: no warnings, and every operations test passes.

### Task 13: Confirm the acceptance criteria

Step 1: Format, build, and test both builds, and run the cross-build check.

Run:
```
git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i
cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"
ctest --preset linux-debug
cmake.exe --build --preset windows-debug
ctest.exe --preset windows-debug
scripts/cross-build-check.sh
```
Expected: no warnings, all tests pass on both builds, and the cross-build check passes.

### Task 14: Commit

Review and commit per implementing-features, via commit-hygiene, with the subject "Operations: Supply shops from depots over backstage routes".
