# Feature: Meal Service

## Summary

meal-service makes shops serve guests. A guest sends a shop a visit, one guest-visits unit addressed to itself. The shop queues visits in the order they arrive and takes the guest at the front when it is free and holds a supply unit. It converts the unit into a meal and sends the meal and the visit back to the guest together, with the delay SERVICE_INTERVAL, the time service takes, and it takes no other guest until then. A starved shop keeps serving from its stock and the shipments on their way to it, and returns at once, without a meal, the visits those cannot cover. A shop consumes as abandoned any visit or meal it holds for a guest that is gone, and the ledger returns a deleted shop's queue to its guests. The shop's queue lives in a private state component on its box's entity. The park's registrations move into addPark, so a new scenario, food-shop, can add synthetic guests to the park's schema and put service in the cross-build check. The offer and the inspection record come in later features.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema between cycles, t its tick, and "the cycle" the stepWorld that follows. A guest is any entity whose key is the handle of guest-visits units. Tests state W by stepping and by writing the ledgers between cycles, and a test's synthetic guests are entities it creates and destroys between cycles.

1. addOperations registers the flow kinds supply-orders, supplies, guest-visits, and meals, then the state component type shop-service, then its systems. makeParkSchema gives a schema with exactly the component types, systems, swaps, resolvers, finishers, and commands that addPark registers into an empty schema. A shop box that has never held a visit behaves as a shop with an empty queue that is free to serve.
2. A shop's queue holds one entry for each visit unit it holds, in the order the units arrived in its stock, and units that arrived at the same swap in ascending guest key order. Criteria 4 and 5 act on the queue in that order, so a guest whose visit arrived earlier is served before, and kept by a starved shop before, one whose visit arrived later.
3. In the cycle, each shop consumes, with the cause abandoned, every meals unit it holds in W and every guest-visits unit it holds in W under a handle that names no live entity. It never sends such a unit, and such a guest's queued visits are never served or returned.
4. In the cycle, a shop serves when its queue is not empty, it holds at least one supplies unit under its own handle, and it last served at t - SERVICE_INTERVAL (90) or earlier, or never. Serving takes the guest at the front of the queue: the shop consumes exactly one supplies unit with the cause served, creates one meal under the guest's handle, and sends the meal and one of the guest's visit units to the guest, each as a packet of one unit with the guest as handle and the delay SERVICE_INTERVAL. It serves no other guest in the cycle, and a supplied shop sends guest-visits and meals only for the guest it serves.
5. In the cycle, a starved shop, one with no nearestDepot in W, keeps queued only its first c visits, where c is the supplies it holds under its own handle plus the units of supplies packets addressed to it whose destination is the shop, all in W. It sends every other queued visit back to its guest with the guest as handle and the delay RETURN_DELAY (1), and sends no meal with it. A supplied shop returns no visit.
6. When a shop box is deleted, the visits the shop held reach their guests' stocks with no meal, and its supplies are consumed as discarded. Visits and meals it sent to a guest that is gone come back to it and are consumed as abandoned, or as undeliverable when the shop is gone too.
7. In every tick of randomized sequences of park edits (tests/sim/support/route_edits.h), interleaved with synthetic guests that send visits to shops and some of which are destroyed:
    - each of the four kinds satisfies the ledger's identity;
    - consumed supplies carry only the causes returned, served, undeliverable, and discarded, and consumed guest-visits and meals only abandoned, undeliverable, and discarded;
    - meals created equal supplies consumed as served;
    - every meals packet a shop sends has a guest-visits packet sent by the same shop, to the same guest, with the same handle and arrival.
   Every world such a sequence reaches is legitimate: no step or edit throws. The world loaded from its save and resolved equals it, and stepping each further keeps them equal. A candidate made from it with an edit equals the world that queues the same edit for its next cycle, once that cycle's resolution is done.
8. registeredScenarios includes food-shop, after park-edits. Its world holds a shop joined to a depot by a backstage path and a shop with no backstage path. Over the runner's default 3000 ticks, it creates meals, returns visits without meals, and consumes visits as abandoned, and each kind satisfies the ledger's identity after every cycle.

## Medium

