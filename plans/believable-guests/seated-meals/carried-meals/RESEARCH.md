# Research: carried-meals

## How a guest holds the meal it carries

A shop sends a served guest's meal back as one unit of the meals flow, addressed to the guest's own key (src/sim/operations/operations.cpp, GoodKinds). Today takeVisit in src/sim/guests/guests.cpp consumes it with EATEN_CAUSE in the tick the visit comes back. If takeVisit leaves the unit in the guest's stock, the guest is carrying food, and unitsHeld of meals at the guest says so. Nothing new needs saving for it, because saves already hold the ledger.

If the guest leaves while it holds the meal, the ledger's swap finds the stock's endpoint gone, and its handle, the guest's own key, gone too. It consumes the unit with DISCARDED_CAUSE (src/sim/medium/flow.cpp, the orphaned stocks at the end of the swap). So meals conserve with no code in guests. No check in tests/ counts meals consumed by cause.

Rejected: a saved carrying flag on the guest. It would repeat what the ledger holds, and the two could disagree.

## The reliefs a meal seat gives

best meal seats holds each piece's meal relief as an entry's Value (src/sim/wayfinding/best_seats.h). The rest relief that a seated meal also gives isn't in that field. seat-offers holds both reliefs, and its slot is keyed by the piece, so `sourceSlot<SeatOffers>(world, piece)` finds the piece's offer, as seatsOf in src/sim/operations/seats.cpp does. The guest reads it once, when it picks the meal seat, and keeps it on its entity beside ServingRelief. If an edit removes the piece later, the guest still has both reliefs when its rest comes back (served-seats sends rests to seated guests whose piece is gone).

A rest seat keeps its single relief in ServingRelief, as seat-rests built it.

## When the guest chooses after it is served

After a facility visit comes back, awaitVisit clears the target and the turn, and the guest makes its next choice for the node it stands on. A shop's door is at a node, so that already happens in the same tick, but nothing forces it. seat-rests passes mustChoose to walk when a seat visit comes back, because a seat place may lie inside an edge. A guest that comes back with a meal takes the same path, so it chooses in that tick wherever it stands.

## What the Inspector says while a guest carries food

The six-row table shows each need's best option and the chance of any option of that need. While a guest carries food, its only options answer hunger. If nothing else changed, the thirst, tiredness, and toilet rows would say "no offer", and heading home's row would say "no route home". Both are false explanations (principle 8). The rows other than hunger's say instead that the guest is carrying food. Hunger's row counts eating now with the meal seats, and names eating now as "eat now" when it scores highest. A "Carrying" row in the guest's rows says whether it carries food. Whether its last meal was seated, and the relief lost, are explained-seating's to show.

Rejected: letting a carrying guest also weigh carrying on and heading home. The milestone limits it to eating now and meal seats, so a meal is never carried for long.
