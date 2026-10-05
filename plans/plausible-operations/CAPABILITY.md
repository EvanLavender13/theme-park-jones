# Capability: Plausible Operations

## Summary

plausible-operations makes the park run believably behind what the player sees (principle 7). Shops need supplies that travel real routes, and service takes time. When something is missing, the shortfall shows up where it happens, and the player can trace it to its cause.

The player designs how the park looks. This capability makes sure that what they built has to work, and it shows them when it doesn't.

It owns:

- operational entities: shops and depots now, and staff, maintenance, and deliveries later;
- the flows that keep them running;
- the offers they make to guests.

This matters because the park's success depends on how it is laid out and supplied, not only on what it contains.

The capability grows over time, with staff, money, more goods, maintenance, and standards the player sets. It is never finished.

## Foundation criteria

A depot and a generic food shop are derived from their box intent, committed or candidate. In the foundation, the depot is an unlimited source of supplies. The ledger records supplies as created when the depot ships them.

A shop keeps supplies stocked with an (s, S) policy. When its stock plus the supplies already on order falls to its reorder point, the shop sends order units to the nearest depot it can reach on the backstage network. It finds that depot from the route distance entries at its backstage anchor.

The depot consumes each order unit when it ships the matching supplies. Each shipment's delay comes from the route distance between the depot and the shop.

Some orders come back unfilled:

- If a depot can't fill an order because the shop is no longer reachable from it, the order goes back to the shop as unfilled.
- When a depot is deleted, the ledger returns every order the depot holds to the shop the order is addressed to, as unfilled.
- Orders still in transit to a deleted depot come back to their shop by the ledger's return-to-sender rule (plans/shared-medium/first-field-and-flow/MILESTONE.md, criterion 11).

A shop's inventory position never counts an order that won't arrive, because every such order comes back. The shop reorders as soon as a route exists again.

Orders and supplies conserve exactly, every tick:

- Orders placed equal orders in transit either way, plus orders held by the depot, plus orders fulfilled or returned unfilled.
- Supplies created equal supplies in transit, plus supplies in shop stock, plus supplies converted to meals.

A shop serves its queue of guest visits in arrival order, at most one guest per service interval. For each guest served, it converts one supply into one meal, sends the meal to that guest, and returns the visit as served.

Meals served are limited by the scarcest of three things:

- demand, which is the guests queued;
- supply, which is the stock;
- the fixed service rate.

Meals conserve with the ledger's identity.

A shop publishes a food offer at its guest anchor. The offer holds:

- the relief a meal gives;
- the expected wait;
- whether the shop is supplied.

The expected wait is for the next guest to arrive, who stands behind the whole current queue. It is the later of two times:

- when that guest's turn comes, one service interval after the guest ahead of it is taken;
- when a supply unit for that guest becomes available.

Supply units come first from stock, then from shipments in transit, in order of arrival. Beyond those, they come from the order the shop would place now, which arrives after the order's delay and the route delay.

The wait is exact. It is computed from the shop's state and the route distance it samples. It is also exact for an empty new shop and for a shop with nothing on order.

If a shop is starved, its offer says no meals and has no wait.

A shop is supplied while a route to some depot exists. Without one, it is starved, and from the next tick its offer says no meals. A starved shop keeps serving queued guests from its stock and from shipments already in transit. If those can't cover every queued guest, it returns the rest at once, unserved. It returns later arrivals the same way.

When a shop is deleted, the ledger returns every queued guest's visit to that guest, unserved. The ledger records the shop's remaining supplies and meals as consumed, with the cause "discarded". Guest visits and shipments still in transit to the shop return to their senders by the same return-to-sender rule.

A shop publishes an inspection record (decision 0025). It holds:

- stock;
- queue length;
- supplies on order;
- its limiting factor, which is one of demand, supply, service rate, or no supply route;
- whether the shop is starved.

effortless-building's box rendering reads whether the shop is starved, for display, and marks the box. Rendering is display only and never feeds back into the simulation (decision 0025).

The foundation works without believable-guests, on synthetic guest visits. Check its behavior by playing and with `--capture`. If it registers state, add that state to a park file in tests/parks/, where the park world checks cover it (docs/testing.md).

## Medium

- Food offer: an entry field this capability produces at each shop's guest anchor. believable-guests samples it to choose (decision 0019). legible-simulation samples it for the overlay and for attribution.
  - Resolution publishes the offer from intent and route distance alone. A preview, or a shop that was just committed or moved, has a complete offer to sample at once.
  - Stepping republishes the offer from the shop's state as the queue and stock change. The new offer is visible from the next tick. If both offers exist, a sample prefers the stepped one.
  - If a resolution changes a shop's resolved offer, for example because a supply route was removed, it clears the stepped offer. The very next tick sees the change (plans/shared-medium/first-field-and-flow/MILESTONE.md).
