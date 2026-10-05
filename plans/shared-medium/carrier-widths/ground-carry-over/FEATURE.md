# Feature: Ground Carry-Over

## Summary

When an edit re-derives a network, carryOver keeps a place on the ground it stood on whenever a carrier of the new network still passes over that ground, even if the carrier there has a new key. It prefers the place's own carrier when that carrier still passes there. Otherwise carryOver behaves as it does today: a place on a reshaped carrier moves to the nearest point of that carrier, and a place whose carrier and ground are both gone is retired. If carrying puts a guest on another carrier that runs the other way over that ground, the guest turns its walking direction, so it keeps walking the same way over the ground. Today's edits never re-key a carrier under a place, so the game plays as before. path-network's splits will re-key segments under walking guests, and this feature keeps those guests where they stand.

## Acceptance criteria

1. If an edit changes or removes a place's carrier, and a carrier of the new network passes over the place's old ground position, allowing only for rounding, carryOver keeps the place at that ground position. It stays on its own carrier if that carrier passes there, and otherwise goes on the carrier nearest it. If no carrier passes there, carryOver behaves as it does today. Confirmed by the good-paths slice's integration test for a split under walking guests (SLICE.md, criterion 1), once effortless-building's path-network splits segments.
2. If carryOver puts a guest on another carrier that runs against its old carrier at that ground, the guest's walking direction flips, so it keeps walking the same way over the ground. Confirmed by play once path-network merges segments. Draw two segments in a straight line toward the point they share, so they run opposite ways. Wait for guests to wander along both, then delete the shared point to merge them. No guest turns around. path-network's milestone carries this check, because SLICE.md criterion 8 covers jumps but not turning around.
3. The simulation is unchanged by today's edits. Confirmed by `tpj_scenarios --compare` on windows-debug output from before and after, run for 3000 ticks over every scenario and every park in tests/parks. The park-edits scenario makes about 300 random edits under arriving guests. It prints no difference, and the command in Task 7 prints `same`. The feature changes two outcomes on purpose. Both need a guest standing exactly on a node when the edit lands, which these runs are not expected to reach:

- If a box moves, a guest standing exactly where its connector met a path stays on the path, instead of moving along the moved connector.
- If a path is deleted, a guest standing exactly where it met another path or connector goes onto that one, as today, but now keeps walking the same way over the ground.

If the comparison shows a difference, stop and report it.

## Tests

None. docs/testing.md allows a feature to add only a bug reproduction or an integration test of a consequence across modules. Today's edits can't re-key a carrier under a place. The good-paths slice adds the integration test once path-network splits segments.

## Medium

The feature adds no fields or flows. It changes carryOver, which every holder of places applies across a re-derivation.

- carryOver (shared-medium): its holders are believable-guests' guests and hungry footfall. Both already call it in their finishers. Footfall needs no code change, because it already regroups carried entries by the edge they land in.
- Guests read directionAt from the networks before and after the re-derivation, through the medium's network type, to keep each carried guest walking the same way over the ground. Guests change only their own state.

## Spec changes

src/sim/medium/SPEC.md, in Carry-over, replace the second paragraph's sentences from "If the carrier changed shape" up to "which is a legitimate state its holder handles (principle 2)." with:

    Otherwise, if a carrier of the new network passes over the place's old ground position, allowing only for rounding, the place stays at that ground position. It stays on its own carrier if that carrier passes there, and otherwise goes on the nearest carrier that does. So if an edit splits a carrier and gives a part a new key, places on that part stay where they stood. If no carrier passes there but the place's carrier still exists with a new shape, the place moves to the nearest place on that carrier. If the place did not resolve before, or its carrier is gone and no carrier passes over its ground, the place is retired, which is a legitimate state its holder handles (principle 2).

src/sim/medium/SPEC.md, in Places, replace "nearestPlace, nearestPlaceOn, and directionAt are the medium's only straight-line measures. The first two are used only to snap a ground position onto a network," with:

    nearestPlace, nearestPlaceOn, and directionAt, and carryOver's check that a snapped place lies on a place's old ground, are the medium's only straight-line measures. The first two and that check are used only to snap a ground position onto a network,

src/sim/guests/SPEC.md, in Carrying across an edit, replace the first two sentences, "When the player edits the park, each guest keeps its ground position if its path survives the edit. If a guest's path or connector is deleted, the guest moves to the nearest point of the guest network.", and the third, "Carrying changes only where guests stand.", with:

    When the player edits the park, each guest keeps its ground position if a path or connector still runs over it, even one with a new key, as when an edit splits a path. The guest stays on its own path or connector if that one still runs over its ground. If the guest is carried onto another one that runs the other way there, it turns, so it keeps walking the same way over the ground. If nothing runs over where the guest stood, but its own path or connector is still there with a new shape, the guest moves to the nearest point of it. If its path or connector is deleted, the guest moves to the nearest point of the guest network. Carrying changes only where guests stand and which way they walk.

src/sim/guests/SPEC.md, in Hungry footfall, replace the bullets "if a stretch is split, its value stays with the half holding its middle;" and "if a path is deleted, its footfall is dropped." with:

    - if a stretch is split, its value stays with the part holding its middle, even if that part has a new key;
    - if a path is deleted, its footfall is dropped, except where another path runs over a stretch's middle.

plans/shared-medium/CAPABILITY.md is the capability's brief, not a spec, but its Medium section states the carry-over rules. Its update is in Task 3 of PLAN.md.

## Files affected

- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/medium/network.h
- Modify: src/sim/medium/network.cpp
- Modify: src/sim/guests/SPEC.md
- Modify: src/sim/guests/guests.cpp
- Modify: plans/shared-medium/CAPABILITY.md

## Dependencies

- carrier-widths (feature 1 of this milestone), for directionAt. Merged.

## Out of scope

- Keeping a guest's way over the ground when its ground is gone and it jumps to the nearest point of the network. It has no way over the ground to keep, so it keeps today's behavior.
- Keeping a place on a carrier that passes over its ground only within the carrier's width. A place on a deleted path would land on any neighbor within half a width (RESEARCH.md).
- Any reader that splits or re-keys carriers. effortless-building's path-network and navigable-networks' wide-routes add them.

## Open questions

None.
