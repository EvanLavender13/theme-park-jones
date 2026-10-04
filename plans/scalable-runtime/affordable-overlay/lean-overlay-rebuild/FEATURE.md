# Feature: Lean Overlay Rebuild

## Summary

Takes the allocation and copying out of the overlay rebuild's loops (docs/conventions.md, Memory in loops). The route and footfall edge rules write into a list the sampler reuses. sampleFieldAtPlaces hands each entry to its visitor directly instead of collecting per-place lists. availabilityValues adds each term straight into its place's value without building contribution lists. The band reuses its working lists across carriers and reserves its mesh. The overlay looks the same and gives the same values. It just costs less to rebuild at every tick.

## Acceptance criteria

1. The simulation is unchanged. Confirmed by `tpj_scenarios --compare` of windows-debug output before and after, which exits 0. Guests' choices and operations' offers sample fields through the changed code at every tick.
2. The food overlay looks exactly the same. Confirmed by windows-release captures of tests/parks/warm.park with `--overlay food` before and after, which differ in no pixel.
3. The full park's overlay build gets faster. Confirmed by `scripts/runtime-report.sh --compare` of the before and after reports: windows-release full.park's food-overlay stage reads `faster`, its vertex count is unchanged, and no ticks stage reads `slower`. The app's median frame on the full park with the overlay shown is no slower than its median with it off.
4. The hitch on the full park with the overlay shown gets smaller. Confirmed by the after report: the worst frame with the overlay shown is closer to the worst frame without it than before (31.5 ms against 13.3 ms).
5. The rebuild doesn't allocate in its loops. Confirmed by five Very Sleepy runs of the windows-release tpj_bench on the full park, as in RESEARCH.md: heap allocation and freeing are under 5% of the time under buildBandOverlay, down from 45%.
6. Hungry footfall, whose edge rule changes, still reads where guests walk. Confirmed by play, which needs the mouse, so the report asks Evan to do it. On tests/parks/warm.park with Place shop chosen, the tooltip's hungry footfall is above 0 beside the path guests walk to the shop.
7. windows-debug builds without warnings, `ctest.exe --preset windows-debug` passes, and scripts/tidy.sh is clean.

## Tests

None. The feature fixes no bug and adds no consequence across modules. The scenarios comparison covers the changed sampling at every tick.

## Medium

No field or flow changes what it carries. The medium's contract for an edge rule changes: the rule appends to a list it is given. So does sampleFieldAtPlaces's visitor, which now gets one entry at a time. Route distance (navigable-networks) and hungry footfall (following-footfall) are the two edge rules. legible's availabilityValues is the only caller of sampleFieldAtPlaces. Nothing enters a world's state, hash, or save.

## Spec changes

src/sim/medium/SPEC.md, in the paragraph beginning "sampleFieldAtPlaces gives a field's entries", the sentence "It hands each place's entries to a visitor, in the list's order." becomes "It hands the visitor one entry at a time, with its place's index. Each place's entries arrive in the order sampleField gives them, but along one edge the places' entries interleave, one source at a time. A place with no entries gets no call."

## Files affected

- Modify: src/sim/medium/SPEC.md, src/sim/medium/field.h, src/sim/medium/kept_field.h, src/sim/routes/route_distance.h, src/sim/routes/route_distance.cpp, src/sim/guests/footfall.h, src/sim/guests/footfall.cpp, src/legible/availability.cpp, src/render/band_overlay.cpp
- Modify: plans/scalable-runtime/affordable-overlay/MILESTONE.md (the frame target, see Spec decisions), docs/conventions.md (Memory in loops)

## Dependencies

- batched-field-samples: sampleFieldAtPlaces. Met.

## Out of scope

- The debug panel's summarizePark, which costs about 3 ms every frame on the full park, overlay or not. It isn't the overlay. It goes to scalable-runtime's candidates.
- Allocation in the tick path outside the overlay, such as sampleField returning a new list for each guest's choice. It changes the simulation's hot path and needs its own profile of ticks, so it is its own item in scalable-runtime's candidates.
- Hungry footfall's node rule, sampleNode, still returns a list. It runs only at places on nodes, which the overlay never samples with footfall.

## Spec decisions

- The milestone's 3 ms build and 1 ms frame targets were guesses. The target is the player's frame rate on the full park with the overlay shown matching it with the overlay off. MILESTONE.md says so.

## Open questions

None.
