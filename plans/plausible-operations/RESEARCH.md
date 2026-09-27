# Research: plausible-operations

## How do park games keep shops supplied?

In Parkitect, goods arrive at the main gate. They travel through delivery tubes to depots, and haulers, who are staff, carry them from depots to the shops assigned to each depot. Players zone haulers so they stay close to their shops. A single hauler in a small zone can keep four or more shops stocked, and stock-outs follow from too few haulers, zones that are too large, or tired staff. The system is legible because its failure is visible and local: an empty shop, and a hauler far away.

For this project, decision 0018 turns the haulers into an aggregate flow: supplies move from depot to shop over the backstage network, taking a delay that grows with route length. Staff come later as a capacity pool. What carries over from Parkitect is the structure (a source, a depot, a network, a shop) and the rule that a stock-out must be traceable to its cause. In the slice, that cause is a missing route or a long one.

Rejected:

- Individual hauler agents in the foundation: decision 0018 starts with aggregate flows, and principle 9 has the player direct through standards, not individual workers.

Sources:

- https://parkitect.fandom.com/wiki/Depot: depots, delivery tubes, and haulers restocking assigned shops.
- https://steamsolo.com/guide/zoning-for-park-staff-restocking-shops-parkitect/: hauler zoning, and how many shops one hauler covers.

## How should a shop decide when to reorder?

The (s, S) policy, also called min-max, is the standard continuous-review rule. When the inventory position falls to or below the reorder point s, an order brings it back up to the level S. The inventory position is stock on hand plus stock already ordered. Counting what is already ordered is what stops a policy from ordering the same shortfall twice while a shipment is in transit. The reorder point is normally set to cover expected use during the lead time, plus a safety margin, and S minus s sets the batch size.

For this project, the lead time is the supply route's travel delay, which the shop can sample from route distance. So a sensible s grows with the route: a distant shop has to reorder earlier. In the foundation, s and S are fixed tuning values. Later they can become a standard the player sets (principle 9), or be derived from the route and demand. Because the policy counts orders already placed, it fits the order flow: units in transit to the depot, and supplies in transit back, both count toward the position.

Rejected:

- Periodic review, which checks at fixed intervals: it adds a timing parameter, and on its own gives nothing over continuous review in a simulation that ticks anyway.
- Ordering from stock on hand only: it double-orders while shipments are in transit.

Sources:

- https://en.wikipedia.org/wiki/Reorder_point: the reorder point as use over the lead time plus safety stock.
- https://smartcorp.com/inventory-control/inventory-control-policies-software/: min-max (s, S) and related policies.

## How should a shop estimate a guest's expected wait?

Little's law says that the average number in a system equals the arrival rate times the average time spent in it, whatever the arrival or service pattern. Its practical form for a single queue is simple: the wait is roughly the number ahead divided by the service rate.

For this project, a shop's expected wait is the time to serve the guests already queued at its fixed service rate. The wait is for the next guest to arrive, standing behind the whole queue. It is the later of that guest's turn at the service rate and the time a supply unit for that guest becomes available. Units come from stock, then shipments in transit by arrival tick, which the shop knows from its own orders, then an order it would place now. This is an estimate the shop publishes in its food offer, not a promise. Guests act on it, and legible-simulation can show it. An estimate that can be computed exactly from state is also testable.

Rejected:

- Simulating the queue ahead of time to predict the wait: it costs more, and the slice rules out previews that simulate ahead.

Sources:

- https://en.wikipedia.org/wiki/Little's_law: L = λW and why it holds generally.
- https://eng.libretexts.org/Bookshelves/Civil_Engineering/Fundamentals_of_Transportation/05:_Traffic/5.01:_Queueing: practical queue waiting estimates.