- Draws guest-visits: guests send visits to shops. Supplies it back: a shop returns every visit it does not consume as abandoned to its guest, served or not.
- Supplies meals: a shop creates one for each guest served and sends it with the visit. The guest believable-guests adds later receives it. A visit that arrives with a meal was served.
- Draws supplies: a shop consumes one unit per guest served, with the cause served.
- Samples backstage-route-distance, through nearestDepot, for whether a shop is starved.
- Reads addressedTo of the shop's key for supplies on their way to it, the shop's own stocks, its own shop-service component, whether a guest's key names a live entity, and box intent through parkBoxes (decision 0025).

A shop never reads a guest's state or another shop's or depot's state.

## Principle checks

- Principle 3: criterion 7's identities, causes, and laws, in every tick of randomized runs with synthetic guests, and criterion 8's identities in the scenario.
- Principle 6: criteria 3 to 5 compute each shop's service from W. Review checks that the service step reads only the acting shop's stocks and component, addressedTo of its key, nearestDepot, liveness of its guests' keys, and intent, and that ShopService is declared under src/sim/operations/internal.
- Principle 7: criterion 4. Service takes SERVICE_INTERVAL, costs a supply, and a shop serves no faster than its rate.
- Principles 2 and 5: criteria 3, 5, 6, and 7. A starved shop is placed and turns guests away, gone guests and deleted shops leave nothing stuck, and randomized runs never throw.
- Principle 1: criterion 7. The queue is state, saved, and a saved world loads back equal and steps on equal.
- Principle 10: criterion 8. The cross-build check runs food-shop, so service is compared across builds.

## Spec changes

- src/sim/operations/SPEC.md: the Registration section registers shop-service and drops "The module registers no component type"; a new Service section describes visits, meals, the queue, the step, and what the ledger settles for a deleted shop; the Orders and supplies section's last paragraph gains the served cause, the meals law, and the abandoned cause. PLAN.md's Task 1 gives the text.
- src/sim/SPEC.md: addPark registers the park's types, makeParkSchema returns a schema holding them, and the food-shop scenario adds its guests after them.
- src/scenarios/SPEC.md: the food-shop scenario.

Public interface added to src/sim/operations/operations.h:

```cpp
// Ticks service takes: a served guest's meal and visit arrive this long after the shop takes it,
// and the shop takes no other guest until then.
inline constexpr uint32_t SERVICE_INTERVAL = 90;
// Ticks an unserved visit takes to go back to its guest.
inline constexpr uint32_t RETURN_DELAY = 1;

inline constexpr std::string_view SERVED_CAUSE = "served";
inline constexpr std::string_view ABANDONED_CAUSE = "abandoned";
```

Added to src/sim/park_schema.h:

```cpp
// Registers every capability's component types, systems, swap functions, resolvers, and commands
// for the park, in a written order.
void addPark(WorldSchema &schema);
```

Added to src/scenarios/synthetic.h:

```cpp
Scenario foodShopScenario();
```

## Files affected

- Create: src/sim/operations/internal/shop_service.h, src/scenarios/food_shop.cpp
- Modify: src/sim/operations/operations.h, src/sim/operations/operations.cpp, src/sim/operations/SPEC.md, src/sim/park_schema.h, src/sim/park_schema.cpp, src/sim/SPEC.md, src/scenarios/synthetic.h, src/scenarios/scenarios.cpp, src/scenarios/CMakeLists.txt, src/scenarios/SPEC.md, plans/plausible-operations/supplied-food-shop/MILESTONE.md (criteria 5 and 6 name the service delay and the abandoned rule)
- Tests (test pass): tests/sim/operations/, tests/sim/CMakeLists.txt, tests/scenarios/, and tests/scenarios/CMakeLists.txt. The supply-chain test that addOperations registers no component type of its own now contradicts criterion 1 and is updated by the test pass.

## Dependencies

- supply-chain: the operations module, nearestDepot, and supplies addressed to a shop. Met.
- shared-medium's flow ledger: returns to sender, settlement of a gone endpoint's stock, and the undeliverable and discarded causes. Met.
- tests/sim/support/route_edits.h and tests/sim/support/park_worlds.h.

## Out of scope

- The food offer and its expected wait (food-offer).
- The inspection record, the limiting factor, and the starved mark (shop-records).
- A queue limit, and guests who give up waiting at the shop; both sit in MILESTONE.md's deepening candidates or belong to believable-guests.
- Real guests, hunger, and what a meal does to a guest (believable-guests).

## Open questions

None.
