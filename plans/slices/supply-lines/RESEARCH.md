# Research: supply-lines

## How do theme park games get goods into the park?

Parkitect is the only major park game that simulates supply. Crates of goods arrive at a Deliveries building, which the developer advises placing near the park entrance. Tubes or conveyors carry them to Depots placed around the park. Each shop is linked by the player to one Depot, and Haulers carry crates the last few meters to the shop. Trash flows the other way. The developer says no real park moves cargo like this, and chose it because it plays better. Reviewers found hauling slow and tedious before depots existed, and efficient but costly with them. They also liked the immersion rule: guests dislike seeing staff stock stalls, so players hide backstage paths behind hedges. The result is an interleaved maze of guest and backstage paths that reviewers enjoyed.

RollerCoaster Tycoon and Planet Coaster have no deliveries. Stock is abstract, and each sale has a cost. Planet Coaster players ask for staff to wheel deliveries from the entrance, and Planet Coaster 2's reviews call its management shallow by design.

This suggests the slice's shape is sound. Goods enter at one point, travel in bulk to depots, and take a short last leg to shops. Ordering straight from a service entrance with the full lead time keeps a slow baseline, so depots become the upgrade, as they did in Parkitect.

Rejected: abstract unlimited stock, as in RollerCoaster Tycoon and Planet Coaster — it removes the decisions this slice exists for, and it is the most common complaint about Planet Coaster's management. Parkitect's manual link from shop to depot — it is an individual instruction (principle 9), and choosing the soonest source makes the same hub-and-spoke shape appear without it.

Sources: https://www.texelraptor.com/blog/devlog-update-110 — Parkitect's Deliveries, tubes, Depots, and Haulers, and why; https://www.pcgamer.com/parkitect-review/ — hauling tedious without depots, praise for hidden backstage paths; https://coasterforce.com/forums/goto/post?id=996608 — Deliveries placement and Depot cost; https://godisageek.com/reviews/planet-coaster-2-review/ — Planet Coaster 2's management shallow by design.

## What makes logistics in builder games satisfying rather than tedious?

In Anno 1800, every warehouse on an island shares one storage pool, so a warehouse is only an access point. The bottleneck is its loading bays. A cart holds a bay for a set time, and upgrades add bays. Queues of carts show on screen, and the developers chose that for readability. Players complain about jams when warehouses sit too close to producers and about micromanaging trade routes. Factorio and Satisfactory give each belt tier a fixed throughput, and players reason in ratios. In Transport Fever 2, a terminal serves one vehicle at a time, loading time is the usual early bottleneck, and a line counts as congested when a third of its vehicles are stopped. Timberborn keeps one good per storage building and balances districts by how full their storage is. Players find its distribution slow and its balancing opaque.

The satisfying parts are fixed, readable throughput numbers, bottlenecks the player can see, such as a queue at a dock or a backed-up belt, and upgrades that visibly widen the bottleneck. The tedious parts are setting quantities per route and watching goods move one unit at a time. For this slice, an entrance's dock rate and a depot's hauling rate work like Anno's bays: one number sets each bottleneck. Showing capacity as a number, and drawing carts that wait at a dock, makes the bottleneck readable. One target per good per depot is about as much control as players accept.

Rejected: Anno's island-wide shared pool — it removes the depot placement decisions this slice is built on. Balancing by percentage full, as Timberborn does — it is opaque and can move goods back and forth. Dropping cargo when a node overflows, as Transport Fever 2 does — it breaks conservation (principle 3), and aggregate flows don't need the cap.

Sources: http://www.anno-union.com/?p=9737 — Anno's shared storage, bays as the bottleneck, readability; https://steamcommunity.com/app/916440/discussions/0/6039921111957848235 — Anno jams and route micromanagement; https://satisfactory.fandom.com/wiki/Belt — belt tier throughputs; https://steamcommunity.com/app/1066780/discussions/0/1746772488905106339 — Transport Fever 2 terminal limits; https://timberborn.org/articles/district-management-guide — Timberborn's district balancing; https://wiki.coigame.com/Unit_Storage — Captain of Industry storage and ports.

## How should congestion slow a shipment that keeps its route?

