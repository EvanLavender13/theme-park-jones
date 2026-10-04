# operations

This module holds operations: the shops and depots that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Shops and depots meet only through the medium: the flow ledger and route distance. Neither reads the other's state (principles 3 and 6).

## Boundary

addOperations registers:

- the flow kinds supply-orders, supplies, guest-visits, and meals;
- a shop's service state;
- the food-offer field and the resolver that publishes it;
- the systems that step shops and then depots.

It needs the routes module's registrations before it, and addPark calls it after addRoutes.

Shops and depots read the backstage route distance field, the guest network's anchors, parkBoxes, and their own ledger stocks. Every shop box of parkBoxes is a shop, and every depot box is a depot.

Other modules call operations.h for:

- the GuestVisits and Meals flow kinds, through which guests visit shops and receive meals;
- the FoodOffer field and its OfferEntry;
- nearestDepot;
- shopRecord and limitingFactorName;
- the consumption causes.

## Supply

A shop is supplied when the backstage network connects it to a depot. nearestDepot returns the depot with the shortest backstage route, or none. A shop is starved when it has no backstage connection, or its connection reaches no depot. If a shop is starved, it can still be placed, and it simply gets no supplies (principle 5).

If a shop is supplied, it keeps its stock topped up by ordering from its nearest depot. It orders whenever what it holds plus what it has on order falls to REORDER_POINT, and it orders up to ORDER_UP_TO. Orders take ORDER_DELAY to reach the depot. Supplies travel back at SUPPLY_SPEED, so they take longer on a longer backstage route. A depot is an unlimited source.

If a shop's depot is deleted, the shop reorders from another depot at once. A shop never holds more than ORDER_UP_TO, counting what is on its way. If a depot can no longer reach a shop, it sends the shop's orders back unfilled.

Every unit is accounted for when shops, depots, or routes are deleted, so the supplies created always equal the orders filled, and the meals created always equal the supplies served (principle 3). If an order or supply is in transit to an endpoint that is deleted, it goes back to its sender or is consumed with a named cause.

## Service

A shop serves the guests who visit it, one at a time, at most one per SERVICE_INTERVAL, while it holds supplies. Its meals are limited by the scarcest of demand, supply, and service rate. Visits are served in the order they arrived. When a guest is served, its visit comes back with a meal, in the same packet timing. If a visit comes back with a meal, the guest was served, and if it comes back without a meal, the guest was not.

If a shop is starved, it turns away, with no meal, every queued guest beyond the supplies it holds or has on the way. It turns away only those, since supplies already shipped always arrive. If a shop is supplied, it never turns a guest away, however long its queue.

When a shop is deleted, the guests waiting at it get their visits back with no meal. If a visit or meal is sent to a guest who has left, it is consumed as abandoned.

## Food offer

Each shop publishes what it offers to guests in the entry field food-offer. It publishes one OfferEntry at its guest anchor, holding:

- the hunger a meal relieves, on a scale from 0 to 1;
- the ticks the next guest to arrive would wait to be served;
- whether it has meals at all.

If a shop is starved, it offers no relief, no wait, and no meals. If a shop has no guest connection, it publishes nothing.

The wait is exact under two assumptions: no other visit arrives, and supplies come as the shop expects. Then the next guest is served in the cycle the wait says. If a shop is newly placed, or is in a candidate, it offers at once, before it has stepped. If an edit cuts or restores a shop's supply route, the shop's offer changes from that tick.

## Inspection

shopRecord(world, key) returns a shop's inspection record for display (decision 0025). The record holds its stock, its queue, what it has on order, whether it is starved, and its limiting factor. The record is computed, not stored, so it adds nothing to a save, and a candidate's shops have theirs at once.

The limiting factor is NoSupplyRoute for a starved shop. Otherwise it is:

- Demand when no one is queued;
- Supply when the shop holds fewer supplies than guests queued;
- ServiceRate when it has enough for everyone queued.

limitingFactorName returns each factor's display name.
