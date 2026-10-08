# Feature: Own Pace and Side

## Summary

Each guest walks at its own speed and keeps to its own side of the paving for its whole stay. Both are drawn on the guest's key when it arrives, so faster guests overtake slower ones. A guest's speed is drawn from 1.0 to 1.6 m/s, so guests walk at WALK_SPEED, 1.3 m/s, on average. Its side is a place across the width its box can use, seen the way it walks. Every side lies right of SIDE_BIAS, and the share of the rest is the mean of two uniform draws, so guests thin out toward the edges. Each point of a guest's route is moved to its side before the guest looks for its aim. On the 10 m main street, guests walking each way stream along opposite sides. On 3 m paths they walk close together. Where the guest's box wouldn't fit at its side of a route point, as at a door, at a path's free end, or around the outside of a junction, the point stays in the middle. A guest heading for a door still walks to the door itself.

## Acceptance criteria

1. Each guest walks at its own speed for its whole stay, drawn on its key when it arrives, uniformly from 1.0 to 1.6 m/s. Faster guests overtake slower ones. On main-street.park, guests walking the same way pass one another. (play)
2. Each guest keeps to its own side of its route, to the right of the way it walks, with sides thinning toward the edges. On main-street.park's 10 m street, guests walking each way stream along opposite sides, and on its 3 m side paths they walk close together. The lanes check reports that more than two thirds of walking guests clear of the middle of the 10 m street are right of it, with a mean offset between 1 and 3 m to the right, where the unchanged tree's mean offset is near 0. (play, and the lanes check)
3. Where a guest's box wouldn't fit at its side of a route point, that point stays in the middle, so no guest stops short of a point it can't reach. A guest heading for a door walks to the door itself and visits. The lanes check reports 0 stalled guests in every park, and paving-walk's door check reports meals and no guest stuck heading to the shop. (the lanes check and the door check)
4. Guests still never leave the paving. paving-walk's stays check reports 0 guests off the paving in every park in tests/parks and parks/. (the stays check)
5. Guests save their speed and share. warm.park, cut.park, and stress/full.park are made again, and the user's parks that hold guests are carried into the new save. The park world checks, food_loop_test, and the rest of ctest pass. (ctest)
6. The runtime report on full.park shows the tick still inside the 33 ms budget. The work of moving a route to a guest's side depends on the guest's route ahead, not on the size of the park (decision 0031). (scripts/runtime-report.sh)

## Tests

None. Speeds and sides are gameplay, checked by playing and by the scratch checks (docs/testing.md). The saved speed and share are covered by the park world checks once warm.park holds them.

## Medium

- The guest network (navigable-networks, through shared-medium's network type): guests now read the width of each carrier along their route ahead, the width widthAt gives.
- Keyed draws (deterministic-simulation): each guest's speed and share are drawn on its key in addGuest, as its hunger rate is.
- Route distance, food offers, guest visits, meals, and hungry footfall: unchanged. The guest's place stays on the network, so everything reads and feeds it as before (decision 0038).
- Food availability's REFERENCE_SPEED stays WALK_SPEED, which is still the mean speed guests walk at.
- Guest state gains speed and share. warm.park holds guests with both, so the park world checks cover them.

There are no new fields or flows.

## Spec changes

src/sim/guests/SPEC.md:

- In Arriving and leaving, replace "A new guest's hunger and hunger rate are drawn, so guests differ from one another." with:

    A new guest's hunger, hunger rate, speed, and share of the width are drawn, so guests differ from one another.

- In Walking, replace "A guest walks straight toward its aim at a fixed speed. Its aim is the farthest point of its route, looking at most AIM_LOOKAHEAD along it, that a straight line reaches with the guest's box on the paving." with:

    A guest walks straight toward its aim at its own speed. Its aim is the farthest point of its route, moved to the guest's side and looking at most AIM_LOOKAHEAD along it, that a straight line reaches with the guest's box on the paving.

- In Walking, add a paragraph after the one that begins "A guest walks straight toward its aim":

    Each guest walks at its own speed and keeps to its own side of its route for its whole stay, so faster guests overtake slower ones. Speeds spread evenly by WALK_SPEED_SPREAD either side of WALK_SPEED, so guests walk at WALK_SPEED on average. A guest's side is a place across the width its box can use, measured from the left of the way it walks. Sides lie from SIDE_BIAS of the way across up to the right edge, and fewer guests keep near either end of that range than near its middle. On a wide path, guests walking each way stream along opposite sides, and on a narrow one they walk close together. Each point of a guest's route is moved to its side. If the guest's box wouldn't fit at its side of a point, as at a door, at a path's free end, or around the outside of a junction, that point stays in the middle. So a guest heading for a door walks to the door itself.

- In Walking, replace "A guest's place on the network is the point of its route, between its place and its aim, nearest where it stands." with (added in play, see PLAN.md's Task 11 deviation from play):

    A guest's place on the network is the point of its route, between its place and its aim, whose position on the guest's side lies nearest where it stands.

## Files affected

- Modify: src/sim/guests/SPEC.md
- Modify: src/sim/guests/guests.h (WALK_SPEED's comment, WALK_SPEED_SPREAD, SIDE_BIAS)
- Modify: src/sim/guests/internal/guest.h (speed and share, saved)
- Modify: src/sim/guests/internal/route_ahead.h and src/sim/guests/route_ahead.cpp (each route point's width and side position, moving a route to a side)
- Modify: src/sim/guests/guests.cpp (drawing speed and share, walking at the guest's speed, moving its route to its side)
- Modify: tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park (made again)
- Modify, untracked: parks/busted-path.park, parks/jank.park, parks/radii.park (their guests carried into the new save)

## Dependencies

- paving-walk: the route ahead, the aim, and GuestGround's fits. Merged.
- early-turns: the route ahead past the next node. Merged.
- The carrier's width (shared-medium's carrier-widths). Met.

## Out of scope

- Stepping around other guests. sidestep adds it. Until then, a faster guest walking in the same lane as a slower one passes through it.
- A guest's speed changing with tiredness or crowding, and groups walking at one pace. Both are already deepening candidates of this milestone.
- Showing a guest's speed and side in the inspector. The deepening candidate "A guest's aim and its sidestep shown in the inspector" can take them.

## Open questions

- Whether SIDE_BIAS 0.5 and a 1.0 to 1.6 m/s spread read as natural on main-street.park. Resolved by the user playing it and adjusting SIDE_BIAS and WALK_SPEED_SPREAD.
