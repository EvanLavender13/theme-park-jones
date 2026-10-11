# Slice: Supply Lines

Status: planned

## Summary

When this slice lands, goods come into the park from outside, and the player builds the network that carries them to the shops. The player places service entrances on the park's edge. Food and drink are delivered there from outside after a lead time, at a limited rate. Depots stop being unlimited sources and become storage: each holds as much as its floor area allows, keeps each good up to a target the player sets, and restocks from whichever source can deliver soonest, counting travel, whether a service entrance or another depot. A central warehouse by the service entrance can feed small depots near the shops. Backstage paths carry only so much, and wider paths carry more, so an overloaded trunk slows every shipment on it. The player sees carts moving where goods travel, an overlay of load on the backstage paths, and inspectors that name each depot's and entrance's limiting factor. This is the right next goal because the depot is the last thing in the park that creates something from nothing. Making goods come from outside gives the player a logistics network to design, in the manner of an automation game: where goods enter, where they are stored, and which paths carry them. Rising demand in the manner of Anno is a later slice that can build on this one.

## End-to-end scenario

The slice starts from a park built as day-at-the-park leaves it, with food and drink as separate goods.

A new park has a guest entrance and no service entrance. The player draws a guest path, places a food shop and a drink shop beside it, places a depot behind them, and joins them with a backstage path. The shops' boxes are marked starved, as today, because nothing in the park can supply them. The depot is empty, and it has no source to order from.

The player picks the service entrance tool. The ghost slides along the park's edge and refuses any place away from it. The player sets the service entrance down on the edge behind the shops and draws a backstage path from it to the depot. The starved marks go away. The depot orders food and drink from the service entrance, up to its targets. Each delivery reaches the service entrance after the outside lead time, and the entrance unloads it at its dock rate. Carts then carry the goods along the backstage path into the depot, and from the depot to the shops. The player clicks the depot. Its inspector shows the stock of each good against its target and its storage, the orders it holds and has out, and its limiting factor.

As the park grows, the player lays a second street of shops far from the service entrance. They place a small depot near the new shops and a large warehouse depot beside the service entrance, and join them with one long backstage trunk. The small depot orders from the warehouse, because the warehouse can deliver sooner than the service entrance can. The warehouse orders from the service entrance. Goods now move in one direction, from the entrance to the warehouse, along the trunk, to the small depot, and out to the shops.

At the lunch rush the shops on the far street order more often. The carts on the trunk bunch up and slow down. The player switches the overlay to supply traffic, and the trunk shows heavy load against its capacity. Hovering the trunk lists each shipper's share of the load. The small depot's inspector names its limiting factor as supply, since its deliveries are arriving late. The player widens the trunk with the segment width handle. After that, carts on it move at full speed, and the small depot refills before its stock runs out.

Later the player lowers the warehouse's drink target, because drink sells less than food. The warehouse stops ordering drink until its stock falls below the new target. One shop at the park's far corner has no depot near it and draws straight from a second service entrance. It stays supplied, but every order waits the full outside lead time, so its inspector shows it limited by supply more often than its neighbors.

## Acceptance criteria

Three park files are checked in under tests/parks:

- lines.park holds the scenario's grown park at 9:00 with no guests: a guest entrance, guest paths with shops on two streets, a service entrance, a warehouse depot beside it, and empty depots. A backstage trunk runs from the warehouse to the far street. The far street's shops join the trunk by short branches, and a small depot stands on the trunk near them. Deleting the small depot leaves those shops joined to the warehouse.
- lines-noon.park is lines.park run to 12:00, with guests in the park and goods in transit.
- Every other checked-in park that has a depot gains a service entrance joined to its depot by a backstage path. That covers tests/parks (day.park, noon.park, no-toilet.park, evening.park, fed.park, warm.park, cut.park, supply.park, main-street.park, warm-main-street.park, routes.park, sketch.park, and degenerate-intent.park) and tests/parks/stress (full.park and winding-path.park). The park builders that make these parks add one the same way. Every existing integration test, including food_loop_test on warm.park, must keep passing, and the stress parks keep measuring supply.

The member that adds lines.park and lines-noon.park makes tpj_scenarios --slice-parks write them alongside the earlier slices' parks. The park world checks and the cross-build check then cover the slice's new state without a new test. That state includes service entrances, depot targets, depot stock, orders at each source, and supply traffic. The world checks' chosen edits gain three edits of lines-noon.park: placing a second service entrance, changing a depot target, and deleting the service entrance. Each one's preview is then checked against its commit (principle 8).

