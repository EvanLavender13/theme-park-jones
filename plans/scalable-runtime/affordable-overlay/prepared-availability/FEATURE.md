# Feature: Prepared Availability

## Summary

The food overlay becomes the first availability overlay, the standard way to shade the ground by how well the sources reachable from each place serve a need. legible gains an availability defined by an AvailabilityDefinition: the offer its sources publish, read through a function giving a source's offer at a node, a discount curve, and Full, the value at which its overlay's color saturates. availabilityAt gives an availability at one place with its contributions, as foodAvailability does today, and availabilityValues gives its values at many places in one call, finding each source's offer once per call rather than once per place. Food availability is the availability FOOD_AVAILABILITY, and foodAvailability, foodNear, and the tooltip keep their values and types. render's food overlay becomes the band overlay: it collects every place it shades, asks for all their values in one call, and saturates its ramp at a Full it is given, so render no longer holds food's saturating value. views builds an availability's overlay from a definition, and the app and tpj_bench build food's with FOOD_AVAILABILITY. A later overlay of this type, such as thirst, is a definition. Before the overlay changes, scripts/runtime-report.sh also times the app's frames with the food overlay shown, and the feature's report compares before and after.

## Acceptance criteria

1. The runtime report times the app's frames with the food overlay shown on every stress park, beside its frames without it. Confirmed by a check on the before report: `grep -c '^park windows-release-app-overlay ' build/runtime-report/prepared-availability-before.txt` prints the same number as `ls tests/parks/stress/*.park | wc -l`.
2. The food overlay looks as it does today, and its tooltip explains the shade under it. Confirmed by capture and play. A `--capture` of tests/parks/warm.park with `--overlay food` after the feature shows the same bands in the same colors as one before, apart from where guests stand; two captures of one build differ there too, since the ticks stepped during the captured frames follow real time. By play, which the feature's report asks the user to do, since it needs the mouse: with warm.park open and the Food overlay box checked, hovering a guest path near the shop and then far from it shows the tooltip with the shop's terms, and its value is higher where the band is yellower.
3. An overlay of another need is a new definition, with no new availability, band, or overlay code: none of the code that computes an availability, draws the band, or joins them names food. Confirmed by a check: `grep -c -i food src/legible/availability.h src/legible/availability.cpp src/render/band_overlay.h src/render/band_overlay.cpp src/views/availability_overlay.h src/views/availability_overlay.cpp` prints 0 for every file.
4. The full park's food overlay builds faster, since each shop's offer is found once per build instead of at every place. Confirmed by a check: `scripts/runtime-report.sh --compare` of the feature's before and after reports marks windows-release full.park's food-overlay stage `faster`.
5. windows-debug builds without warnings, `ctest.exe --preset windows-debug` passes, and scripts/tidy.sh is clean. Confirmed by running them.

## Tests

None. The feature fixes no bug and makes no new consequence across modules; it changes how the overlay's values are computed, not what they are.

## Medium

The availability reads guest route distance, from navigable-networks, through sampleField at each place, and an offer field, from plausible-operations, through the definition's offer function at each source's lowest anchored node of the guest network; for food that is sourceEntryAtNode of FoodOffer, as foodAvailability reads it today. It emits nothing. No field or flow changes, and nothing enters a world's state, hash, or save, so the save, copy, and hash checks are untouched, and tpj_scenarios, which links the sim alone, is unaffected. The overlay is built afresh from the world it shows and keeps nothing between builds: availabilityValues holds the offers it found only for the length of a call (principle 1). Principle 8's attribution holds because the tooltip and the overlay compute every place's value through the same code, which the reviewer checks against legible/SPEC.md.

## Spec changes

src/legible/SPEC.md, in Boundary, replacing "views calls foodAvailability." with "views calls availability.h for availabilityValues, and the app and tpj_bench read FOOD_AVAILABILITY from food.h."

src/legible/SPEC.md, replacing the section "## Food availability" whole with:

"## Availability

An availability at a place is how well the sources a guest standing there can reach serve one need: the sum, over the supplied sources it can reach, of each source's relief discounted by the time it would take to be served there, walking and waiting (decision 0020). An AvailabilityDefinition in availability.h says which need: the offer its sources publish, the curve that discounts a relief by time, and Full, the value at which an overlay of it is fully saturated. So an availability of another need, such as thirst, is a definition, with no new code.

