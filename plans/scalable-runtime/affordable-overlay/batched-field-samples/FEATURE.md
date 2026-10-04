# Feature: Batched Field Samples

## Summary

The shared medium samples a field at a list of places in one call, sampleFieldAtPlaces, giving each place exactly the entries sampleField gives there. Consecutive places strictly inside one edge share each source's lookups of its entries at the edge's ends and along it, so sampling a line's places costs lookups per edge rather than per place, and only the field's own rule runs at each place. A single place's sample is the batch of one place, so sampleField, sampleResolvedField, and kept fields' samples all go through the same code, and every field, resolved, stepped, or kept, with or without its own rules, gets the batch. The availability overlay reads guest route distance through it, so the full park's food overlay, which the app rebuilds at every tick while it is shown, gets cheap enough that showing it no longer costs the frame rate.

## Acceptance criteria

1. The simulation is unchanged. Confirmed by a scenarios comparison: `tpj_scenarios --compare` of windows-debug's `tpj_scenarios tests/parks/*.park` output before and after exits with status 0. Every guest choice and every offer the operations publish samples a field through the changed code, so a difference in resolved or stepped fields would show. No simulation code samples a kept field, so criterion 5 covers those.
2. The food overlay looks exactly as it does today. Confirmed by capture: windows-release captures of tests/parks/warm.park with `--overlay food` before and after differ in no pixel.
3. The full park's food overlay builds faster, and over the whole milestone falls below the 33 ms budget. Confirmed by checks: `scripts/runtime-report.sh --compare` of the feature's before and after reports marks windows-release full.park's food-overlay stage `faster` and no ticks stage `slower`; and the compare of the milestone's starting report, build/runtime-report/prepared-availability-before.txt, with this feature's after report marks it `faster`.
4. On the full park, the app's frames with the food overlay shown get faster. Confirmed by a check: the compare in criterion 3 marks windows-release-app-overlay full.park's frames stage `faster`. The milestone's frame target is lean-overlay-rebuild's.
5. Hungry footfall, the one kept field, still reads where guests walk. Confirmed by play, which the feature's report asks the user to do, since it needs the mouse: on tests/parks/warm.park with Place shop chosen, the tooltip's hungry footfall is above 0 beside the path guests walk to the shop, and 0 beside a guest path no guest has walked.
6. windows-debug builds without warnings, `ctest.exe --preset windows-debug` passes, and scripts/tidy.sh is clean. Confirmed by running them.

## Tests

None. The feature fixes no bug and makes no new consequence across modules. The scenarios comparison exercises resolved and stepped fields' sampling at every tick through the code this feature changes; the kept field's is checked by play (criterion 5).

## Medium

The feature adds a query to the shared medium, a field sampled at a list of places, and changes no field or flow, no entry, and no state, so nothing enters a world's state, hash, or save differently and the save, copy, and hash checks are untouched. Its first reader is legible's availabilityValues, which reads guest route distance, from navigable-networks, through it. Every other reader of fields keeps calling sampleField, sampleResolvedField, or fieldValue, which now sample one place through the same code: the guests' choices (guest route distance), the operations' offers (shop offers, resolved and stepped), the scenarios (stall offers), and legible's previews (hungry footfall, a kept field). The batch holds what it gathers only for the length of a call (principle 1).

## Spec changes

src/sim/medium/SPEC.md, in Boundary, nothing changes: sampleFieldAtPlaces is in sim/medium/field.h, which the boundary already names.

src/sim/medium/SPEC.md, in Fields, after the paragraph beginning "sampleField gives a field's entries at a place", adding:

"sampleFieldAtPlaces gives a field's entries at each of a list of places in one call, exactly the entries sampleField gives at each, handing each place's entries to a visitor in the list's order. It holds for every field, resolved, stepped, or kept, with or without its own rules, and keeps nothing after it returns. For a field with its own rule for places inside an edge, consecutive places strictly inside one edge share each source's lookups of its entries at the edge's ends and along it, so sampling the places along a line costs lookups for each edge it crosses, and only the rule runs at each place."

## Files affected

- Modify: src/sim/medium/field.h, src/sim/medium/kept_field.h, src/legible/availability.cpp, src/sim/medium/SPEC.md

## Dependencies

- prepared-availability: availabilityValues, which this feature makes read route distance through the batch. Met.
- following-footfall: hungry footfall as a kept field, so the batch covers kept fields. Met.

## Out of scope

- Keeping the overlay between builds: the capability's candidate, gated on this feature's report.
- Changing sampleEdge to fill a list it is given rather than return one, which would save an allocation per source and place: lean-overlay-rebuild weighs it with the profile after this feature.
- Batching samples at nodes: places at nodes are few along a line, and each reads only its own node's entries.

## Spec decisions

- The batch shares lookups among consecutive places on one edge, not among all places on it wherever they come in the list (RESEARCH.md). The band and any line-following overlay give places in order along each line.
- A single place's sample is the batch of one place, so there is one sampling code path for every caller (RESEARCH.md).

## Open questions

None.
