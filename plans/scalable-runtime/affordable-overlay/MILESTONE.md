# Milestone: Affordable Overlay

Slice: none

## Summary

affordable-overlay makes the food overlay cheap enough to rebuild at every tick on the full park, and makes the way it does so the standard way to build any overlay of its type: one that shades each place by how well the sources reachable from it serve a need, each source's relief discounted by the time it takes to reach and be served. On windows-release the full park's overlay build costs 37.4 ms, over the 33 ms a frame has, and the app rebuilds it at every tick while it is shown. A profile puts about 53% of a build in sampling guest route distance at each of about 7,000 places, and about 37% in finding every shop's offer again at each of them, though only the offsets along an edge differ between the places on it and an offer does not vary by place. The milestone has an availability overlay find each source's offer once per build and ask for its values over many places at once, and gives the shared medium a sample of a field at many places that shares each source's lookups among the places on one edge. Food is the first availability overlay; a later one, such as thirst, supplies its offer field, its discount curve, and its ramp's scale, and inherits both speedups without new sampling or band code. The report also gains the app's frames with the overlay shown, since a build at every tick is what the player feels.

## Acceptance criteria

- The runtime report times the app's frames on every stress park with the food overlay shown, beside its frames without it, with the median and worst frame, so the cost of rebuilding the overlay at every tick appears where the player feels it.
- An availability overlay is defined by an offer field whose entries give a relief, a wait, and whether the source is supplied, a discount curve, and the value at which its ramp saturates. Its value at a place is the sum, over the sources of guest route distance reachable from the place whose offers are supplied, of each relief discounted by the effective time, in ascending source order, as legible/SPEC.md states food availability today. Food availability is that overlay for the food offer, FOOD_DISCOUNT_CURVE, and a saturating value of 0.5, which moves from render's OVERLAY_FULL into legible's food definition, since render and legible share a layer and neither may include the other (decision 0027); render's ramp takes the saturating value it is given. foodAvailability, foodNear, and the tooltip are unchanged, and their tests pass unchanged. The band gives every place the color it gives today. Render's band and ramp tests, which call the value-per-place builder and the one-argument ramp this replaces, are rewritten by the test pass from the new spec.
- An availability overlay's build finds each source's offer once, not once per sampled place, and gives its values for all the band's places in one call.
- The shared medium samples a field at a list of places, giving at each exactly the entries sampleField gives there, for every field: resolved, stepped, and kept, with or without an edge rule. Places on one edge share each source's lookups of its entries on the edge and at its two nodes, so a sample's lookups grow with the edges its places lie on rather than with the places. src/sim/medium/SPEC.md states it.
- The availability overlay's values at its places come from that sample of guest route distance, and equal, bit for bit, the value foodAvailability gives at each place, so a tooltip's terms still sum to the shade under the cursor (principle 8).
- The milestone's report compares the runtime report before and after on every stress park. The full park's food overlay build median on windows-release falls below 33 ms, with the change marked clear, and the app's frame median on the full park with the overlay shown is below 33 ms, so a player with the overlay on keeps the frame rate they have with it off.
- Nothing kept: the overlay is still built afresh from the world it shows, holds nothing between builds, and enters no world's state, hash, or save (principle 1). tpj_scenarios's windows-debug output is identical before and after by tpj_scenarios --compare (decision 0030).
- No test, hook, or check fails on a timing. windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean. Linux is checked at release, not by the milestone (decision 0030).

## Medium

The milestone changes how the overlay reads the medium, not what any field or flow carries. Guest route distance, from navigable-networks, and the food offer, from plausible-operations, are read as today. The shared medium adds one query, a field sampled at a list of places, which batched-field-samples supplies and the availability overlay is its first reader. legible's availability overlay reads route distance and an offer field through it and through sourceEntryAtNode, views joins it to render's band, and the app shows it as before. The report reads parks and the app only through tpj_bench's public headers and the app's command line.

## Dependencies

- measured-runtime and indexed-sampling: the stress parks, tpj_bench, the report with its budgets and app frames, and the comparison. Met.
- following-footfall: hungry footfall as a kept field, so the medium's batched sample covers kept fields from the start. Met.
- A report showing the overlay over budget after indexed-sampling: met, 37.4 ms on windows-release on the full park in the report after following-footfall.

## Core feature

`prepared-availability`, because it sets the standard a later overlay of this type follows, and removes the larger of the two repeated lookups it can remove alone: finding every shop's offer at every sample. Before changing the overlay, it adds the app's frames with the overlay shown to the report and takes the before report with them, so both speedups are measured where the player feels them.

## Features

1. `prepared-availability`: the runtime report launches the app on every stress park a second time with --overlay food and reports those frames beside the frames without it; legible's availability overlay, defined by an offer field, a discount curve, and a ramp's saturating value, with food availability as its first instance; a build finds each source's offer once and gives its values for all the band's places in one call; render's band takes those values and a ramp scale rather than a value per place; and the report is compared before and after. Depends on: none.
2. `batched-field-samples`: the shared medium's sample of a field at a list of places, sharing each source's lookups among the places on one edge, and the availability overlay's route distance read through it, with the report compared before and after. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

None.

## Open questions

None.

## Research notes

- A Very Sleepy profile of the full park's overlay build puts about 53% in route distance samples, mostly finding each source's entries at the sampled edge's nodes, about 37% in finding each shop's offer again at every sample, and about 1% in the band's geometry.
- Keeping the overlay between builds and rebuilding only when its inputs change was set aside: offers are published at every tick and change with queues, so on a busy park some input changes most ticks. It is a capability deepening candidate, gated on the report.
- The band's geometry is what paths-and-plazas' ground-overlay replaces, so the standard is the value side, which any geometry asks for its places.
- A menu of overlays waits for a second overlay to settle its shape.

Depth is in RESEARCH.md.
