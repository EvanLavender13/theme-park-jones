# operations

This module holds operations: the shops, depots, benches, and toilet blocks that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Shops and depots meet only through the medium: the flow ledger and route distance. Neither reads the other's state (principles 3 and 6).

## Boundary

addOperations registers:

- the flow kinds supply-orders, supplies, guest-visits, meals, drink-orders, drink-supplies, drink-visits, and drinks;
- a shop's service state;
- the fields food-offer and drink-offer and the resolvers that publish them;
- the systems that step shops and then depots.

addOperations then calls addFacilities, which registers:

- the flow kinds rest-visits, rests, toilet-visits, and toilet-uses;
- a facility's service state;
- the fields rest-offer and toilet-offer and the resolvers that publish them;
- the system that steps benches and toilet blocks, after the depots.

addOperations last calls addSeats, which registers the flow kind seat-visits, a piece's seat state, the field seat-offers and the resolver that publishes it, and the system that steps the pieces holding seat visits, after benches and toilet blocks. It needs furniture's registrations before it, and addPark calls addFurniture before addOperations.

It needs the routes module's registrations before it, and addPark calls it after addRoutes.

Shops, depots, and facilities read the backstage route distance field, the guest network's anchors, parkBoxes, and their own ledger stocks. Every shop box of parkBoxes is a shop, and every depot box is a depot. Every bench box is a bench, and every toilet block box is a toilet block. Benches and toilet blocks are facilities. A shop sells the goods its stock setting names (park/SPEC.md).

Seats read the seat places field, their own seat offers, and their own ledger stocks. They read furniture only through the seat places field.

Other modules call operations.h for:

- the GuestVisits and Meals flow kinds, through which guests visit shops for food and receive meals, and DrinkVisits and Drinks, for drink;
- the FoodOffer and DrinkOffer fields and their OfferEntry;
- nearestDepot;
- shopRecord and limitingFactorName;
- the consumption causes.

Other modules call facilities.h for the RestVisits and Rests flow kinds, through which guests visit benches and receive rests, ToiletVisits and ToiletUses, for toilet blocks, the RestOffer and ToiletOffer fields, and facilityRecord.

Other modules call seats.h for the SeatVisits flow kind, through which guests visit furniture and get rests, the SeatOffers field and its SeatOffer, SEAT_TIME, and the relief of each kind's seat.

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

- the need a serving relieves, on a scale from 0 to 1: MEAL_RELIEF for food and DRINK_RELIEF for drink. Guests weigh this relief when they choose the shop. A drink relieves thirst by it, but the hunger a meal relieves depends on where the guest eats it (sim/guests/SPEC.md, Meals);
- the ticks the next guest to arrive for the good would wait to be served;
- whether it has the good at all.

A shop offers each good it sells. It also offers a good it no longer sells while it still holds or awaits some of it. If a shop is starved, each of its offers gives no relief, no wait, and no serving. An offer of a good the shop no longer sells gives the same when the next guest for it would be beyond the shop's cover. If a shop has no guest connection, it publishes nothing.

The wait is exact under two assumptions: no other visit arrives, and supplies come as the shop expects. Then the next guest for the good is served in the cycle the wait says, whatever goods the guests ahead of it came for. If a shop is newly placed, or is in a candidate, it offers each good it sells at once, before it has stepped. If an edit cuts or restores a shop's supply route, its offers of the goods it sells change from that tick. If an edit switches a shop away from a good it still holds or awaits, the shop's offer of that good stays as the shop last published it, in a candidate and in the committed world, until the shop steps. Other edits before that step leave it as it was too.

## Inspection

shopRecord(world, key) returns a shop's inspection record for display (decision 0025). The record holds its stock and what it has on order of each good, its queue of guests for either good, whether it is starved, and its limiting factor. The record is computed, not stored, so it adds nothing to a save, and a candidate's shops have theirs at once.

shopRecord can also keep the lists it works with in TickBuffers the caller keeps, and then allocates nothing once they have grown.

The limiting factor is NoSupplyRoute for a starved shop. Otherwise it is:

- Demand when no one is queued;
- Supply when the shop holds fewer of some good than guests queued for it;
- ServiceRate when it holds enough of each good for everyone queued for it.

limitingFactorName returns each factor's display name.

## Benches and toilet blocks

A bench has BENCH_SEATS seats and a toilet block has TOILET_STALLS stalls. Its seats or stalls are its places. A facility needs no supplies.

A facility takes the guests who visit it in the order their visits arrived. Whenever one of its places is free, it gives the place to the guest at the front of its queue. A guest keeps its place for SITTING_TIME on a bench and TOILET_TIME in a toilet block. Then its visit comes back with a rest or a toilet use, and the place is free from that tick. Guests beyond the places wait at the door in arrival order, and none is turned away. Guests visit a bench through rest-visits and get rests, and visit a toilet block through toilet-visits and get toilet uses.

When a facility is deleted, the guests waiting at its door get their visits back with no serving. The guests in its places still get theirs with a serving. If a visit or serving is sent to a guest who has left, it is consumed as abandoned. Every unit is accounted for, so a facility's servings created always equal the guests it has seated.

Each bench publishes its offer in the entry field rest-offer, and each toilet block in toilet-offer. It publishes one OfferEntry at its guest anchor, holding REST_RELIEF or TOILET_RELIEF, the ticks the next guest to arrive would wait for a place, and that it has places. The wait is exact if no other visit arrives. If a facility is newly placed, or is in a candidate, it offers a wait of 0 at once, before it has stepped. If a facility has no guest connection, it publishes nothing.

facilityRecord(world, key) returns a facility's inspection record for display (decision 0025). The record holds its places, how many of them are taken, and how many guests wait at its door. It is computed from the facility's state, not stored, so it adds nothing to a save. For a key that is not a bench or toilet block box, it returns none. It allocates nothing.

## Seats

Each piece of furniture with a seat place serves guests from its seats. It needs no supplies.

Each such piece publishes one SeatOffer in the resolved entry field seat-offers, at its seat place's place. The offer holds the tiredness a rest there relieves and the hunger a meal eaten there relieves, each on a scale from 0 to 1, and the piece's number of seats. A table relieves a meal more and tiredness less than a bench. The offers follow from the seat places alone, so a candidate's pieces offer at once, and a piece no longer laid offers nothing. An offer has no wait, because no guest waits at a piece.

Guests visit a piece through seat-visits. A piece takes the visits that reach it in a tick one at a time. While one of its seats is free, it gives the seat to the guest, who keeps it for SEAT_TIME. Then the visit comes back with a rest, and the seat is free from that tick. If every seat is taken, the visit comes back after RETURN_DELAY with no serving. A piece with no seat place has no seats, so every visit to it comes back that way. A tick's work for seats grows with the visits reaching pieces, never with the number of pieces (decision 0031).

When a resolution no longer lays a piece, visits on their way to it, and visits that reached it in the same tick, come back to their guests with no serving. Guests it seated still get their rests when their time is up. If a visit or rest is sent to a guest who has left, it is consumed as abandoned, or as undeliverable when the piece is gone as well. Every unit is accounted for, so the rests furniture creates always equal the guests it has seated.
