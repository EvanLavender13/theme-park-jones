# Feature: Supply Chain

## Summary

supply-chain creates the operations module, src/sim/operations, and makes shops and depots run over the park's real routes. makeParkSchema registers the module's four flow kinds and its two systems. Every shop box is a shop and every depot box a depot. A shop is supplied while backstage route distance joins it to some depot, and starved otherwise. A supplied shop orders by an (s, S) policy from its nearest depot. A depot ships supplies for the orders it holds, with a delay taken from the route distance to the shop. It sends back the orders of a shop it cannot reach, and it cancels those of a shop that is gone. A shop's orders and supplies carry the shop's key as their handle. A new ledger query lists the units addressed to an entity, so the shop's inventory position is exactly what the medium holds addressed to it, and neither side reads the other. Service, offers, and records come in later features.

## Acceptance criteria

1. Resolving any world made with makeParkSchema gives it a ledger for each of supply-orders, supplies, guest-visits, and meals. The operations module registers no component type of its own.
2. addressedTo<K>(world, handle) gives exactly the kind's ledger packets whose handle is the key, in the ledger's order, and exactly its stocks whose handle is the key, in the ledger's order. It gives nothing for a world holding no ledger for the kind.
3. supplyRouteLength(world, at, source) is the least Distance among the entries of the source that sampling backstage-route-distance gives at the nodePlace of each backstage node anchored to at, and none when there is no such entry. nearestDepot(world, shop) is the depot box with the least supplyRouteLength at the shop, ties to the lower key, with that length, and none when no depot box has one. A shop with no nearest depot is starved.
4. shipmentDelay(distance) is ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS)), computed in that order, with SUPPLY_SPEED 2 m/s. It gives 1 for a result below 1 or a NaN, and the largest uint32_t for a result above it.
5. Take any world W between cycles, and the cycle that follows. Each shop consumes every order unit it holds under its own handle in W, with the cause unfilled. When nearestDepot(W, shop) exists and inventoryPosition(W, shop) is REORDER_POINT (8) or below, the shop sends exactly one new packet of supply-orders. It holds ORDER_UP_TO (24) minus that position units, goes to the nearest depot with the shop as handle, and has the delay ORDER_DELAY (30). Otherwise the shop sends none. inventoryPosition counts four things addressed to the shop: the supplies it holds, supplies packets whose destination is the shop, supply-orders packets whose destination is a depot box, and supply-orders stocks held by depot boxes. So orders held by or heading to a deleted depot never count, and the position never exceeds ORDER_UP_TO.
6. Take the same W and cycle. Each depot consumes every supplies unit it holds in W, with the cause returned. For each handle under which it holds order units in W: when the handle names no shop box, it consumes them as cancelled. When supplyRouteLength(W, depot, handle) is none, it sends them to the handle with that handle and the delay ORDER_DELAY. Otherwise it consumes them as fulfilled, creates as many supplies, and sends them to the handle with that handle and the delay shipmentDelay of that length.
7. In every tick of randomized sequences of park edits (tests/sim/support/route_edits.h), interleaved with stepping:
    - each of the four kinds satisfies the ledger's identity;
    - consumed order units carry only the causes fulfilled, unfilled, cancelled, undeliverable, and discarded, and consumed supplies only returned, undeliverable, and discarded;
    - supplies created equal orders consumed as fulfilled;
    - no shop's inventoryPosition exceeds ORDER_UP_TO.
8. Every world such a sequence reaches is legitimate: no step or edit throws. The world loaded from its save and resolved equals it, and stepping each of them further keeps them equal.

## Medium

- Samples backstage-route-distance, which navigable-networks publishes, at a shop's and a depot's anchored backstage nodes, through supplyRouteLength.
- Supplies and draws supply-orders: shops create and send them to depots, and depots consume them as fulfilled or cancelled, or send them back.
- Supplies and draws supplies: depots create and ship them to shops. This feature consumes them only when they come back to a depot. meal-service converts them into meals.
- Registers guest-visits and meals, declared in operations.h for believable-guests. Nothing moves them in this feature.
- Reads box intent through parkBoxes (decision 0025): which boxes are shops and depots, and whether an order's handle is still a shop.
- Adds addressedTo to the medium's ledger queries.