- Route distance: sampled from navigable-networks.
  - A shop samples it at its backstage anchor to find the nearest depot, to judge whether it is supplied, and to estimate lead time.
  - A depot samples it at its own backstage anchor, with the ordering shop as the source. It uses the sample to judge whether the shop is reachable and to set each shipment's delay.
- Guest visits: a flow that believable-guests sends into a shop's queue. The shop returns each visit to its guest, served or unserved. Every visit that comes in goes back out.
- Meals: a flow to believable-guests, with one meal for each guest served.
- Supplies and supply orders: flows inside this capability, between shops and depots, through shared-medium's ledger. Shops and depots are separate entities and never read each other's state. The ledger handles missing endpoints this way:
  - If a packet's destination is gone, the ledger returns the packet to its sender.
  - If the sender is gone too, it records the packet's units as consumed, with the cause undeliverable.
  - When an endpoint is removed, the ledger sends the units it held to the entity their handle names, such as a deleted depot's orders to their shops. It records the rest as consumed, with the cause discarded.
- Networks and anchors: the shop and depot find their anchored nodes through shared-medium's network type, as navigable-networks derives them.
- Park intent (decision 0025): shop and depot boxes, read from effortless-building.
- Inspection records (decision 0025): read by legible-simulation and tests.

## Principles

- Principle 7 is this capability's reason to exist. Operation depends on routes and stock. When something fails, the failure is local and visible, and the limiting factor shows its cause.
- Principle 3 is at risk inside the capability, because it is tempting to wire shops and depots together directly. Orders and supplies are flows instead, and they conserve exactly every tick.
- Principle 6 is at risk from a central dispatcher that reads every shop. There is none. Each shop decides from its own state and the fields it samples. Each depot decides from the orders it holds.
- Principle 2 is at risk from starved, unconnected, or deleted shops. Each of these is a defined state. The shop has no offer or a no-meals offer, its guests are returned, and nothing is lost from the ledger.
- Principle 5 is at risk from gating, such as refusing to place a shop that has no supply route. The game places such a shop, and the shop does poorly.
- Principle 8 is served by the limiting factor, and by an expected wait that is exact and testable.

## Dependencies

- deterministic-simulation (world-as-value): the cycle, resolvers, and state registration. Met.
- shared-medium (first-field-and-flow): entry fields, the flow ledger, and anchors. Met.
- navigable-networks (paths-become-routes): anchors for shops and depots, and route distance. Met.
- effortless-building (sketch-a-park): shop and depot box intent, and box rendering that shows display state a box's owner publishes, such as starved. Met.
- believable-guests: not needed for the foundation, which runs on synthetic guest visits.

## Foundation

The foundation is one depot and one kind of generic food shop. They run on orders, supplies, and service over real routes. The shop publishes an offer and explains its limiting factor.

It is the smallest version that makes a layout succeed or fail for operational reasons. If the player cuts the route, the shop starves.

It is useful on its own, because it can be run and observed with synthetic guests before guests exist.

## Milestones

1. `supplied-food-shop`: the depot and the generic food shop, resolved from committed or candidate intent. It adds (s, S) orders and supplies over the backstage network, and meal service limited by the scarcest of demand, supply, and service rate. It also adds the food offer, unserved returns, the inspection record with its limiting factor, and the starved marking on the box. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, navigable-networks' paths-become-routes, and effortless-building's sketch-a-park.

Later milestones come from the deepening candidates, once the slice has been played.

## Deepening candidates

- Staff as a capacity pool, drawn over network distance, in place of the fixed service rate (decision 0018).
- Restocking standards the player sets, such as how full shops are kept, instead of fixed s and S (principle 9).
- Deliveries into the park: depots are restocked from the entrance or a delivery point, instead of being unlimited.
- More goods, such as drinks. A shop's identity comes from what it is supplied with (docs/design-notes.md).
- Money: prices, costs, and payment as the result of an exchange.
- Garbage as a flow from shops and guests into bins, and litter pressure as a field.
- Capacity on backstage routes, so congested supply routes slow shipments down (shared-medium's edge capacity).
- Maintenance as a flow for rides, with infrastructure as its capacity (decision 0006).
- Dirt as a stock in toilet blocks. It builds up with use, and cleaning staff drain it. Dirt lowers the toilet offer's appeal but doesn't close the toilet.

## Open questions

- Reorder levels, batch size, service interval, and relief per meal are tuning values. They are set while planning supplied-food-shop, and adjusted by playing.

## Research notes

- Parkitect's depots and haulers show the structure: source, depot, network, shop. They also show that a stock-out must be visibly local.
- The (s, S) policy avoids double-ordering, because its inventory position counts orders in transit.
- Little's law supports the wait estimate: the queue ahead divided by the service rate.
- SimCity 4 assigns aggregate trips over a network, with cosmetic figures and a route query for tracing, and players loved its simulation. Its failures came from commute-only trips and greedy assignment, not from a lack of agents.

The details are in RESEARCH.md.
