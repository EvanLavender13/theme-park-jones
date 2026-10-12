# Research: Seated Meals

## How other games have guests eat and sit

In Planet Coaster, guests buy food and mostly eat it as they walk. Its guests sit at a picnic bench to eat only now and then. Players report that the tables go unused. A forum explanation is that a guest's activity is either sitting, when tired, or eating, when hungry, and rarely both. Guests with drinks never sit. In RollerCoaster Tycoon, a guest sits only when a bench happens to be on its tile (slice RESEARCH.md).

The Sims routes a Sim to a slot on an object. If the route fails or the slot is taken, the object's "route failure" behavior runs, and the Sim gives up and picks something else. A Sim doesn't retry the same object at once.

What this means: games that let guests sit to eat make it a weak pull, and players notice that seating does nothing. This milestone makes the pull a scored option against eating now. A table near the shop wins clearly, and a table far away loses to eating on the move. A guest turned away from a full piece remembers it for a while and picks again, as a Sim does after a route failure.

Rejected: eating and sitting as separate activities, as Planet Coaster does. Seating near food would then do nothing for meals, and the slice exists to make it matter. Queuing at a full piece. served-seats turns guests away at once, so an offer has no wait and the fields can be resolved from intent alone.

Sources: https://forums.frontier.co.uk/threads/guest-not-using-benches.529318 — guests rarely sit and eat together; https://forums.frontier.co.uk/goto/post?id=7498271 — picnic tables go unused; https://massivelyop.com/?p=134890 — picnic tables added for eating; https://linkedbyair.net/bin/The%20Sims%20Design%20Documents/117492946-The-Sims-Failure-Trees.pdf — route failure feedback on objects.

## How a guest scores a seat

A guest already scores an offer as RELIEF_WEIGHT times its need's curve times the offer's relief, plus a weight on the route distance, the wait, and the commitment bonus (src/sim/guests/guests.cpp, scoreOptions). A seat fits the same sum with no wait. Rest seats score by their rest relief against tiredness, and meal seats by their meal relief against hunger.

A seated meal also rests the guest, because the seat visit comes back with a rest. The milestone doesn't add that rest to a meal seat's score. Each field's guarantee covers a score on one relief only (plans/navigable-networks/best-seats/RESEARCH.md). With one relief per score, a guest's own best few seats are always in the field it samples. The rest still comes, as a side effect the score doesn't count.

Eating now is an option with a relief term only: RELIEF_WEIGHT times hunger's curve times STANDING_MEAL_RELIEF. A table beats eating now by RELIEF_WEIGHT times the curve times 0.25, less 0.01 for each meter. A guest just served at hunger 0.7 has a curve of 0.8. A table then breaks even with eating now at about 80 m, and a bench at about 48 m. Both are a short walk in a park of day.park's size.

Rejected: scoring a meal seat by both reliefs. The meal field could then miss a guest's best seat once relief varies by piece. Eating now by a fixed rule when no seat is within some distance. Principle 5 asks for a gradient, and the scored option gives one.

## How a guest knows it is seated

A piece seats a guest or turns it away without telling it which. A turned-away visit comes back after VISIT_DELAY and RETURN_DELAY, 2 ticks (src/sim/operations/operations.h). A seated visit comes back after SEAT_TIME. A guest whose visit hasn't come back after the turn-away time is therefore seated. It learns this only from the flow's timing, and it reads no state of the piece (principle 6).

Rejected: a separate flow that tells a guest it was seated. It adds a flow kind and changes nothing a player sees. Reading the piece's seats. They are private to plausible-operations.

## Where a seated guest is drawn

A piece has 2 or 4 seats, and the piece doesn't say which one a guest took. The guest record can give the piece and its pose, and the renderer can place the guests at one piece on its seats. A rule that only reads the current seated guests can't keep seats unique and keep guests still: when two guests prefer one seat, one of them must move whenever the other sits down or stands up. So the renderer keeps each seated guest's seat from frame to frame. A guest keeps its seat while it stays seated at the piece, and a guest newly seated takes the lowest free seat. Picking reads the same seats, so clicking a seated guest selects it. The seats are presentation only. After a park loads, its first frame gives seats in key order.

If an edit removes a piece while guests sit there, the piece's pose is gone with it. The guests still wait for their visits, which served-seats still returns with rests. They are drawn standing at their ground positions until then. The reliefs they get were saved when they chose the piece, as a guest's serving relief is today.

Rejected: a rule that reads only the current seated guests. Seated guests jump between seats as others come and go. Each guest picking a seat from its own key. On a bench with two guests, they would share a seat half the time. Guests picking a free seat by sensing each other through guest presence. Presence holds the last tick's entries, so two guests sitting down in one tick can take the same seat. It also puts a placement rule in the simulation that only drawing needs.

## How long a guest remembers a full piece

A guest that forgot a full piece at once would walk straight back to it, because the field still ranks it best. A guest that never forgot it could never use it again. The milestone keeps up to SEAT_CHOICES pieces, each for FULL_MEMORY_TIME. A full piece frees a seat within SEAT_TIME, so after FULL_MEMORY_TIME the guest's memory of it is probably out of date. The memory has a fixed size for each guest, so it doesn't grow with the number of pieces.

## Carrying a meal

The meal a shop sends back is a unit of the meals flow. A guest that holds it until it eats is carrying food, and nothing else needs to be saved. If a guest leaves carrying it, the ledger consumes the unit as discarded, so meals conserve (src/sim/medium/flow.h, ledgerSwap). The guest consumes the unit with EATEN_CAUSE when it eats, so meals consumed as eaten still count meals eaten.