availabilityAt(world, definition, place) gives the value and each source's contribution, with its relief, route distance, wait, effective time, and term. The terms add up to the value exactly, so the value is attributed to its sources (principle 8). Sources' offers are found as guests find them.

availabilityValues(world, definition, places) gives the value at each of many places in one call, each exactly the value availabilityAt gives there, so a shade and the tooltip over it agree. It finds each source's offer once per call, however many of the places reach the source, and keeps nothing after it returns.

Distance is measured along the paths, so a source across a fence is as far as its route (principle 4). Every reachable supplied source contributes, however far, and its term only falls as its effective time grows (principle 5). A place with no reachable supplied source, or that is not on the guest network, has the value 0.

Food availability is FOOD_AVAILABILITY, in food.h: shops' food offers, and a Full that is a meal's relief with no walk and no wait, so a color means the same availability in every park. foodAvailability(world, place) gives it at a place."

src/render/SPEC.md, in Boundary, replacing "The food overlay shades by a value function it is given, which views supplies." with "The band overlay shades by values it is given, which views supplies.", and "food_overlay.h for OVERLAY_BAND" with "band_overlay.h for OVERLAY_BAND".

src/render/SPEC.md, replacing the section "## Food overlay" whole with:

"## Band overlay

buildBandOverlay(world, values, full) draws a band along each guest path, shaded at each place by the value values gives for it, from gray for none through a perceptually uniform, colorblind-safe ramp from dark purple to yellow, which reaches yellow at full. It calls values once per build, with every place it shades, so whatever computes the values sees the whole build at once. A color means the same value in every park for the same full, and any positive value is distinguishable from none. The band covers the ground within OVERLAY_BAND of each guest path, lies over the terrain and under every path, and rises toward its own path, so where bands overlap the nearest path's value shows. Connectors and backstage paths have no band."

src/views/SPEC.md, in Boundary, replacing "The app and tpj_bench call food_overlay.h." with "The app and tpj_bench call availability_overlay.h.", and replacing the section "## Food overlay" whole with:

"## Availability overlays

buildAvailabilityOverlay(world, definition) gives render's band overlay of the world shaded by the definition's availability at each place, saturating at its Full. The app builds its food overlay with FOOD_AVAILABILITY, and tpj_bench times the same call, so what is timed is what is shown. An overlay of another need is the same call with its definition."

src/bench/SPEC.md, The script, replacing "and the windows-release app on every park." with "and the windows-release app on every park, without and then with the food overlay shown, so the report shows what rebuilding the overlay at every tick costs a player who shows it."

## Files affected

- Create: src/legible/availability.h, src/legible/availability.cpp, src/render/band_overlay.h, src/render/band_overlay.cpp, src/views/availability_overlay.h, src/views/availability_overlay.cpp
- Delete: src/render/food_overlay.h, src/render/food_overlay.cpp, src/views/food_overlay.h, src/views/food_overlay.cpp
- Modify: src/legible/food.h, src/legible/food.cpp, src/legible/CMakeLists.txt, src/render/CMakeLists.txt, src/views/CMakeLists.txt, src/app/scene/scene_uploads.cpp, src/app/ui/food_tooltip.cpp, src/bench/stages.cpp, scripts/runtime-report.sh
- Modify (specs): src/legible/SPEC.md, src/render/SPEC.md, src/views/SPEC.md, src/bench/SPEC.md

## Dependencies

- indexed-sampling and following-footfall: the report with app frames and its comparison, and today's food overlay. Met.
- sourceEntryAtNode and OfferEntry: met.

## Out of scope

- Sampling route distance at many places at once: feature 2, batched-field-samples.
- A menu of overlays and a second availability: they wait for a second need (the milestone).
- Keeping the overlay between builds: a capability deepening candidate.

## Spec decisions

- An availability's definition is a value with a function pointer for its offer, not a template parameter (RESEARCH.md).
- Contributions keep the member name Shop, since every offer field is published by shops.
- availabilityValues keeps the offers it found only for one call (RESEARCH.md), so nothing is kept between builds.
- The discount curve's evaluation and the ramp's color are private to legible's and render's sources: no other module calls them, so foodDiscount and foodColor leave the public headers rather than becoming general.

## Open questions

None.
