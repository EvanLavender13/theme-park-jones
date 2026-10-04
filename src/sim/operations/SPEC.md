# operations

Operations: the shops and depots that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Shops and depots meet only through the medium, the flow ledger and route distance, and neither reads the other's state (principles 3 and 6).

## Boundary

addOperations registers the flow kinds supply-orders, supplies, guest-visits, and meals, a shop's service state, the food-offer field and the resolver that publishes it, and the systems that step shops and then depots. It needs the routes module's registrations before it, and addPark calls it after addRoutes.

Shops and depots read the backstage route distance field, the guest network's anchors, parkBoxes, and their own ledger stocks. Every shop box of parkBoxes is a shop and every depot box a depot.

Other modules call operations.h: the GuestVisits and Meals flow kinds, through which guests visit shops and receive meals; the FoodOffer field and its OfferEntry; nearestDepot; shopRecord and limitingFactorName; and the consumption causes.

## Supply

A shop is supplied when the backstage network connects it to a depot, and nearestDepot gives the depot with the shortest backstage route, or none. A shop with no backstage connection, or whose connection reaches no depot, is starved: it can be placed, and simply gets no supplies (principle 5).

A supplied shop keeps its stock topped up by ordering from its nearest depot whenever what it holds and has on order falls to REORDER_POINT, ordering up to ORDER_UP_TO. Orders take ORDER_DELAY to reach the depot, and supplies take longer to come back the longer the backstage route, at SUPPLY_SPEED. A depot is an unlimited source. A shop whose depot is deleted reorders from another at once, and a shop never holds more than ORDER_UP_TO counting what is on its way. A depot that can no longer reach a shop sends its orders back unfilled.

Every unit is accounted for when shops, depots, or routes are deleted. Orders and supplies in transit to a deleted endpoint go back to their sender or are consumed with a named cause, so the supplies created always equal the orders filled, and the meals created always equal the supplies served (principle 3).

## Service

A shop serves the guests who visit it, one at a time, at most one per SERVICE_INTERVAL, while it holds supplies, so its meals are limited by the scarcest of demand, supply, and service rate. Visits are served in the order they arrived. A served guest's visit comes back with a meal, in the same packet timing, so a returned visit with a meal means served and one without means not.

A starved shop turns away, with no meal, every queued guest beyond the supplies it holds or has on the way, and only those, since supplies already shipped always arrive. A supplied shop never turns a guest away, however long its queue.

When a shop is deleted, the guests waiting at it get their visits back with no meal. A visit or meal sent to a guest who has left is consumed as abandoned.

## Food offer

Each shop tells guests what it offers through the entry field food-offer, one OfferEntry at its guest anchor: the hunger a meal relieves, on a scale from 0 to 1, the ticks the next guest to arrive would wait to be served, and whether it has meals at all. A starved shop offers no relief, no wait, and no meals. A shop with no guest connection publishes nothing.

The wait is exact under its assumptions: if no other visit arrives and supplies come as the shop expects, the next guest is served in the cycle the wait says. A newly placed shop, or one in a candidate, offers at once, before it has stepped. An edit that cuts or restores a shop's supply route changes its offer from that tick.

## Inspection

shopRecord(world, key) gives a shop's inspection record for display (decision 0025): its stock, its queue, what it has on order, whether it is starved, and its limiting factor. It is computed, not stored, so it adds nothing to a save, and a candidate's shops have theirs at once. The limiting factor is NoSupplyRoute for a starved shop, and otherwise Demand when no one is queued, Supply when it holds fewer supplies than guests queued, and ServiceRate when it has enough for everyone queued. limitingFactorName gives each factor's display name.
