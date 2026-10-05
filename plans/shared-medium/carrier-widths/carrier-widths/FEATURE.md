# Feature: Carrier Widths

## Summary

Every carrier of a network gets a walkable width, the same along its whole length. The network answers two new questions about a place: the walkable width there, and the direction of travel there, toward higher distances along its carrier. routes gives each path's and each connector's carrier its kind's current fixed path width, 3 m for guest paths and 2 m for backstage paths. The synthetic scenario that builds its own track network gives the track a width too. Nothing reads the width yet, so the simulation is unchanged. Later members of the good-paths slice read it to draw guests across a path and to size the food overlay.

## Acceptance criteria

1. Each carrier of each network routes derives has a walkable width, its kind's path width. widthAt answers at every place that resolves, and directionAt at every such place whose polyline piece has length on the ground. Both give nothing elsewhere. Confirmed when believable-guests' spread-guests and legible-simulation's wide-overlay read them (carrier-widths milestone, criterion 2).
2. Copies, saves, and loads keep each network's widths. Confirmed by `ctest.exe --preset windows-debug` passing, which runs the park world checks over every park in tests/parks.
3. The width changes nothing in the simulation. Confirmed by regenerating the slice parks and the full park on windows-debug and comparing them with the checked-in files. Both comparisons print the "same" lines in Task 7. State hashes change, because a network's width is part of the derived state they cover, so `tpj_scenarios --compare` can't show this.

## Tests

None. docs/testing.md allows a feature to add only a bug reproduction or an integration test of a consequence across modules, and nothing reads the width yet. Once a network holds a width, the park world checks cover its copies and loads.

## Medium

The feature adds no fields or flows. It changes the network type that every field and flow lives on. routes produces networks and now gives every carrier a width. No module consumes the width or direction in this feature.

## Spec changes

src/sim/medium/SPEC.md, in Networks, after "It is given as a polyline of ground points with the producer's own arc lengths.":

    Each carrier also has a walkable width, the same along its whole length.

src/sim/medium/SPEC.md, in Places, a new paragraph after the groundPoint paragraph:

    widthAt gives the walkable width at a place, and directionAt gives the direction of travel there, as a unit vector on the ground pointing toward higher distances along its carrier. At a point where the carrier's polyline bends, the direction is that of the piece ahead. At the carrier's end, it is that of the last piece. Where a place doesn't resolve, both give nothing. If the piece has no length on the ground, directionAt gives nothing.

and replace the last two sentences of the groundPoint paragraph, "These are the medium's only straight-line measures. They are used only to snap a ground position onto a network (principle 4).", with:

    nearestPlace, nearestPlaceOn, and directionAt are the medium's only straight-line measures. The first two are used only to snap a ground position onto a network, and directionAt only to scale a direction to unit length. None of them measures how far anything travels (principle 4).

and add this bullet to the list of what makes a network well formed, after "Each carrier's distances start at 0 and strictly increase.":

    - Each carrier's width is finite and above zero.

src/sim/routes/SPEC.md, in Networks, after "It also has a carrier for each connector.":

    Every carrier, a path's or a connector's, has its kind's path width (pathWidth, sim/park/SPEC.md).

## Files affected

- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/medium/network.h
- Modify: src/sim/medium/network.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.cpp
- Modify: src/scenarios/stalls.cpp

## Dependencies

None.

## Out of scope

- Width that varies along a carrier. It is a deepening candidate in the carrier-widths milestone.
- Any reader of the width or direction. navigable-networks, legible-simulation, and believable-guests add them later in the slice.
- Each segment's own width. navigable-networks' wide-routes gives it, once effortless-building's path-network adds segments.

## Open questions

None.
