# Milestone: Spread Guests

Slice: good-paths

Status: planned

## Summary

spread-guests makes guests use a path's whole width the way people do. Each guest walks at its own speed, so guests overtake one another instead of keeping the spacing they arrived with. Each is drawn on a line of its own across the path, keeping to one side by the way it walks, clear of the edges, and wandering a little. A follower in the drawing moves each guest smoothly toward that line, so guests cut the inside of corners, cross junctions on a curve, and never jump where the width or the direction changes. Guests heading somewhere move toward the side of their next turn before they reach it. Guests also walk straight through shaping points without choosing there, so the number of points the player draws doesn't change how they behave. This is the good-paths slice's fifth and last member. wide-routes has given every carrier its segment's width and put nodes only at points and connector ends. This milestone is what makes a 10 m street look used, and it makes warm-main-street.park, which the slice's closing checks read.

## Acceptance criteria

1. Each guest walks at its own speed for its whole stay, drawn from its key. In the running app, faster guests overtake slower ones on a long path. (play)
2. In warm-main-street.park, guests on the 10 m main street are drawn spread across its width, those walking one way mostly on their right and those walking the other way on theirs, thinning out toward the edges, each on its own line that wanders slowly. On the 3 m side paths they walk closer together. At every width the tools allow, from 1.5 m to 16 m, every guest's box stays on the paving, including where the follower cuts the sharp inside corner of a bend. (scripted capture, which is the slice's criterion 4, and play)
3. A guest's drawn box moves continuously. It doesn't jump at a node where the path bends, where the width changes, or where the guest turns back, and it cuts the inside of corners. If the simulation moves a guest farther in one tick than any guest walks, as when its path is deleted under it, the box jumps with it. The drawn positions are never saved, the simulation never reads them, and a guest seen for the first time is drawn on its line at once. (play)
4. A guest heading to a shop or home moves toward the side of its next turn in the few meters before a junction or a bend. (play)
5. Picking a guest and the inspected guest's highlight both use its drawn box. (play)
6. A guest walks straight through a shaping point, a node with exactly two edges and no anchor, without choosing there, so the number of points the player draws doesn't change what guests choose. Inserting a point into a straight guest segment under a wandering guest whose stay is over leaves it wandering as it walks through the new point, where choosing would have sent it home. (integration test in through-points)
7. tpj_scenarios --slice-parks makes tests/parks/warm-main-street.park from main-street.park, as it makes warm.park from fed.park, and the park is checked in. The park world checks and the cross-build check pass with every park in tests/parks, and food_loop_test passes. If a change to what a save holds makes the stress parks stale, they are generated again. (ctest and scripts/release-check.sh)
8. The runtime report after this milestone becomes the new baseline, as the slice says, and on full.park it reports the cost of moving and drawing about 2,000 guests' boxes. (scripts/runtime-report.sh)
9. src/sim/guests/SPEC.md and the spec of every module that draws or picks guests describe their changes.

## Medium

Cross-capability, as the slice's medium map assigns to believable-guests:

- Networks (navigable-networks, through shared-medium's network type): spread-lines and through-points walk guests through nodes, and through-points reads each node's edge ends and anchor to find shaping points. spread-lines reads groundPoint, widthAt, and directionAt at each guest's place. turn-lines reads the edges at the node ahead.
- Route distance (navigable-networks): turn-lines samples guest route distance's Next step for a guest's target or the entrance, to find its next turn. The guests module already samples it for walking.
- Inspection records (decision 0025): spread-lines adds the way a guest walks to its record, and draws and picks guests from their records. Nothing in the park reads them.
- Keyed draws (deterministic-simulation): spread-lines draws each guest's speed, line, and wander on the guest's key.

The milestone adds no fields or flows. Route distance is unchanged: guests still walk along carriers and route distance measures center lines (principle 4). The follower is drawing state, kept apart from the world, never saved, and never read by the simulation (principles 1 and 10).

## Dependencies

- shared-medium's carrier-widths: widthAt, directionAt, and carry-over that keeps a guest at its ground position. Met.
- effortless-building's path-network: segments with widths and corner radii, and main-street.park. Met.
- navigable-networks' wide-routes: nodes only at points and connector ends, and each carrier with its segment's width. Met.
- believable-guests' hungry-guests: guests, their walking and choices, inspection records, the guest mesh, and --slice-parks. Met.

## Core feature

spread-lines. It is what the player came to see: guests at their own pace, on their own lines, keeping to a side, and moving smoothly across a wide street. It is visible in the app from its first commit, and it shows whether the method reads as natural before the rest is built on it.

## Features

1. `spread-lines`: each guest's speed drawn from its key, each guest's line across the path by the way it walks with fewer near the edges and a slow wander, the follower that moves each drawn box toward its line by simulated time and jumps with the simulation's own jumps, the way a guest walks in its record, and picking and the highlight from the drawn box. Depends on: none.
2. `turn-lines`: a guest heading to a shop or home moves toward the side of its next turn before a junction or bend. Depends on: feature 1.
3. `through-points`: guests walk through shaping points without choosing, --slice-parks makes warm-main-street.park, the stress parks generated again if they're stale, and the new runtime baseline, last so the baseline and the park hold every change. Depends on: features 1 and 2.

## Tuning values

Starting values. Each feature that introduces one fixes it in its spec, and playing adjusts them.

- Walking speed: drawn per guest uniformly from 1.0 to 1.6 m/s, so the mean stays WALK_SPEED, the speed food availability's REFERENCE_SPEED uses.
- Follower time constant: 0.5 s of simulated time.
- A jump the follower follows at once: any move in one tick longer than the fastest guest walks in a tick.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Drawing guests apart where their boxes overlap, as a pass over the drawn positions only. Gated on: play showing overlaps that read badly.
- Wandering guests moving toward the side of their next turn, by drawing their pick ahead of the node. Gated on: turn-lines in play.
- Groups that walk side by side at one pace.
- A guest's speed changing with tiredness or crowding.

## Open questions

- Whether the lines, the wander, and the follower's time constant read as natural on main-street.park, and whether the keep-to-side bias is strong enough to show two streams. Resolved by the user playing spread-lines and adjusting the values.

## Research notes

- Varying speed per guest is the cheapest fix with the largest effect: real speeds center on about 1.34 m/s and range roughly from 1.0 to 1.65 m/s.
- Two-way flow sorts into lanes on the right, walkers keep off the edges, and they cut the inside of corners.
- A line worked out only from a guest's current state jumps at bends, width changes, and turnarounds, and a wandering guest can't blend across a junction because it doesn't know its next edge or remember its last. A follower in the drawing, stepped by simulated time, keeps every drawn guest continuous and cuts corners on its own.

Depth is in RESEARCH.md.
