# Capability: Plausible Operations

## Summary

plausible-operations deepens how believably the park runs behind what the player sees (principle 7). Shops need supplies that travel real routes, service takes time, and when something is missing the shortfall shows up where it happens and can be traced to its cause. The player designs how the park looks. This capability makes sure what they built has to work, and shows them when it does not.

It owns operational entities (shops and depots now; staff, maintenance, and deliveries later), the flows that keep them running, and the offers they make to guests. Its value is that the park's success depends on how it is laid out and supplied, not only on what it contains. It deepens with staff, money, more goods, maintenance, and player-set standards, and never finishes.

## Foundation criteria

- A depot and a generic food shop are derived from their box intent, committed or candidate. The depot is an unlimited source of supplies in the foundation, recorded in the ledger as created when it ships.
- A shop keeps supplies stocked with an (s, S) policy. When its stock plus the supplies already on order falls to its reorder point, it sends order units to the nearest depot it can reach on the backstage network, found from route distance entries at its backstage anchor. The depot consumes each order unit when it ships the matching supplies. An order it cannot fill, because the shop is no longer reachable from it, goes back to the shop as unfilled. When a depot is deleted, the ledger returns every order it holds to the shop the order is addressed to, as unfilled, and orders still in transit to it come back to their shop by the ledger's return-to-sender rule (plans/shared-medium/first-field-and-flow/MILESTONE.md, criterion 11). So a shop's inventory position never counts an order that will not arrive, and it reorders as soon as a route exists again. Each shipment's delay comes from the route distance between them. Orders and supplies conserve exactly, every tick: orders placed equal orders in transit either way plus orders held by the depot plus orders fulfilled or returned unfilled, and supplies created equal supplies in transit plus supplies in shop stock plus supplies converted to meals.
- A shop serves its queue of guest visits in arrival order, at most one guest per service interval. It converts one supply into one meal for each guest served, sends the meal to that guest, and returns the visit as served. Meals served are limited by the scarcest of demand (guests queued), supply (stock), and the fixed service rate. Meals conserve with the ledger's identity.
- A shop publishes a food offer at its guest anchor. The offer holds the relief a meal gives, the expected wait, and whether the shop is supplied. The expected wait is for the next guest to arrive, who stands behind the whole current queue. It is the later of two times: when that guest's turn comes, one service interval after the guest ahead of it is taken, and when a supply unit for that guest becomes available. Supply units come first from stock, then from shipments in transit in order of arrival, and beyond those from the order the shop would place now, arriving after the order's delay and the route delay. The wait is computed exactly from the shop's state and the route distance it samples, and it is tested that way, including for an empty new shop and for a shop with nothing on order. A starved shop's offer says no meals and has no wait.
- A shop is supplied while a route to some depot exists. Without one it is starved, and its offer says no meals from the next tick. It keeps serving queued guests from its stock and from shipments already in transit. It returns the queued guests those cannot cover unserved at once, and returns later arrivals the same way. When a shop is deleted, the ledger returns every queued guest's visit to that guest, unserved, and records its remaining supplies and meals as consumed with the cause "discarded". Guest visits and shipments still in transit to it return to their senders by the same return-to-sender rule.
- A shop publishes an inspection record (decision 0025): stock, queue length, supplies on order, and its limiting factor, which is one of demand, supply, service rate, or no supply route. Its record includes whether it is starved. effortless-building's box rendering reads that for display and marks the box, since rendering is display and never feeds back into the simulation (decision 0025).
- Every behavior above is tested against synthetic guest visits and synthetic networks, without believable-guests.

## Medium