1. Goods come only from outside: if lines.park's service entrance is deleted before opening, its shops serve no meals and no drinks all day, while lines.park unchanged serves some. (integration test)
2. Narrowing the trunk slows supply: in lines-noon.park with the trunk set to MIN_PATH_WIDTH, supplies sent to the small depot over the next hour take longer to arrive, on average, than with the trunk set to MAX_PATH_WIDTH. (integration test)
3. A depot near the shops cuts their wait: in lines-noon.park with the small depot deleted, supplies sent to the far street's shops over the next hour take longer to arrive, on average, than in lines-noon.park unchanged. (integration test)
4. A capture of lines-noon.park with --overlay supply-traffic shows the trunk shaded heavier than the branches to single shops, and no shading on guest paths. (scripted capture: legible-simulation's member)
5. A capture of lines-noon.park shows carts on the backstage paths between the service entrance, the depots, and the shops, and none on guest paths. (scripted capture: legible-simulation's member)
6. In the running app, the service entrance tool places entrances only with their back on the park's edge, facing in, and its ghost shows any other place as invalid. A placed entrance joins a backstage path drawn to it. (manual)
7. In the running app, setting a depot's target for a good makes the depot fill to that target and no further. A depot never holds more than its storage. Lowering a target stops the depot ordering that good until its stock falls below the new target. (manual)
8. In the running app, the depot and service entrance inspectors show stock per good, targets, storage, orders held and out, and a limiting factor that matches what the player sees. A shop that draws straight from a service entrance stays supplied but is limited by supply more often than one beside a stocked depot. (manual)
9. In the running app, hovering the supply traffic overlay lists each shipper's share of a path's load, and the shares sum to the value shown, to the precision shown. (manual)
10. In the running app, each cart follows its shipment's real route and reaches the destination at the shipment's arrival tick. It spends longer on the loaded paths of its route than on the free ones, so carts bunch up on an overloaded path, and widening that path speeds up the carts sent after it. (manual)

## Medium

Abbreviations: SM is shared-medium, NN is navigable-networks, PO is plausible-operations, LS is legible-simulation, and EB is effortless-building.

- Networks: the guest and backstage graphs, unchanged in kind. NN produces them through SM's network type. A service entrance's front door serves the backstage network. Every capability consumes the networks as before.
- Route distance: an entry field over the networks, produced by NN and unchanged in kind. Service entrances become backstage sources once their doors anchor backstage nodes. PO consumes it to judge reach, to find sources, and to walk a shipment's route. LS consumes it to walk a cart's route from its sender to its destination.
- Path capacity: how much a backstage path carries per minute, as a function of its width. NN states it as a public function of the carrier width that SM's widthAt already gives, so it adds no state and no interaction between entities. Capacity never falls as width grows, and a path at MAX_PATH_WIDTH carries more than one at MIN_PATH_WIDTH, whether service-doors maps width to capacity smoothly or in steps. PO consumes it for congestion delay, and LS for the overlay and for placing carts.
- Supply offer: an entry field on backstage network nodes, produced by PO at each service entrance's and depot's backstage anchor. For each good, it holds how soon the source could deliver an order placed now, not counting travel, and the source's rank, its backstage route distance to the nearest service entrance, which is zero for a service entrance. Shops and depots in PO sample it with route distance to choose a source. A depot takes only sources of strictly lower rank than its own, so goods move only away from the service entrances, and no two depots order from each other. LS shows it in the inspectors.
- Supply traffic: a kept scalar field on backstage network edges (decision 0031), produced by PO. When a source sends a shipment, it adds the shipment's load to each edge along the shipment's route, attributed to itself. Load decays on read as time passes. PO samples it along a route to set a new shipment's delay: the travel time at full speed, stretched by the load on each edge against that edge's capacity. The stretch rises with any load above zero, with no flat stretch below capacity, and rises linearly above capacity. LS consumes it for the overlay and hover attribution, and to place carts. A shipment's delay is set once, when it is sent, because the ledger never reads a network.
- Supplies and supply orders: conserved flows, one pair per good, through SM's ledger, produced and consumed by PO. A service entrance creates supplies when an outside delivery lands, and that is the only place they are created. Orders go from shops and depots to their chosen source. Supplies go from service entrances to depots and shops, and from depots to depots and shops. The ledger's existing return-to-sender and discard rules cover deleted sources.
- Shipments in transit: LS reads the supplies packets in transit, each with its sender, destination, arrival tick, and delay, to place carts along their routes. A cart's total travel time is its shipment's delay. LS splits that time over the edges of the route in proportion to each edge's stretched travel time under the current supply traffic, so the cart arrives on time and lingers where the load is heavy. This is a new query on the existing ledger, added in SM's module by LS's member, as supply-chain added addressedTo. It is display only and never feeds back into the simulation (decision 0025).

Some dependencies are not fields or flows. Decision 0025 rules on them.

- Park intent: service entrance boxes, and each depot's target for each good. A service entrance is a gate in the park's boundary, where deliveries reach it from outside. Its back must lie on the park's edge, facing in. This is a physical validity rule beside the existing ones, so commands, loads, and ghosts refuse an inland service entrance as they refuse overlaps (principle 5). EB authors them, and they are saved (principle 1). NN derives the service entrance's door from its box. PO reads the targets and finds which sources are service entrances from intent. LS picks service entrances and depots for the inspectors.
- Candidate resolution: placing or deleting a service entrance, or changing a target, resolves a candidate world as every edit does today. The existing starved marks on shop ghosts then show which shops an edit starves. No new preview is added.
- Inspection records: PO publishes a depot's record (stock and target per good, storage, orders held and out, limiting factor) and a service entrance's record (orders held, deliveries due, its dock queue, limiting factor). Only LS, tests, and the app read them.

## Members

The whole slice depends on day-at-the-park, which gives food and drink as separate goods, and on good-paths, which gives each backstage segment its width.

1. `effortless-building/service-entrances`: the player places service entrances on the park's edge with their own tool, physical validity requires a service entrance's back to lie on the edge, and sets each depot's target for each good, both as saved intent with commands, ghosts, and rendering. Depends on: none.
2. `navigable-networks/service-doors`: a service entrance's front door joins the backstage network and makes it a backstage route distance source, and path capacity is stated as a function of width. Depends on: member 1.
3. `plausible-operations/stocked-depots`: service entrances deliver goods from outside after a lead time at a dock rate, depots store goods up to their storage and targets and restock from the soonest upstream source, shops order from the soonest source, shipments are slowed by supply traffic, and depots and entrances publish supply offers and inspection records. It adds lines.park and lines-noon.park, and gives every checked-in park and park builder that has a depot a service entrance. Depends on: members 1 and 2.
4. `legible-simulation/seen-supply`: the supply traffic overlay with hover attribution and --overlay supply-traffic, the depot and service entrance inspectors, and carts drawn along the routes of shipments in transit, with the ledger query they need. Depends on: member 3.

## Out of scope

- Rising demand, guest tiers, and new goods that unlock as the park grows. These are the Anno part, for a later slice.
- Processing chains, such as a kitchen that turns ingredients into meals.
- Money: the cost of goods, deliveries, and storage.
- Player-sized buildings. Depots and service entrances have one fixed size here, and storage follows floor area so a later slice can grow it.
- Staff. A depot's hauling rate and an entrance's dock rate are fixed capacities, not staff pools.
- Shipments that reroute around congestion. Routes stay shortest by distance.
- New ghost previews of supply changes, such as previewing load on the trunk before widening it.
- Restocking standards for shops. Shops keep their fixed reorder levels.
- Anything else that leaves the park. Garbage is the exception: litter-and-janitors, which lands first, has staff rooms send garbage over backstage paths to the nearest depot, which disposes of it. This slice must instead send that garbage out of the park through the service entrances, sensibly, as the user asked. Plan it in when this slice is next revised.
- Roads and trucks outside the park.
- Staff crossing between guest and backstage paths. Decision 0040 lets them cross only through buildings. Carts stay on backstage paths.

## Open questions

- Tuning values: the outside lead time, an entrance's dock rate, storage per square meter of floor, a depot's hauling rate, path capacity per meter of width, the congestion curve, and how fast supply traffic decays. They are set while planning stocked-depots and service-doors, and adjusted by playing lines.park through a day.
- Fixed shapes the player can't change in this slice: the size of a service entrance and of a depot. The user chose fixed sizes here, with player-sized buildings as a separate slice.

## Research notes

- Parkitect is the only park game that simulates supply: deliveries arrive at one building, travel in bulk to depots, and take a short last leg to shops. Hauling without depots was tedious, so here ordering straight from an entrance is the slow baseline and depots are the upgrade.
- Builder games feel good when each bottleneck is one readable number that the player can see and widen, like Anno's loading bays or Factorio's belt tiers. They feel tedious when the player sets quantities per route.
- Depot chains avoid cycles by rank, as Factorio's buffer chests never fill each other. Each source's rank is its route distance to the nearest service entrance, and a depot orders only from a lower rank.
- The ledger fixes a delay at send, so congestion fits a delay curve applied per edge at send time. A curve that rises linearly past capacity, such as BPR2 or LOHSE, keeps delays readable where raw BPR explodes.
- Path capacity can be a flow per meter of effective width, or whole cart lanes, which make widening read in steps like belt tiers. service-doors chooses.

Depth is in RESEARCH.md.
