# Milestone: Seated Meals

Slice: somewhere-to-sit

Status: planned

## Summary

seated-meals makes guests use the furniture along the paths. A tired guest weighs the best rest seats where it stands against its other options, walks to the piece it picks, and sits for a while. A guest who buys food carries it. It weighs the best meal seats against eating now. If it goes to a seat, it eats there when its sitting ends, with the seat's full relief and a little rest. If it eats now, it gets less relief and walks on. A guest that finds a piece full remembers it and picks again, so it moves on toward the next best seat. Seated guests are drawn sitting on the piece's seats. The milestone also remakes the day parks: day.park loses its bench box and gains tables along the west arm and benches along the path in. far-tables.park and unfurnished.park join it. It is member 4 of somewhere-to-sit. Furniture, seat offers, seat service, and the best-seats fields are all in place, and this is the member that makes the player see them used.

## Acceptance criteria

1. A guest that isn't carrying food scores each piece in best rest seats where it stands as an option, unless it remembers the piece as full. The option's terms are a relief term, RELIEF_WEIGHT times tiredness's curve times the piece's rest relief in the field, a distance term on the field's distance, and the commitment bonus while the piece is its target. It has no wait term. These options join the facility offers, carrying on, and heading home. guestOptions gives each seat option with its piece, its need, its terms, and its probability. (play full.park and noon.park with the Inspector open on a tired guest; check: a scratch program reads guestOptions for every guest of noon.park and checks that each seat option's terms add up to its score)
2. A guest heading to a piece follows the field's next steps to the piece's seat place, including a seat place inside an edge. It walks up to the edge of the paving beside the piece and sends a seat visit to the piece. If the field where it stands no longer gives the piece, as after an edit, it drops the target and chooses again at once. (play full.park and noon.park, including editing a furnished segment under a guest heading to a seat)
3. A guest waits at the piece until its visit comes back. If the visit hasn't come back within the turn-away time, VISIT_DELAY plus RETURN_DELAY, the guest is seated. A seated guest is drawn sitting on one of the piece's seats, facing out from the piece, with no two guests on one seat. It keeps its seat until it stands up, whoever else sits down or stands up there. Clicking it selects it. When the visit comes back with a rest, tiredness falls by the rest relief the piece offered when the guest chose it. If it comes back with nothing, the guest remembers the piece as full and chooses again at once. If an edit removes the piece while the guest sits there, the guest is drawn standing at its ground position until its visit comes back, and it takes the rest that comes back as before. (play, and a --capture of noon.park showing guests seated at the tables beside the food shop and on the benches, the slice's criterion 4)
4. A served food visit leaves the guest holding its meal, carrying it. A carrying guest chooses only between eating now and each piece in best meal seats that it doesn't remember as full. A meal seat scores as a rest seat does, by hunger's curve and the piece's meal relief. Eating now has a relief term only, by STANDING_MEAL_RELIEF. At a seat, the guest eats when its visit comes back with a rest: hunger falls by the meal relief and tiredness by the rest relief the piece offered when the guest chose it, even if an edit has since removed the piece. If the guest eats now, hunger falls by STANDING_MEAL_RELIEF at once and it walks on. The guest consumes the meal with EATEN_CAUSE when it eats. If it leaves the park carrying it, the ledger consumes the meal as discarded. (play full.park and noon.park; check: the scratch program steps day.park through a day and checks that meals consumed as eaten equal the meals guests record eating, and that the ledger's identity holds)
5. A guest remembers at most SEAT_CHOICES pieces as full, each for FULL_MEMORY_TIME after it found the piece full. If it finds another piece full while it remembers SEAT_CHOICES pieces, it forgets the one it found full first. (check: the scratch program fills a table in a copy of noon.park and follows a guest turned away from it)
6. The guest record gives whether the guest is carrying food, and the pieces it remembers as full with the tick it found each. While the guest is seated and the piece is still laid, the record gives the piece it sits at and the piece's pose. The last meal records whether it was eaten seated. The guest Inspector names a piece by its kind and key wherever it names a target or option, and lists the pieces the guest remembers as full, so an option missing for that reason is explained (principle 8). explained-seating adds the meal's seated or standing line and the relief lost. (play)
7. tpj_scenarios --slice-parks makes day.park without its bench box. day.park gains tables on both sides of the west arm every 6 m, and benches every 10 m on the side of the path in that is away from the toilet block. noon.park, evening.park, and no-toilet.park follow from it. far-tables.park moves the tables to the east arm, and unfurnished.park has no furniture. noon.park holds a guest seated at furniture, a guest carrying food, and a guest that remembers a full piece, so the park world checks cover each. They drop seat-service from their unheld types. The checks gain an edit of noon.park that places a shop facing a furnished segment, checked against its commit. (ctest)
8. Over one day from 9:00, a smaller share of meals is eaten seated in far-tables.park than in day.park. More guests leave unfurnished.park for tiredness than leave day.park for it. These are the slice's criteria 1 and 2, which the slice's closing writes as integration tests. (check: the scratch program runs each comparison and prints both sides)
9. src/sim/guests/SPEC.md describes guests as built, and the other specs the features touch describe their changes. (check: read each beside the code)

## Medium

Cross-capability, as the slice's medium map assigns:

