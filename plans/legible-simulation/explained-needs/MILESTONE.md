# Milestone: Explained Needs

Slice: day-at-the-park

Status: planned

## Summary

explained-needs lets the player see how well the park serves each of a guest's four needs, and why guests leave. Each need gets an overlay on the guest paving: food around shops that sell food, drink around shops that sell drink, rest around benches, and the toilet around toilet blocks. The player picks one at a time. Each overlay fades with the walk and the wait in game minutes, by one rule for every need. A value of nothing shows the ramp's worst color, with no separate color for it, so the shading darkens smoothly and never stops partway along a path, and the abrupt, flickering white edge of today's food overlay goes away. Hovering lists each facility's part of the value, and a ghost previews the coverage it would add. Benches and toilet blocks get inspectors. The guest inspector's choice table shrinks to one row per need. A departures panel counts today's departures by reason, and the Hours panel draws the arrival curve for the hours being chosen. It is the day-at-the-park slice's last member. Every offer, record, and candidate it reads is in place, and once it lands the slice can close.

## Acceptance criteria

1. Each need has an overlay on the guest paving, shaded by its availability: food by shops' food offers, drink by shops' drink offers, rest by benches' offers, and the toilet by toilet blocks' offers. The Debug panel's Overlay choice shows one at a time, or none, and --overlay food, drink, rest, or toilet starts with that one shown. A capture of noon.park with the toilet overlay shows the paving in the ramp's best colors around the toilet block (slice criterion 6). A capture of noon.park with the drink overlay shows the paving in the ramp's best colors around the drink shop and its worst around the food shop (slice criterion 7). (capture: `--park tests/parks/noon.park --speed 0 --overlay toilet`, and the same with `--overlay drink`)
2. Every overlay discounts a facility's relief by one shared curve of the effective time, the walk at the guests' speed plus the offer's wait, in game minutes. The curve halves the relief for about every 20 game minutes and reaches 0 at about two hours. A value of 0 shows the ramp's worst color, the same as the ramp's bottom, so there is no separate color for nothing. Along any path the shading changes gradually from the best color to the worst and never ends abruptly, and it changes gradually from tick to tick. (capture: `--park tests/parks/noon.park --speed 0 --overlay food`; play: day.park at 4x with the food overlay, watching the paving far from the food shop)
3. Hovering shaded paving lists each facility that contributes to the shown overlay, with its relief, route distance, wait and effective time in game minutes, and term, and the value, which the terms add up to exactly (slice criterion 10). (play: noon.park with each overlay)
4. While a ghost is shown, the overlay shows the candidate park. Placing a toilet block with the toilet overlay shows the coverage it would add, and choosing Both for the drink shop with the food overlay shows the food coverage it would add, before either is committed. (play: noon.park)
5. With the Look tool, clicking a bench or a toilet block opens the Inspector on it. It shows its places, how many are taken, and how many guests wait at its door, follows every tick, and says when the facility is gone. (play: noon.park)
6. The guest Inspector's choice table has a fixed six rows: hunger, thirst, tiredness, the toilet, carrying on, and heading home. A need's row shows the guest's best offer of that need, its facility and its relief, distance, wait, commitment, and score, and the probability the guest picks any offer of that need, or that it has no offer. Heading home's base and reason terms stay below the table. The rows don't grow with the park. (play: full.park, clicking a guest)
7. A departures panel shows today's departures by reason, one fixed row for each of hunger, thirst, tiredness, the toilet, and closing, and their total. It counts from the park's last opening, as departureTally does. (play: day.park through closing at 4x; capture: `--park tests/parks/evening.park --ticks 3000 --speed 0`)
8. The Hours panel draws the arrival curve over the day, the guests per game hour each entrance admits, for the hours it shows, and marks the park's time of day on it. While the player's chosen hours differ from the park's, the curve is read from a candidate with only those hours applied, before Apply, whatever other edit the ghost previews. (play: day.park, moving Opens and Closes, then hovering a bench placement while the chosen hours still differ)
9. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after each feature shows no difference. src/legible/SPEC.md, src/views/SPEC.md, and src/app/SPEC.md describe the overlays, the curve, the inspectors, and the panels. The windows-debug build has no warnings, ctest passes, and tidy is clean.