Traffic models give travel time on a link as its free-flow time stretched by a function of load against capacity. The BPR function multiplies free-flow time by one plus 0.15 times the load ratio to the fourth power. It has no wall at capacity, but past capacity it climbs so steeply that delays become hard to read. Davidson's function comes from queuing and goes to infinity at capacity. Spiess's conical functions, and the BPR2 and LOHSE variants in PTV Visum, keep the curve rising but give it a finite or linear slope above capacity, which is the usual fix. MATSim instead runs each link as a first-in, first-out queue with a flow capacity, adding fractional capacity each one-second step and releasing a vehicle when a whole one accumulates. That conserves flow by construction and gives a queue to draw.

For this slice, the ledger fixes a packet's delay when it is sent and never reads a network. That fits a delay function applied at send time to each edge on the route, using the load recorded on it. It doesn't fit a per-edge queue, which would need packets to move edge by edge. A curve with a linear tail above capacity keeps delays readable. Routes stay fixed, so there is none of the oscillation that rerouting by congestion causes.

Rejected: Davidson's function — infinite delay at capacity. Raw BPR past capacity — delays explode and are hard to read. A per-edge queue as in MATSim — it needs shipments to step along edges, which the ledger doesn't do. Rerouting to balance load — the user chose fixed routes, and rerouting adds feedback loops.

Sources: https://cgi.ptvgroup.com/vision-help/VISUM_2025_ENG/Content/1_Benutzermodell%20IV/1_5_Vordefinierte%20CR-Funktionen.htm — BPR, BPR2, conical, LOHSE, and Akcelik forms; https://pubsonline.informs.org/doi/10.1287/trsc.24.2.153 — Spiess's conical functions; https://onlinepubs.trb.org/onlinepubs/circulars/ec018/19_61.pdf — Davidson's function; https://www.strc.ch/2010/Dobler.pdf — MATSim's queue model.

## How can depots restock from each other without ordering cycles?

Clark and Scarf showed that base-stock policies by echelon are optimal for serial supply chains, where a node's echelon stock is its own stock plus everything downstream and in transit. Orders swing more as they move upstream, the bullwhip effect. The swings grow with lead time and with order-up-to policies that react fully to every change. A common cause is ignoring stock already in transit. Ordering only a fraction of the gap each review damps the swings, at the cost of holding more stock.

Factorio prevents cycles by rule: a buffer chest never fills its request from another buffer chest, and sources rank by type, with ties going to the nearest chest. For this slice, the same effect comes from giving each source a rank, its route distance to the nearest service entrance, with entrances at zero. A depot then orders only from a source of strictly lower rank. Goods move only away from the entrances, the rule is deterministic, and ranks change only when the network changes. A depot's inventory position should count goods on order and in transit, as shops' (s, S) policy already does, or it double-orders when lead times are long.

Rejected: each depot picking its nearest source with stock, with no rank — two depots can feed each other forever. Global optimization across echelons — the player can't follow it.

Sources: https://wiki.factorio.com/Buffer_chest — buffer chests never fill each other; https://wiki.factorio.com/Logistic_network — source priority and nearest-chest ties; https://pubsonline.informs.org/doi/pdf/10.1287/educ.1063.0026 — echelon base-stock policies; https://ore.exeter.ac.uk/rest/bitstreams/193948/retrieve — proportional order-up-to and bullwhip damping.

## How should a backstage path's width set its throughput?

Pedestrian planning measures walkway flow per meter of width per minute. Fruin's levels of service step at roughly 23, 33, 49, 66, and 82 people per minute per meter, and US highway guidance puts capacity near 75. Past capacity, flow becomes erratic and drops. The Highway Capacity Manual uses effective width, the width minus a buffer at each edge. Disney's service tunnels are wide enough for two carts to pass.

Two mappings fit. Capacity can be a flow per meter times the width less an edge buffer. Or it can be a whole number of cart lanes in the effective width, times a rate per lane, so widening shows as discrete steps, like belt tiers. Either way, sizing capacity to a comfortable level of service and letting the delay curve act above it reads better than a hard wall.

Rejected: capacity linear in width with no edge buffer — it overstates narrow paths. Modeling flow that drops past capacity — it invites gridlock.

Sources: https://www.fhwa.dot.gov/publications/research/safety/pedbike/98107/section3.cfm — walkway capacity and levels of service; https://www.nyc.gov/assets/planning/download/pdf/plans/transportation/td_pedloschaptertwo.pdf — Fruin's and the HCM's levels; https://en.wikipedia.org/wiki/Disney_utilidor_system — backstage cart corridors.