- Best rest seats and best meal seats (resolved entry fields, from navigable-networks' best-seats): sampled at the guest's place for choice. Each entry's next step leads the guest toward the piece, and the piece's own entry at its seat place says the guest has arrived. seat-rests reads best rest seats, and carried-meals reads best meal seats.
- Seat places (resolved entry field, from effortless-building's furnished-paths): read at a piece's seat place for the piece's pose, which the guest record gives while the guest sits there.
- Seat offers (resolved entry field, from plausible-operations' served-seats): read at the piece's seat place, when the guest chooses a meal seat, for the rest relief a seated meal also gives.
- Seat visits (flow, supplied to plausible-operations): a guest sends one to the piece it reaches. It comes back after SEAT_TIME with a rest if the guest was seated, or after RETURN_DELAY with nothing if the piece was full.
- Rests (flow, drawn from plausible-operations): consumed when a seat visit comes back with one.
- Guest visits and meals (flows, as today): a served guest now holds its meal until it eats.
- Guests (inspection record, read by the renderer and legible-simulation): whether the guest is carrying food, the piece it sits at and the piece's pose while seated, the pieces it remembers as full, and whether its last meal was eaten seated.

Internal to this capability:

- A guest's carried meal is its held unit of meals. The pieces it found full, with the tick it found each, and the reliefs of the seat it chose are saved on its entity. They leave it only through the inspection record.
- Guest presence: unchanged. A seated guest drops its entry, as a waiting guest does.

A guest learns a piece is full only from its visit coming back with nothing, and that it is seated only from its visit not coming back within the turn-away time (principle 6). It never reads the piece's seats.

The record gives the piece and its pose, not a seat, and the renderer puts the guests at one piece on its seats (RESEARCH.md, "Where a seated guest is drawn"). This milestone updates the slice's medium map to match.

## Dependencies

- plausible-operations/served-seats: seat offers, seat visits, and seat service. Complete.
- navigable-networks/best-seats: the best rest seats and best meal seats fields. Complete.
- effortless-building/furnished-paths: furniture, furnishing settings, and seat places. Complete.
- believable-guests/a-day-out: the four needs, choosing, and the day parks. Complete.

## Core feature

`seat-rests` is the core. When it lands, tired guests in furnished-paths.park and full.park walk to benches and tables and are drawn sitting there, and a guest turned away from a full piece walks on to the next. It builds everything seats need: seat options, walking to a seat place inside an edge, seat visits, knowing it is seated, the memory of full pieces, and seated drawing. carried-meals reuses all of it for meals.

## Features

1. `seat-rests`: guests score the best rest seats, walk to a piece by the field, send seat visits, sit, and take their rest. A guest turned away remembers the piece as full and chooses again. Seated guests are drawn on the piece's seats, and the Inspector names pieces. Depends on: none.
2. `carried-meals`: a served guest carries its meal and chooses between the best meal seats and eating now. It eats at a seat with the seat's reliefs or eats now with STANDING_MEAL_RELIEF. The record gives carrying, and the last meal records whether it was seated. Depends on: feature 1.
3. `seat-parks`: tpj_scenarios --slice-parks makes day.park with its furniture and without its bench box, and makes far-tables.park and unfurnished.park. The world checks drop seat-service from their unheld types and gain the shop edit. The scratch program checks criterion 8, and the tuning values are adjusted by watching noon.park. Depends on: feature 2.

Until seat-parks lands, day.park and noon.park hold no furniture, so features 1 and 2 check their criteria by playing full.park, whose guest segments all carry furniture. seat-parks then checks every criterion on noon.park.

Each of features 1 and 2 changes how guests step. Each one remakes every park in tests/parks that holds guests, with --slice-parks and tpj_bench --full-park, and rewrites the guest lines of the play parks in parks/ so they still load. Their integration tests must keep passing. full.park already carries furniture on every guest segment, so its guests use seats from feature 1 on, and the runtime report measures them. Each feature compares its runtime report with the one before it.

Retiring the bench box is the slice's last step, after explained-seating. Until then, guests still weigh bench boxes' rest offers in parks that hold them.

No feature adds a test (docs/testing.md). The slice's closing adds the integration tests for criterion 8.

## Tuning values

These are starting values, adjusted in seat-parks by watching noon.park.

- STANDING_MEAL_RELIEF: 0.25, half a table's meal relief. A guest just served at hunger 0.7 prefers a table over eating now up to about 80 m away, and a bench up to about 48 m (RESEARCH.md, "How a guest scores a seat").
- FULL_MEMORY_TIME: 900 ticks, 30 game minutes, three times SEAT_TIME.
- SEAT_CHOICES: 4, from best-seats. A guest remembers at most this many full pieces.
- SEAT_TIME, and each kind's reliefs: as served-seats set them.
- The relief, distance, and commitment weights and the softmax temperature: as today.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Counting a seated meal's rest in a meal seat's score. Gated on: a field whose guarantee covers a score on two reliefs, or play showing tired guests passing benches to eat at tables.
- Drinks carried to a seat, as the slice leaves out.
- Food that loses relief the longer it is carried, so a guest doesn't carry it far.
- Groups who sit together at one table. Gated on: groups.

## Open questions

None.

## Research notes

- Other games make sitting to eat a weak pull, and players notice their tables go unused. Here it is a scored option against eating now, so a near table wins and a far one loses.
- A guest knows it is seated because its visit didn't come back within the turn-away time, and it knows a piece is full because the visit came back with nothing.
- A guest remembers a few full pieces for a while, as a Sim gives up on an object after a route failure.
- The renderer puts the guests at one piece on its seats, because the piece doesn't say which seat each took. It keeps each guest's seat from frame to frame, so seated guests don't jump.

Details are in RESEARCH.md.
