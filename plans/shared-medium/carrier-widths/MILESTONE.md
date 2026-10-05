# Milestone: Carrier Widths

Slice: good-paths

Status: complete

## Summary

carrier-widths makes two changes to the network type that the good-paths slice needs before paths change. First, every carrier gets a walkable width, and the network answers the width and the direction of travel at any place. Guests are drawn across the width and the overlay is sized by it later in the slice. Second, carryOver keeps a place at its ground position when that ground survives an edit under a different carrier key. When path-network splits a segment where another path crosses it, guests on the split-off half then keep walking from where they stand, instead of jumping to the split point. It comes first in the slice because path-network's splits need the new carry-over the moment they land, and navigable-networks, legible-simulation, and believable-guests all read the width. Until navigable-networks' wide-routes, routes gives every carrier its kind's current fixed path width, so nothing the player sees changes.

## Acceptance criteria

1. Every carrier of every network routes derives has a walkable width. For now, each path's and each connector's carrier has its kind's fixed path width. The park world checks pass with the width in every network, so copies keep it and loads derive it again. (ctest)
2. The network gives the walkable width and the direction of travel at every place that resolves, and none at a place that doesn't. (believable-guests' spread-guests and legible-simulation's wide-overlay use both, and the slice's captures show them)
3. If an edit leaves ground where it was under a different carrier key, a place on that ground keeps its ground position. A guest there stays where it stood and keeps walking the same way over the ground, even if the new carrier runs the other way. If the ground is gone, carryOver behaves as it does today. (the good-paths slice's integration test for a split under walking guests, once path-network splits segments)
4. The width changes nothing in the simulation. After carrier-widths, `tpj_scenarios --slice-parks fed warm cut` and `tpj_bench --full-park` on windows-debug write files identical to the ones they wrote before it. State hashes change, because a network's width is part of the derived state they cover, so `tpj_scenarios --compare` can't show this. Saves hold no derived data, so identical saves after thousands of ticks show that guests, operations, and footfall behave as before. ground-carry-over deliberately changes one rare outcome today. If a guest stands exactly where a connector meets a path and the box moves, the guest stays on the ground it stood on, instead of moving along the changed connector. (comparison)

## Medium

The milestone adds no fields or flows. It changes the network type that every field and flow lives on, and the carry-over that holders apply to their places.

- Network width and direction: carrier-widths adds them to the network type. routes is the producer. Today it gives the kind's fixed width, and navigable-networks' wide-routes later gives each segment's own width. Consumers later in the slice are believable-guests, which draws guests across the width, and legible-simulation, which sizes the overlay.
- Carry-over: ground-carry-over changes carryOver. Its holders are believable-guests' guests and hungry footfall, which already call it. A guest's walking direction is relative to its carrier, so ground-carry-over also changes the guests' carry step to keep each guest walking the same way over the ground, using the direction query. Footfall needs no change. It already regroups carried entries by the stretch they land in.

## Dependencies

None. This is the slice's first member.

## Core feature

`carrier-widths` is the core. It changes the network type that three later members read, and every caller that builds a network must supply the width in the same change. That makes it the larger of the two features and the one the rest of the slice builds on.

## Features

1. `carrier-widths`: each carrier holds a walkable width that the network refuses unless it is finite and above zero. The network answers the width and the direction of travel at a place. routes gives every carrier its kind's fixed path width, and the synthetic scenario that builds its own track network (src/scenarios/stalls.cpp) gives its track a width. Depends on: none.
2. `ground-carry-over`: if a place's carrier changed or is gone, and its old ground position lies on a carrier of the new network, carryOver keeps the place at that ground position. It prefers the same carrier when that carrier still passes there. Otherwise carryOver behaves as it does today. When a guest is carried onto a carrier, it keeps walking the same way over the ground, judged by the direction of travel before and after. The feature rewords the carry-over rules in src/sim/medium/SPEC.md and src/sim/guests/SPEC.md, and in this capability's brief. Depends on: feature 1, for the direction query.

Neither feature adds a test (docs/testing.md). The slice adds the integration test that covers ground-carry-over once segments can split.

## Deepening candidates

- Width that varies along a carrier, such as a path that flares. Gated on: a path model with width that varies along a segment, which good-paths leaves out.

## Open questions

None.

## Research notes

- Detour's crowd re-finds an agent's position by its ground point when the polygon under it becomes invalid after a navmesh rebuild. ground-carry-over does the same for places whose carrier changed.

Depth is in RESEARCH.md.
