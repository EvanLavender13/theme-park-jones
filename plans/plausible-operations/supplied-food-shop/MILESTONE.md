# Milestone: Supplied Food Shop

Slice: boxes-and-tubes

## Summary

supplied-food-shop makes the park's shops operate. It adds the module src/sim/operations. The module turns shop and depot boxes into operating entities. A shop orders supplies by an (s, S) policy from the nearest depot it can reach on the backstage network, and the depot ships them with a delay that comes from route distance. The shop serves queued guest visits one per service interval, sending a meal with each served visit, and returns the visits it cannot serve. It publishes a food offer with an exact expected wait and an inspection record with its limiting factor, and its box shows when it is starved. It comes fifth in the slice. The medium, park intent, and route distance are in place, and guests cannot choose without offers. So it lands before believable-guests and is tested against synthetic guest visits. Once it lands, a layout succeeds or fails for operational reasons: cut the backstage path and the shop starves.

## Acceptance criteria

1. makeParkSchema registers the operations module after the routes module. Every shop box is a shop and every depot box is a depot, in the committed world and in a candidate, with no other intent. A shop's state lives on its box's entity, and a box that has never stepped behaves as an empty shop or depot. The flow kinds supply-orders, supplies, guest-visits, and meals are registered, and guest-visits and meals are declared in a public header for believable-guests.
2. A shop is supplied while the backstage route distance field has an entry, at its backstage anchor, for some depot. The shop learns which sources are depots from box intent (decision 0025). Otherwise it is starved, including when it has no backstage connector.
3. A supplied shop orders by an (s, S) policy. When its inventory position, meaning its stock plus the supplies in transit to it plus its outstanding orders, falls to REORDER_POINT or below, it sends enough order units to bring the position up to ORDER_UP_TO. It sends them to the nearest depot by route distance, with ties going to the lower key, and they arrive after ORDER_DELAY. A depot ships one unit of supplies for each order unit it holds, creating them as it ships. The shipment's delay is the route distance to the shop, sampled at the depot's backstage anchor, converted at SUPPLY_SPEED, and it is at least one tick. A depot that cannot reach the shop sends the order units back to the shop as unfilled. A depot consumes the orders it holds for a shop that is no longer a shop box, as cancelled, so orders never bounce between a depot and a deleted shop. When a depot is deleted, its held orders go back to their shops, and orders still in transit to it return to their senders. A shop's outstanding orders never count an order held by or heading to a deleted depot, and its position never exceeds ORDER_UP_TO.
4. Orders, supplies, and meals each conserve with the ledger's identity. Beyond that, every order unit is in transit, held by a depot, fulfilled, returned unfilled, cancelled, or consumed by the ledger as undeliverable or discarded, and every supply unit is in transit, held in a shop's stock, converted to a meal, taken back by the depot a shipment returned to, or consumed by the ledger as undeliverable or discarded. Both hold in every tick of randomized runs that interleave park edits (tests/sim/support/route_edits.h) with synthetic guest visits.
5. A shop serves its queued visits in arrival order, with visits that arrive in the same tick taken in guest key order, and serves at most one guest per SERVICE_INTERVAL. Each guest served costs one supply. The shop creates a meal and sends it to the guest with the visit, in the same tick and with the delay SERVICE_INTERVAL, the time service takes, so a visit that arrives with a meal was served. So meals served are limited by the scarcest of demand, supply, and the service rate.
6. A starved shop keeps serving from its stock and from supplies in transit to it. It returns the queued visits those cannot cover as unserved, at once, and returns later arrivals the same way. A visit or meal a shop holds for a guest that is gone, including one that came back because its guest left, is consumed as abandoned, never sent again. When a shop is deleted, the ledger sends its queued visits back to their guests without meals, and records its remaining supplies as discarded.
7. A shop publishes a food offer at its guest anchor. It holds MEAL_RELIEF, the expected wait in ticks, and whether the shop is supplied. The resolved offer is computed from intent and route distance alone, as for an empty shop, so a candidate or newly placed shop has a complete offer at once. The stepped offer is republished from the shop's state every tick, and a starved shop's offer says no meals from the tick its route is cut. The wait is the one the capability defines: the later of the next arrival's turn, SERVICE_INTERVAL after the guest ahead of it is taken, and the time a supply unit for it becomes available. It is computed exactly and tested exactly, including for an empty new shop and for a shop with nothing on order.
8. A shop publishes an inspection record (decision 0025): its stock, queue length, supplies on order, whether it is starved, and its limiting factor, one of demand, supply, service rate, or no supply route. A starved shop's box is marked in the scene. A ghost shows the starved marks of its candidate world, so hovering a backstage path with the delete tool shows the shop starving before the click. tests/parks/supply.park holds one supplied shop and one starved shop, and a --capture of it shows the mark on the starved one only.
9. Every world that randomized edit sequences with synthetic guests reach is legitimate (principle 2). A saved world loads back and resolves equal to the world saved. A candidate made with an edit equals the world that commits it. tpj_scenarios gains a food-shop scenario with synthetic guests, and the cross-build check passes with it and with supply.park.
10. src/sim/operations/SPEC.md describes the module as built, and src/sim/SPEC.md, src/sim/medium/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and src/scenarios/SPEC.md describe their changes. linux-debug builds without warnings and its tests pass, and windows-debug builds.

## Medium

Cross-capability, as the slice's medium map assigns:

