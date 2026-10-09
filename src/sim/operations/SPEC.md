# operations

This module holds operations: the shops and depots that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Shops and depots meet only through the medium: the flow ledger and route distance. Neither reads the other's state (principles 3 and 6).

## Boundary

addOperations registers:

- the flow kinds supply-orders, supplies, guest-visits, meals, drink-orders, drink-supplies, drink-visits, and drinks;
- a shop's service state;
- the fields food-offer and drink-offer and the resolvers that publish them;
- the systems that step shops and then depots.

It needs the routes module's registrations before it, and addPark calls it after addRoutes.

Shops and depots read the backstage route distance field, the guest network's anchors, parkBoxes, and their own ledger stocks. Every shop box of parkBoxes is a shop, and every depot box is a depot. A shop sells the goods its stock setting names (park/SPEC.md).

Other modules call operations.h for:

- the GuestVisits and Meals flow kinds, through which guests visit shops for food and receive meals, and DrinkVisits and Drinks, for drink;
- the FoodOffer and DrinkOffer fields and their OfferEntry;
- nearestDepot;
- shopRecord and limitingFactorName;
- the consumption causes.

## Goods

A shop sells food, drink, or both. Each good has its own kinds: supply-orders, supplies, guest-visits, meals, and food-offer for food, and drink-orders, drink-supplies, drink-visits, drinks, and drink-offer for drink. A shop set to food behaves as shops did before drink.

## Supply

A shop is supplied when the backstage network connects it to a depot. nearestDepot returns the depot with the shortest backstage route, or none. A shop is starved when it has no backstage connection, or its connection reaches no depot. If a shop is starved, it can still be placed, and it simply gets no supplies (principle 5).

If a shop is supplied, it keeps its stock of each good it sells topped up by ordering from its nearest depot. It orders a good whenever what it holds of it plus what it has on order falls to REORDER_POINT, and it orders up to ORDER_UP_TO. Orders take ORDER_DELAY to reach the depot. Supplies travel back at SUPPLY_SPEED, so they take longer on a longer backstage route. A depot is an unlimited source of both goods.

If a shop's depot is deleted, the shop reorders from another depot at once. A shop never holds more than ORDER_UP_TO of a good, counting what is on its way. If a depot can no longer reach a shop, it sends the shop's orders back unfilled.

If the player switches a shop away from a good, it orders no more of it. Orders it has already sent are still shipped.

Every unit is accounted for when shops, depots, or routes are deleted, so each good's supplies created always equal its orders filled, and its servings created always equal its supplies served (principle 3). If an order or supply is in transit to an endpoint that is deleted, it goes back to its sender or is consumed with a named cause.

## Service

A shop is one counter. It serves the guests who visit it one at a time, at most one per SERVICE_INTERVAL, in the order their visits arrived, whatever good they came for. Its servings are limited by the scarcest of demand, supply, and service rate. Each guest served takes one supply of the good its visit names. If the shop holds none of that good, the guest at the front waits for it, and so do the guests behind. If a guest has visits for more than one good at one shop, they are taken in the order of the goods, food first. When a guest is served, its visit comes back with a meal or a drink, in the same packet timing. If a visit comes back with a serving, the guest was served, and if it comes back without one, the guest was not.

A shop's cover of a good is the supplies of it the shop holds or has on the way. If a shop is starved, it turns away, with no serving, every queued guest beyond its cover of the good the guest came for. It does the same for a good it no longer sells. It turns away only those, since supplies already shipped always arrive. If a shop is supplied, it never turns away a guest for a good it sells, however long its queue.

When a shop is deleted, the guests waiting at it get their visits back with no serving. If a visit or serving is sent to a guest who has left, it is consumed as abandoned.

## Offers

Each shop publishes what it offers to guests: food in the entry field food-offer, and drink in drink-offer. In each, it publishes one OfferEntry at its guest anchor, holding:

- the need a serving relieves, on a scale from 0 to 1: MEAL_RELIEF for food and DRINK_RELIEF for drink;
- the ticks the next guest to arrive for the good would wait to be served;
- whether it has the good at all.

A shop offers each good it sells. It also offers a good it no longer sells while it still holds or awaits some of it. If a shop is starved, each of its offers gives no relief, no wait, and no serving. An offer of a good the shop no longer sells gives the same when the next guest for it would be beyond the shop's cover. If a shop has no guest connection, it publishes nothing.

The wait is exact under two assumptions: no other visit arrives, and supplies come as the shop expects. Then the next guest for the good is served in the cycle the wait says, whatever goods the guests ahead of it came for. If a shop is newly placed, or is in a candidate, it offers each good it sells at once, before it has stepped. If an edit cuts or restores a shop's supply route, its offers of the goods it sells change from that tick. If an edit switches a shop away from a good it still holds or awaits, the shop's offer of that good stays as the shop last published it, in a candidate and in the committed world, until the shop steps. Other edits before that step leave it as it was too.

## Inspection

shopRecord(world, key) returns a shop's inspection record for display (decision 0025). The record holds its stock and what it has on order of each good, its queue of guests for either good, whether it is starved, and its limiting factor. The record is computed, not stored, so it adds nothing to a save, and a candidate's shops have theirs at once.

The limiting factor is NoSupplyRoute for a starved shop. Otherwise it is:

- Demand when no one is queued;
- Supply when the shop holds fewer of some good than guests queued for it;
- ServiceRate when it holds enough of each good for everyone queued for it.

limitingFactorName returns each factor's display name.
