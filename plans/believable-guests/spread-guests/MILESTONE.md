# Milestone: Spread Guests

Slice: good-paths

Status: planned

## Summary

spread-guests makes guests walk on the paving like people shaped like boxes. Each guest holds a position of its own on the guest paving, and the network still chooses its route. Each tick it walks straight toward the farthest point along that route it can reach without its box leaving the paving, moved to its own side of the path. It walks at its own pace and steps around the guests near it, which it senses only through guest presence, the park's first ground field. Its place on the network follows from where it stands, so route distance, offers, visits, meals, footfall, and the overlay work as before (decision 0038). Guests decide each turn a little before they reach it, and walk through shaping points without choosing. This is the good-paths slice's last member. Every path, width, and junction it walks on is built, and shared-medium's ground-fields gives it guest presence. It makes warm-main-street.park, which the slice's closing checks read.

## Acceptance criteria

1. Each guest holds a ground position on the guest paving, and saves hold it. Its box stays on the paving at every width the tools allow, from 1.5 m to 16 m, through bends, junctions, tapers, and walkways. The one exception is the door a guest is entering or has just come through, where it stands at the door. Where guest paths overlap without sharing a point, a guest stays on the path it is on. (play, and the slice's scripted capture)
2. A guest walks straight across the paving toward its aim: the farthest point of its route that it can reach in a straight line with its box on the paving, moved to its own side of the path (criterion 5). Where the route ends at the door a guest is heading for, its aim is the door itself, with no shift to a side. A guest heading for a shop across a wide street walks straight to its door. At a bend, a guest cuts the inside without leaving the paving. (play)
3. A guest's place on the network is the nearest place on the carrier of the segment, point, or walkway it stands on. Choices, route distance, food offers, visits, meals, and hungry footfall read and feed that place as before, and food_loop_test passes. (ctest, and play with the food overlay)
4. A guest makes its choice for a node about 10 m before reaching it, and a wandering guest picks its next edge then. A guest walks through a shaping point, a node with exactly two edges and no anchor, without choosing there. Inserting a point into a straight guest segment under a wandering guest whose stay is over leaves it wandering as it walks through the new point, where choosing would have sent it home. (integration test in early-turns)
5. Each guest walks at its own speed for its whole stay, drawn from its key, so faster guests overtake slower ones. Each keeps to its own share of the width, biased to the right of the way it walks and thinning toward the edges. On the 10 m main street, guests walking one way stream along one side and those walking the other way along the other. On the 3 m side paths they walk close together. (play, and the slice's scripted capture)
6. Each guest publishes where it stood and how it moved in guest presence, a ground field. Each guest samples it around itself and steps aside from guests it would otherwise walk into, judged from where they stood at the end of the last tick, and never from guests already moved this tick. A guest that leaves the park drops its entry. Guests sense one another only through guest presence. warm.park and warm-main-street.park hold guest presence, so the park world checks cover its entries. This confirms ground-fields' deferred criteria 2, 4, and 5. (play, and the park world checks)
7. After an edit, a guest keeps its ground position if guest paving still lies there, and its place follows. If not, it moves to the nearest point where its box fits on the guest paving. If no guest paving is left, it leaves the park. No guest jumps when the player widens, splits, or merges a segment under it. A candidate's guests stand where committing its edit would put them (principle 8). (play, and the slice's integration test for a split under walking guests)
8. Before guests walk on it, a check over every park in tests/parks and tests/parks/stress shows that joined guest paving fits edge to edge with no holes. If it doesn't, walkway-pieces fixes the paving in sim/park. sim/park gives a walkway's piece on its own, and routes gives each walkway with its connector's key, so guests know which connector a walkway belongs to. (a check walkway-pieces runs once and reports)
9. tpj_scenarios --slice-parks makes tests/parks/warm-main-street.park from main-street.park, as it makes warm.park from fed.park. Every checked-in park that holds guests is made again whenever what a guest saves changes. The park world checks, the cross-build check, and food_loop_test pass with every park in tests/parks. (ctest and scripts/release-check.sh)
10. The runtime report after this milestone becomes the new baseline, as the slice says. On full.park it reports what walking, sensing, and stepping around about 2,000 guests costs per tick, against the 33 ms budget. A tick's work on guests grows with the guests, never with the size of the park (decision 0031). This confirms ground-fields' deferred criterion 7. (scripts/runtime-report.sh)
11. src/sim/guests/SPEC.md, src/sim/park/SPEC.md, src/sim/routes/SPEC.md, and src/scenarios/SPEC.md describe their changes, and believable-guests' brief describes guests walking on the paving. (review)

## Medium

Cross-capability, as the slice's medium map assigns to believable-guests:

- Park intent's derived shape (effortless-building, decision 0025): the guest paving's pieces from parkSurface, and each guest walkway's piece with its connector's key. walkway-pieces adds the walkway piece and key to sim/park and routes, and paving-walk builds the guests' walkable paving from them as derived data.
- Networks (navigable-networks, through shared-medium's network type): paving-walk reads each guest's route along carriers and finds its place on the carrier of the piece it stands on. early-turns reads each node's edges and anchor to find shaping points. own-pace-and-side reads widthAt and directionAt along the route to set a guest's own side.
- Route distance (navigable-networks): paving-walk follows Next steps toward a target or the entrance, chained from node to node, to know the route ahead. Choices read it as before.
- Food offers, guest visits, and meals (plausible-operations): unchanged, read and fed at the guest's place.
- Hungry footfall: unchanged, fed at the guest's place. legible-simulation's overlay reads it as before.
- Guest presence: a ground field that sidestep registers with shared-medium's addGroundField. Each guest sets its entry and samples the others'. Nothing outside the guests module reads it.
- Inspection records (decision 0025): a guest's record gives the ground position it stands on. Drawing and picking read it as before.
- Keyed draws (deterministic-simulation): own-pace-and-side draws each guest's speed and share of the width on its key.

Route distance stays the network's (principle 4, decision 0038). The walkable paving is derived and never saved (principle 1). Guests step in key order, but every guest reads the others only through last tick's guest presence, so the result doesn't depend on order (principle 10).

## Dependencies

- shared-medium's ground-fields: addGroundField, setGroundEntry, dropGroundEntry, and sampleGroundField. Met.
- shared-medium's carrier-widths: widthAt and directionAt. Met.
- effortless-building's path-network: segments with widths, corners, edges, and tapers, the paved surface, walkways, and main-street.park. Met.
- navigable-networks' wide-routes: nodes only at points and connector ends, carriers with their segment's width, and doors connected from a segment's edge. Met.
- believable-guests' hungry-guests: guests, their choices, visits, meals, footfall, inspection records, and --slice-parks. Met.

## Core feature

paving-walk. It is the change the slice exists for: guests stand on the paving and walk straight across it along their routes, with their boxes kept on it. It is visible from its first commit, and every later feature adjusts how a guest walks on it.

walkway-pieces comes before it, although it shows nothing in play. It holds the work outside the guests module: two small additions to sim/park and routes, and the hole check, whose fix has no known size until the check runs. Keeping them apart means paving-walk changes only the guests module and can't stall on a paving fix.

## Features

1. `walkway-pieces`: sim/park gives a walkway's piece on its own, routes gives each walkway with its connector's key, and the one-time check over every park that joined guest paving has no holes, with any fix to sim/park. Depends on: none.
2. `paving-walk`: the guests' walkable paving, joined by shared edges, with walkways joined to the paving they overlap and a lookup by cell; a ground position in guest state; walking toward the farthest reachable point of the route with the box kept on the paving; the place from where a guest stands; carrying and stranding by ground position; and every park holding guests made again. Depends on: feature 1.
3. `early-turns`: choosing for each node about 10 m before it, wanderers picking their next edge then, and walking through shaping points without choosing, with the integration test of criterion 4. Depends on: feature 2.
4. `own-pace-and-side`: each guest's speed and share of the width drawn on its key, and its aim moved to its own side of the route by that share, except at a door it is heading for. Depends on: feature 2.
5. `sidestep`: guest presence, and each guest stepping aside from the guests it would walk into, with warm.park made again so it holds guest presence. Depends on: feature 2.
6. `main-street-guests`: --slice-parks makes warm-main-street.park, the stress parks are made again, the slice's scripted capture of warm-main-street.park, and the new runtime baseline. It comes last, so the park and the baseline hold every change. Depends on: features 2 to 5.

## Tuning values

Starting values. Each feature that introduces one fixes it in its spec, and playing adjusts them.

- Clearance: 0.45 m, half the diagonal of a guest's 0.6 m box, so the box stays on the paving at any angle.
- Turn lookahead: 10 m before a node.
- Walking speed: drawn per guest uniformly from 1.0 to 1.6 m/s, so the mean stays WALK_SPEED, the speed food availability's REFERENCE_SPEED uses.
- Share of the width: the mean of two uniform draws, set to the right of the way the guest walks by a bias the feature fixes.
- Guest presence sampling radius: about 3 m, inside the ground field's 4 m cells.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A guest's aim and its sidestep shown in the inspector, so the player can see why it walked where it did.
- Groups that walk side by side at one pace.
- A guest's speed changing with tiredness or crowding.
- Guests pausing or slowing where the paving is crowded, instead of only stepping aside.

## Open questions

- Whether the shares, the side bias, the lookahead, and the sidestep read as natural on main-street.park. Resolved by the user playing each feature and adjusting the values.

## Research notes

- Guests walk the paving as a navigation mesh: a raycast across joined pieces to aim, and a constrained move to step, as Detour does. A hole check over every park comes first.
- The box stays on the paving by keeping a clearance of half its diagonal from walls along the aim line and the step. Eroding pieces one by one was rejected because it misses neighbours' walls at concave corners.
- Choosing about 10 m before each node keeps a wanderer's route running past the node, so it doesn't swing back to the middle at every connector.
- Real walking speeds vary from about 1.0 to 1.65 m/s, two-way flow forms lanes on the right, and walkers keep off edges.
- Sidestepping predicts the nearest approach from last tick's guest presence, as Karamouzas' and Reynolds' models do.

Depth is in RESEARCH.md.
