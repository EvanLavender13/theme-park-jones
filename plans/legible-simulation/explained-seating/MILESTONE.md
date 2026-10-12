# Milestone: Explained Seating

Slice: somewhere-to-sit

Status: planned

## Summary

explained-seating lets the player see where seats serve guests and what a missing seat cost a guest. The Rest overlay moves from bench boxes to the furniture along the paths, read from the best rest seats field. A Meal seats overlay joins it, read from the best meal seats field. Each shades the paving by the SEAT_CHOICES seats that serve it best, each seat's relief discounted by its walk. Hovering lists those seats, each with the segment that lays it, and the furnishing ghost previews the coverage a setting would add. The guest Inspector says whether the last meal was eaten seated or standing, and how much relief it lost compared with a table. It is the somewhere-to-sit slice's last member. Every field and record it reads is in place, and once it lands the slice can retire the bench box and close.

## Acceptance criteria

1. Each seat overlay's value at a place is the sum of the SEAT_CHOICES largest terms among the entries of its field there. Rest reads best rest seats, and Meal seats reads best meal seats. A term is the entry's relief discounted by DISCOUNT_CURVE at its walk in game minutes, its route distance at REFERENCE_SPEED. Seats have no wait term, as a guest scores them without one. The value is exactly the sum over every piece in the park, because a piece that SEAT_CHOICES others dominate can't be among the largest terms (RESEARCH.md). Bench boxes no longer count toward the Rest overlay. (check: a scratch program compares the value at every overlay place of noon.park and full.park with the sum of the largest terms over every piece, each with its own route distance from a search that keeps every piece, and finds them equal allowing for rounding)
2. The Debug panel's Overlay row gains Meal seats after Toilet, and --overlay meal-seats starts with it shown. A capture of noon.park with it shows the paving shaded best around the tables beside the food shop and less well along the benches (slice criterion 5). (capture: `--park tests/parks/noon.park --speed 0 --overlay meal-seats`)
3. Each seat overlay is shaded from its values at its bends, linear between them, as the need overlays are. Along any path the shading changes gradually and never steps where a seat joins or leaves the field's entries. It changes only when the park's intent or the preview changes, never from tick to tick. (check: the scratch program samples between every two consecutive bends of noon.park's and full.park's seat overlays and compares each value with the line between them; capture as criterion 2)
4. Hovering shaded paving under a seat overlay lists each piece it counts: its kind and key, the segment that lays it, its relief, its route distance, its time in game minutes, and its term. It also shows the value, which the terms add up to exactly (principle 8). If no piece is reachable, the tooltip says so. (play: noon.park with each seat overlay)
5. While the Furnish tool's ghost is shown, a seat overlay shows the candidate park. Hovering noon.park's arm toward the drink shop with tables chosen shows, under Meal seats, the coverage the tables would add, before the player confirms. (play: noon.park)
6. The guest Inspector's Last meal line says whether the meal was eaten seated or standing, and the relief it lost compared with a table: a table's meal relief, capped at the hunger before the meal, minus what the meal relieved. From hunger 0.5 or more, a standing meal shows 0.25 lost, a bench meal 0.1, and a table meal none. (play: noon.park, clicking a guest who ate standing and one who ate at a table)
7. With the Meal seats overlay shown on full.park at 4x, the Debug panel's frame time stays about what it is with the Food overlay shown. (play: full.park, with both times reported)
8. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after each feature shows no difference. src/legible/SPEC.md, src/views/SPEC.md, and src/app/SPEC.md describe the seat overlays, their hover, and the meal line. The windows-debug build has no warnings, ctest passes, and tidy is clean.

## Medium

The milestone introduces no fields or flows, and the simulation uses nothing it produces. It reads only what the slice's medium map assigns legible-simulation:

- Best rest seats and best meal seats (resolved entry fields, from navigable-networks' best-seats): sampled at each overlay place and hovered place, through bestSeatsAt. Each entry gives the piece, its segment, its relief, and its route distance (seat-overlays).
- Candidate resolution (deterministic-simulation, decision 0025): the previewed world, with the Furnish tool's tentative SetFurnishing applied, which the seat overlays read as the need overlays do (seat-overlays).
- Seat places (effortless-building's furnished-paths): each piece's kind, by its key, to name it in the hover, as the Inspector already names pieces (seat-overlays).
- Guest records (believable-guests, decision 0025): the last meal's hunger before and after and whether it was eaten seated (meal-line).
- Seat offers (plausible-operations' served-seats): a table's meal relief, which the meal line compares with (meal-line).

The slice's medium map listed legible-simulation as a reader of best seats and the guest record only. This milestone adds it as a reader of seat places and seat offers.

Each seat availability is computed for display and never enters the world: not its state, its hash, or its saves. The rest-offer field stays for guests until the bench box is retired, but no overlay reads it.

## Dependencies

- believable-guests' seated-meals: guests who use furniture, the furnished day parks, and the guest record's seated meal. Complete.
- navigable-networks' best-seats: the best rest seats and best meal seats fields and bestSeatsAt. Complete.
- effortless-building's furnished-paths: furniture, the Furnish tool, and its tentative edit. Complete.
- plausible-operations' served-seats: seat offers by kind. Complete.
- legible-simulation's explained-needs: the overlay row, the shared curve, bends, hover, previews, and the Inspector. Complete.

## Core feature

seat-overlays. It answers the slice's main question for the player, where seats are missing, and it delivers slice criterion 5. It also carries the risk: a seat overlay's bends include the places where two seats' terms cross, which today's overlays don't have.

## Features

1. `seat-overlays`: the seat availability from each best seats field, its bends, the Rest overlay moved onto best rest seats, the Meal seats overlay with --overlay meal-seats, the hover naming each piece and its segment, the furnishing ghost's preview, and the capture of slice criterion 5. Depends on: none.
2. `meal-line`: the guest Inspector's Last meal line, seated or standing, with the relief lost compared with a table. Depends on: none.

## Tuning values

- Each seat overlay's Half, the value it shades halfway up its ramp: one serving's relief with no walk, as each need overlay's is. That is a bench's rest relief, 0.5, for Rest, and a table's meal relief, 0.5, for Meal seats. The user adjusts them by watching noon.park.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A furniture inspector: click a bench or table to see its seats, how many are taken, and the guests turned away from it. It replaces the bench box inspector once bench boxes are retired.
- The relief lost compared with the best seat the guest was offered when it chose to eat now. Gated on: the guest record keeping that seat's relief.
- How often each piece is found full over a day, shown on the furniture or the overlay. Gated on: a record of turn-aways by piece.

## Open questions

- The Halves' values. seat-overlays sets the values above, and the user adjusts them by watching noon.park with each seat overlay.

## Research notes

- A seat overlay can read only the best seats fields, because furniture has no route distance of its own. The sum of the SEAT_CHOICES largest terms is exact from the fields, continuous along the paving, and counts the seats that serve a place best.
- The shading bends at each seat place inside an edge and where two seats' terms cross, as well as where walks switch ends and times cross the curve's points.
- Planet Coaster 2's heatmaps show guests' state, not coverage. Cities: Skylines' coverage shading differs from how services reach buildings. Here the overlay reads the fields guests choose by.

The details are in RESEARCH.md.