A shop reads only the units addressed to it, route distance, and intent. A depot reads only its own stock, route distance, and intent.

## Principle checks

- Principle 3: criterion 7's identities, causes, and the law that supplies created equal orders fulfilled, in every tick of randomized edit runs.
- Principle 6: criteria 5 and 6 compute each shop's and depot's decision from W alone, through addressedTo, the depot's own stock, route distance, and intent. Review checks that operations.cpp reads the ledger only through addressedTo of a shop's key and the acting shop's or depot's own stock, and reads no state of another shop or depot.
- Principle 4: criterion 6, where a shipment's delay is shipmentDelay of the route distance along the backstage network.
- Principles 2 and 5: criteria 3 and 8. A shop with no route is placed, starved, and sends no orders, and randomized edits never throw.
- Principle 1: criteria 1 and 8. The module keeps nothing outside the ledgers, and a saved world loads back equal and steps on equal.
- Principle 10: criterion 8's continued stepping. The cross-build check runs the park-edits scenario and tests/parks/routes.park, whose shop and depot are joined by a backstage path, so orders and shipments are compared across builds.

## Spec changes

- src/sim/operations/SPEC.md (new): the module, as PLAN.md's Task 1 gives it: registration, shops and depots, supply routes, shipmentDelay, inventoryPosition, the shop and depot steps, and what the ledger settles.
- src/sim/medium/SPEC.md: a sentence on addressedTo in the Flows section's paragraph of queries.
- src/sim/SPEC.md: makeParkSchema registers the operations module after the routes module.
- src/scenarios/SPEC.md: park-edits' shops and depots now operate, so their orders and shipments are compared across builds.

Public interface, src/sim/operations/operations.h:

```cpp
struct SupplyOrders { static constexpr std::string_view Name = "supply-orders"; };
struct Supplies { static constexpr std::string_view Name = "supplies"; };
struct GuestVisits { static constexpr std::string_view Name = "guest-visits"; };
struct Meals { static constexpr std::string_view Name = "meals"; };

inline constexpr int64_t REORDER_POINT = 8;
inline constexpr int64_t ORDER_UP_TO = 24;
inline constexpr uint32_t ORDER_DELAY = 30;
inline constexpr double SUPPLY_SPEED = 2.0;

inline constexpr std::string_view FULFILLED_CAUSE = "fulfilled";
inline constexpr std::string_view UNFILLED_CAUSE = "unfilled";
inline constexpr std::string_view CANCELLED_CAUSE = "cancelled";
inline constexpr std::string_view RETURNED_CAUSE = "returned";

struct DepotRoute {
  EntityKey Depot = NULL_KEY;
  double Distance = 0.0;
  bool operator==(const DepotRoute &) const = default;
};

std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source);
std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop);
uint32_t shipmentDelay(double distance);
int64_t inventoryPosition(const World &world, EntityKey shop);
void addOperations(WorldSchema &schema);
```

Added to src/sim/medium/flow.h:

```cpp
struct FlowAddressed {
  std::vector<FlowPacket> Packets;
  std::vector<FlowStock> Stocks;
};

template <FlowDefinition K> FlowAddressed addressedTo(const World &world, EntityKey handle);
```

## Files affected

- Create: src/sim/operations/SPEC.md, src/sim/operations/operations.h, src/sim/operations/operations.cpp
- Modify: src/sim/medium/flow.h, src/sim/medium/SPEC.md, src/sim/park_schema.cpp, src/sim/CMakeLists.txt, src/sim/SPEC.md, src/scenarios/SPEC.md
- Tests (test pass): tests/sim/operations/, tests/sim/medium/flow_test.cpp, tests/sim/CMakeLists.txt

## Dependencies

- navigable-networks' route-distance: backstage-route-distance and parkNetwork. Met.
- shared-medium's flow-ledger: handles, returns, and settlement. Met.
- tests/sim/support/route_edits.h and tests/sim/support/park_worlds.h, for randomized edits and hand-stated parks.

## Out of scope

- Serving guest visits, meals, and converting supplies (meal-service).
- The food offer (food-offer).
- The inspection record and the starved mark (shop-records).
- The deepening candidates in MILESTONE.md, such as an order delay from route distance and shipments combining orders.

## Open questions

None.