- Food offer: an entry field this capability produces at each shop's guest anchor. believable-guests samples it to choose (decision 0019), and legible-simulation samples it for the overlay and attribution. Resolution publishes the offer from intent and route distance alone, so a preview or a newly committed or moved shop is sampled with a complete offer at once. Stepping republishes it from the shop's state as the queue and stock change, visible from the next tick, and a sample prefers the stepped offer. A resolution that changes a shop's resolved offer, such as a removed supply route, clears its stepped one, so the change is seen in the very next tick (plans/shared-medium/first-field-and-flow/MILESTONE.md).
- Route distance: sampled from navigable-networks. A shop samples it at its backstage anchor, to find the nearest depot, to judge whether it is supplied, and to estimate lead time. A depot samples it at its own backstage anchor, with the ordering shop as the source, to judge whether the shop is reachable and to set each shipment's delay.
- Guest visits: a flow drawn from believable-guests into a shop's queue and returned to it, served or unserved. Every visit that comes in goes back out.
- Meals: a flow supplied to believable-guests, one to each guest served.
- Supplies and supply orders: flows internal to this capability, between shops and depots, through shared-medium's ledger. The ledger returns packets whose destination is gone to their sender, records their units as consumed with the cause undeliverable when the sender is gone too, sends a removed endpoint's held units to the entity their handle names, such as a deleted depot's orders to their shops, and records the rest as consumed with the cause discarded. Shops and depots are separate entities and never read each other's state.
- Networks and anchors: the shop and depot find their anchored nodes through shared-medium's network type, as navigable-networks derives them.
- Park intent (decision 0025): shop and depot boxes from effortless-building.
- Inspection records (decision 0025): read by legible-simulation and tests.

## Principles

- Principle 7 is this capability's reason to exist. Operation depends on routes and stock, and failure is local and visible, with a traceable cause (the limiting factor).
- Principle 3 is at risk inside the capability, because shops and depots are tempting to wire together directly. Orders and supplies are flows with conservation tests every tick.
- Principle 6 is at risk from a central dispatcher reading every shop. There is none: each shop decides from its own state and the fields it samples, and each depot from the orders it holds.
- Principle 2 is at risk from starved, unconnected, or deleted shops. Each is a defined state, tested: no offer or a no-meals offer, guests returned, and nothing lost from the ledger.
- Principle 5 is at risk from gating, such as refusing a shop without a supply route. Such a shop is placed and does poorly.
- Principle 8 is served by the limiting factor, and by an expected wait that is exact and testable.

## Dependencies

- deterministic-simulation (world-as-value): the cycle, resolvers, and state registration. Unmet; planned.
- shared-medium (first-field-and-flow): entry fields, the flow ledger, and anchors. Unmet; planned.
- navigable-networks (paths-become-routes): anchors for shops and depots, and route distance. Unmet; planned.
- effortless-building (sketch-a-park): shop and depot box intent, and box rendering that shows display state a box's owner publishes, such as starved. Unmet; planned.
- believable-guests: not needed for the foundation, which is tested with synthetic guest visits.

## Foundation

The foundation is one depot and one kind of generic food shop, running on orders, supplies, and service over real routes, publishing an offer and explaining its limiting factor. That is the smallest version that makes a layout succeed or fail for operational reasons: cut the route and the shop starves. It produces value on its own, since it can be exercised and observed with synthetic guests before guests exist.

## Milestones

1. `supplied-food-shop`: the depot and the generic food shop, resolved from committed or candidate intent, with (s, S) orders and supplies over the backstage network, meal service limited by the scarcest of demand, supply, and service rate, the food offer, unserved returns, the inspection record with its limiting factor, and the starved marking on the box. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, navigable-networks' paths-become-routes, and effortless-building's sketch-a-park.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- Staff as a capacity pool drawn over network distance, replacing the fixed service rate (decision 0018).
- Restocking standards the player sets, such as how full shops are kept, instead of fixed s and S (principle 9).
- Deliveries into the park: depots restocked from the entrance or a delivery point, instead of being unlimited.
- More goods, such as drinks, with a shop's identity emerging from what it is supplied with (docs/design-notes.md).
- Money: prices, costs, and payment as the result of an exchange.
- Garbage as a flow from shops and guests into bins, and litter pressure as a field.
- Capacity on backstage routes, so congested supply routes slow shipments (shared-medium's edge capacity).
- Maintenance as a flow for rides, with infrastructure as capacity for it (decision 0006).
- Dirt as a stock in toilet blocks that builds with use and is drained by cleaning staff, lowering the toilet offer's appeal rather than closing it.

## Open questions

- Reorder levels, batch size, service interval, and relief per meal are tuning values. They are set while planning supplied-food-shop and adjusted by playing.

## Research notes

- Parkitect's depots and haulers show the structure (source, depot, network, shop), and that a stock-out must be visibly local.
- The (s, S) policy with an inventory position that counts orders in transit avoids double-ordering.
- Little's law justifies the wait estimate: queue ahead over service rate.
- SimCity 4 shows aggregate trips assigned over a network, with cosmetic figures and a route query for tracing, carry a beloved simulation; its failures came from commute-only trips and greedy assignment, not from lacking agents.

Depth is in RESEARCH.md.