## Medium

The milestone introduces no fields or flows, and the simulation uses nothing it produces. It reads only what the slice's medium map assigns legible-simulation:

- Need offers (plausible-operations): food-offer, drink-offer, rest-offer, and toilet-offer, each source's entry at its guest anchor, for the overlays and their hover (need-overlays).
- Route distance (navigable-networks): guest route distance from each source, for the overlays and their hover (need-overlays).
- Candidate resolution (deterministic-simulation, decision 0025): the previewed world, with the ghost's edit, the stock choice, or the chosen hours applied, which the overlays read (need-overlays). The Hours panel reads arrivalCurve from a candidate of its own with only the chosen hours applied, so it follows the chosen hours while another edit holds the preview (day-panels).
- Inspection records (believable-guests, plausible-operations, decision 0025): facilityRecord for benches and toilet blocks, and guestOptions for the guest's choice table (inspectors); departureTally and arrivalCurve (day-panels).
- Park intent (effortless-building): bench and toilet block boxes and their poses, to pick them (inspectors), and the opening hours the Hours panel shows (day-panels).
- The clock (deterministic-simulation): the park's time of day, which the Hours panel marks on the arrival curve (day-panels).

Each availability is computed for display and never enters the world: not its state, its hash, or its saves.

## Dependencies

- plausible-operations' served-needs: drink-offer, rest-offer, toilet-offer, and facilityRecord. Met.
- believable-guests' a-day-out: guests' four needs, guestOptions, departureTally, arrivalCurve, and day.park, no-toilet.park, noon.park, and evening.park. Met.
- legible-simulation's explained-food and wide-overlay: the availability library, the paving overlay, its hover, previews, and the Inspector. Met.
- effortless-building's facilities-and-hours: benches, toilet blocks, stock settings, and the Hours panel. Met.

## Core feature

need-overlays. It shows the slice's main new question, where each need is poorly served, and it delivers slice criteria 6, 7, and 10. Its new curve also fixes the food overlay's abrupt edge, which the user sees today.

## Features

1. `need-overlays`: the shared discount curve in game minutes that reaches 0 at about two hours, with 0 shown as the ramp's worst color, availability definitions for drink, rest, and the toilet, the Debug panel's Overlay choice, --overlay with each need, the hover tooltip for the shown overlay naming each facility, and the captures of slice criteria 6 and 7. Depends on: none.
2. `inspectors`: bench and toilet block inspectors, picked with the Look tool, and the guest Inspector's choice table cut to one row per need. Depends on: none.
3. `day-panels`: the departures panel, and the Hours panel's arrival curve, read from a candidate with the chosen hours and marked with the time of day. Depends on: none.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Switching the overlay to a ghost's need while the ghost is shown, as an option the player turns on. Cities: Skylines does it always, and players find that intrusive.
- A curve for each need, if guests ever weigh distance differently by need.
- Expanding a need's row in the guest's choice table to show every offer of that need.
- Departures by reason over past days, beside today's.
- Hovering the Hours panel's arrival curve to read the time and the rate under the cursor.
- A ghost's context for needs other than hunger, such as the guests near a toilet block ghost who need the toilet. Gated on: a footfall field for each need.

## Open questions

- The curve's values: the half time and where it reaches 0. need-overlays sets first values, and the user adjusts them by watching day.park and noon.park with each overlay.

## Research notes

- The availability library is already generic over a need, so each new overlay takes a definition, not new code.
- Real seconds at 1x equal game minutes, so the curve moves to the clock's units without changing the measure. The old curve reached 0 at four game hours, which covers day.park evenly.
- The white zero color made the shading end abruptly partway along a path, where the curve reached 0, and the edge flickered as waits changed. Showing 0 as the ramp's worst color removes the edge. A floor on the curve would have added up over many facilities.
- One shared curve matches guests, who weigh distance the same for every need.
- Cities: Skylines and Planet Coaster 2 show one coverage or need view at a time.
- No park game counts departures by cause. Here each guest records why it heads home, so the count is exact.

The details are in RESEARCH.md.