- Route distance (navigable-networks): supply-chain samples backstage-route-distance at a shop's backstage anchor, to find the nearest depot and judge whether the shop is supplied. It also samples it at a depot's backstage anchor, with the ordering shop as the source, to judge reach and set each shipment's delay. food-offer samples it for the resolved offer's supply delay.
- Food offer (food-offer): an entry field, food-offer, at each shop's guest anchor, resolved and stepped. Sampled later by believable-guests and legible-simulation. Tests sample it here.
- Guest visits (meal-service): a flow, guest-visits, that synthetic guests send in this milestone and believable-guests sends later. Each visit is one unit whose handle is its guest. A shop returns every visit to its guest, and a visit that arrives with a meal was served.
- Meals (meal-service): a flow, meals, from a shop to the guest it served, sent with the visit.
- Inspection records (shop-records, decision 0025): read by tests, by the box rendering for the starved mark, and later by legible-simulation's shop inspector.
- Park intent and networks (decision 0025): shops and depots from parkBoxes, and their anchored nodes through parkNetwork and the Network type's queries.

Internal to this capability:

- Supply orders (supply-chain): a flow, supply-orders, from a shop to a depot, with the shop as each unit's handle, so the ledger returns a deleted depot's held orders to their shops.
- Supplies (supply-chain): a flow, supplies, from a depot to a shop, with a delay from route distance.
- Units addressed to an entity (supply-chain): a query supply-chain adds to the ledger in src/sim/medium. It lists a kind's packets and stocks whose handle is one entity's key, with their arrival ticks and units, so a shop counts its orders and the shipments coming to it without reading a depot. It is a new query on the existing ledger, not a new field or flow.

Shops and depots never read each other's state. A shop decides from its own state, the units addressed to it, the fields it samples, and intent. A depot decides from the orders it holds and the route distance it samples.

## Dependencies

- deterministic-simulation's world-as-value: systems, resolvers, candidates, saves, and the cross-build check. Met.
- shared-medium's first-field-and-flow: entry fields with both layers, and the flow ledger with handles and returns. Met.
- navigable-networks' paths-become-routes: networks, box anchors, and backstage-route-distance. Met.
- effortless-building's sketch-a-park: shop and depot box intent, box rendering, ghosts, and --capture. Met.

## Core feature

supply-chain is the core. It creates the module, and it makes shops and depots operate over real routes. Shops order by (s, S), depots ship with a route-distance delay, and a shop is starved without a route. So the milestone's defining failure, cutting a route and a shop running dry, is testable from the first feature, with both conservation laws.

## Features

1. `supply-chain`: the module src/sim/operations registered in makeParkSchema, the four flow kinds, shops and depots from box intent, supplied and starved from backstage route distance, (s, S) ordering from the nearest depot, depots shipping with route-distance delays, unfilled and deleted-depot returns, the ledger's query for the units addressed to an entity, and conservation tests under randomized park edits. Depends on: none.
2. `meal-service`: the queue of guest visits in arrival order, one guest per SERVICE_INTERVAL, meals sent with served visits, unserved returns, a starved shop's serving from stock and inbound supplies, deleted shops, and the food-shop scenario with synthetic guests for the cross-build check. Depends on: feature 1.
3. `food-offer`: the food-offer field, resolved from intent and route distance and stepped from state, with the exact expected wait and the no-meals offer of a starved shop. Depends on: feature 2.
4. `shop-records`: the shop's inspection record with its limiting factor, the starved mark on the box in the scene and in the ghost, and tests/parks/supply.park with its capture. Depends on: feature 2.

## Tuning values

The capability leaves these to this milestone. Playing adjusts them. RESEARCH.md shows how they relate.

- SERVICE_INTERVAL: 90 ticks (3 s) per guest served.
- REORDER_POINT: 8. ORDER_UP_TO: 24.
- ORDER_DELAY: 30 ticks (1 s) for an order to reach its depot.
- SUPPLY_SPEED: 2 m/s along the backstage route.
- MEAL_RELIEF: 0.5, on a hunger scale from 0 to 1, which hungry-guests adopts or re-tunes.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A depot's inspection record: orders held and units shipped, with its own mark when no shop can reach it.
- An order's delay from route distance, like a shipment's, instead of the fixed ORDER_DELAY.
- A queue limit, with arrivals beyond it returned unserved, as a visible sign of an overloaded shop.
- Shipments that combine the order units a depot holds for one shop into one packet per tick. Gated on: packet counts mattering for cost.
- A Debug panel list of each shop's inspection record, before legible-simulation's inspector lands.
- An offer's wait that counts orders on their way to or held by a depot as supply units, each arriving after its shipment's delay, instead of treating every unit beyond stock and shipments as coming from an order placed now.

## Open questions

- Whether MEAL_RELIEF's 0 to 1 hunger scale holds. Resolved when hungry-guests defines hunger.

## Research notes

- Parkitect marks an out-of-stock shop with an icon above it. Here the mark shows starved (no route to a depot), which depends on intent alone, so a mesh rebuilt when intent changes, and a ghost's candidate, always show it correctly.
- The reorder point covers demand over the lead time at the full service rate, which gives s = 8 and S = 24 for a 40 m route.
- A served visit is told from an unserved one by the meal sent with it in the same tick. The ledger's return of a deleted shop's queue therefore reads as unserved, and no new flow is needed.

Depth is in RESEARCH.md.
