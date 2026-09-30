# Milestone: Explained Food

Slice: boxes-and-tubes

## Summary

explained-food lets the player ask why and what if of the slice's park. It adds the library tpj_legible, which computes food availability at any place of the guest network with each shop's contribution, reading only fields and intent, and the app shows it as an overlay: the ground beside each guest path shaded by how well fed a guest standing there could be. Hovering the band lists the shops behind the value. While a valid ghost is shown, or the delete tool hovers a path or box, a candidate world is kept current, and the overlay shows the park as the edit would leave it, with a shop ghost's context beside it. Clicking a guest or a shop opens an inspector that explains it from its inspection record. It is the slice's last member: every field, flow, record, and candidate it reads is in place, and once it lands the slice's remaining criteria, 4, 5, 8, 9, and 10, can be checked.

## Acceptance criteria

1. tpj_legible links tpj_sim alone. foodAvailability gives, for a world and a place on its guest network, a list of contributions and a value. With R the entries sampleField of guest route distance gives at the place, a source of R contributes when its offer, found as guests find it (sim/guests/SPEC.md, Choice), exists and has Supplied true. Each contribution holds the source, the offer's Relief, R's Distance, the offer's Wait in seconds, the effective time, Distance / REFERENCE_SPEED plus the wait, and its term, Relief times foodDiscount of the effective time. Contributions are in ascending source order, and the value is 0.0 with each term added in that order, so the terms reconstruct it exactly, bit for bit, at every place (slice criterion 4). A place with no contributing source has the value 0.0. Computing it changes nothing: hashWorld is the same before and after.
2. foodDiscount is the authored piecewise-linear curve through the points of FOOD_DISCOUNT_CURVE (decision 0020), falling from 1 at no time to 0, and 0 beyond its last point. REFERENCE_SPEED, the curve's points, and OVERLAY_BAND take the starting values under Tuning values.
3. The Debug panel has a Food overlay checkbox, off at start unless the app is given --overlay food. While it is checked, each ground point within OVERLAY_BAND of a guest path's line is shaded by the food availability at its nearest guest path place, the place of nearestPlace on the guest network's path carriers alone, so where the bands of several paths overlap, the nearest path's value shows. The shading is sampled along each line and interpolated between samples, so it is exact at the samples, and near a bend of one line the far side of the bend may show. It uses a perceptually uniform, colorblind-safe ramp over a fixed range, with zero a color distinct from every nonzero value, drawn over the terrain and under paths, boxes, and guests. Beyond the band there is no shading. The overlay mesh is built on the CPU, so tests check it without a GPU, and it is rebuilt each tick while shown. --overlay with any name but food, or with --hash, prints the usage and exits with a nonzero status.
4. While the overlay is shown and the cursor's ground point lies within the band, a tooltip lists each contributing shop with its relief, route distance, expected wait, effective time, and term, and the value, which is exactly the sum of the terms listed, all taken from foodAvailability at the cursor's nearest guest path place.
5. While the tool's tentative edit is accepted, the app keeps a candidate, makeCandidate of the world with the edit queued, rebuilt after every cycle and whenever the edit changes, and the overlay and its hover show the candidate's availability. With no edit, or an edit isAccepted refuses, there is no candidate, and they show the committed world's. The ghost's walkways and starved marks come from the same candidate.
6. For a shop ghost, an AddBox of a shop or a MoveBox of a shop box, the app shows its context: the hungry footfall at the place where the candidate's guest connector meets the guest path, sampled in the committed world, since footfall is an average over past ticks that a candidate, never stepped, holds only where the last step published it, or that it has no guest connector; and the length of its supply route in the candidate, the least backstage route distance from a depot box to its backstage anchor, or "no supply route". tpj_legible computes it headless.
7. In warm.park, the candidate of an AddBox placing a second shop touching both the guest path and the backstage path equals, by worldsEqual, the world stepWorld gives when the same AddBox is queued for that cycle, and food availability at the guest path places nearest the new shop is higher in the candidate than in the world before, by a nonzero amount (slice criterion 5). The same holds for a DeletePath of warm.park's backstage path, where availability falls to 0. Making any number of candidates leaves the committed world's hashWorld unchanged.
8. With the Look tool, a click opens an inspector for the guest or shop box the cursor's ray first meets, as they are drawn: a guest's box built from its record's Position, and a shop box from its pose in intent. The guest inspector shows its activity, hunger, target, stay, meals eaten, and last meal, and its last choice as a table of options, each with its relief, distance, wait, and commitment terms, score, and probability, the picked one marked. The shop inspector shows its stock, queue, supplies on order, limiting factor, and whether it is starved. Both refresh every frame from guestRecord, shopRecord, and intent alone, and say when their entity is gone. Their rows are built headless, so tests check them.
9. Scripted captures of warm.park and cut.park, each after 3000 ticks with --overlay food, show what slice criteria 8 and 9 describe: paths, boxes, and guests with the overlay on the ground, and in cut.park the starved shop marked and the overlay dimmed around it compared with warm.park.
10. Integration tests in tests/integration/ check slice criteria 4 and 5 on the slice's parks. Evan walks through slice criterion 10 in the running app.
11. The specs of every module the milestone adds or changes describe it as built. Both builds build without warnings, scripts/tidy.sh is clean, and the tests pass on both.

## Tuning values

The capability leaves these to this milestone. They are starting values, fixed in the spec by the feature that introduces them, and adjusted by looking at captures (RESEARCH.md).

- REFERENCE_SPEED: 1.3 m/s, the guests' WALK_SPEED, so an effective time is what a guest would feel.
- FOOD_DISCOUNT_CURVE: (0 s, 1), (60 s, 0.5), (120 s, 0.2), (240 s, 0). A guest at a shop's door with a short wait reads about 0.95 of the shop's relief, and one at fed.park's far end about 0.5 to 0.6.
- OVERLAY_BAND: 6 m from a guest path's line, twice its width, reaching the doors of the boxes the path serves.

## Medium

legible-simulation produces nothing the simulation consumes. Its features read the fields and records the slice's medium map assigns it, and, as every consumer does, the networks and park intent:

- Route distance (navigable-networks): guest route distance at sampled places, for each source's distance (food-overlay), and backstage route distance at a shop ghost's backstage anchor, for its supply route (candidate-previews). Sampled with sampleField.
- Food offer (plausible-operations): each source's offer at its lowest anchored node's nodePlace, for Relief, Wait, and Supplied (food-overlay).
- Hungry footfall (believable-guests): fieldValue in the committed world at the place where a shop ghost's connector meets the guest path in the candidate (candidate-previews).
- Networks (shared-medium, navigable-networks): parkNetwork and the Network type's queries, to find guest path places, anchors, and connectors (food-overlay, candidate-previews).
- Candidate resolution (deterministic-simulation, decision 0025): makeCandidate of the world with the tool's tentative edit (candidate-previews). The candidate is resolved by the owning modules' own resolvers, and tpj_legible samples it as it samples the committed world.
- Inspection records (believable-guests, plausible-operations, decision 0025): guestRecord and shopRecord (inspectors).
- Park intent (effortless-building): parkPaths, to find guest paths (food-overlay); parkBoxes, for depot and shop boxes and their poses (candidate-previews, inspectors); and the tool's tentative edit (candidate-previews).

Food availability is computed for display and never enters the world: not its state, its hash, or its saves.

## Dependencies

- deterministic-simulation's world-as-value: makeCandidate, worldsEqual, and hashWorld. Met.
- shared-medium's first-field-and-flow: sampleField and fieldValue. Met.
- effortless-building's sketch-a-park: tools, tentative edits, ghosts, the renderer's meshes and pipelines, the Debug panel, and --capture. Met.
- navigable-networks' paths-become-routes: networks, anchors, and route distance. Met.
- plausible-operations' supplied-food-shop: the food offer and shop records. Met.
- believable-guests' hungry-guests: hungry footfall, guest records, and the slice's park files. Met.

## Core feature

food-overlay. It creates tpj_legible and puts food availability on the ground at once: the Debug panel's checkbox and --overlay food show where guests could eat well, and its test checks slice criterion 4. Hover, previews, and a shop ghost's context all read the availability it computes, and the captures of slice criteria 8 and 9 need only it.

## Features

1. `food-overlay`: tpj_legible with foodAvailability and its contributions, REFERENCE_SPEED, the discount curve, the banded overlay mesh with its ramp, drawn over the terrain and under the park, the Debug panel's Food overlay checkbox, --overlay food, the integration test of slice criterion 4, and the captures of slice criteria 8 and 9. Depends on: none.
2. `overlay-attribution`: the hover tooltip listing each contributing shop's relief, distance, wait, effective time, and term, and the value, at the path place the band's shading used. Depends on: feature 1.
3. `candidate-previews`: the candidate kept current after every cycle and on every change of an accepted edit, the overlay and hover on the candidate, the ghost's walkways and marks from the same candidate, a shop ghost's footfall and supply route context, and the integration test of slice criterion 5 and of hashes left unchanged. Depends on: feature 1.
4. `inspectors`: ray picking of guests and shop boxes as drawn, with the Look tool, and the guest and shop inspectors, their rows built headless from records and intent. Depends on: none.

## Deepening candidates

Unordered pool this milestone draws later features from.

- The ghost's own contribution marked in the preview's hover, so the player sees which term the edit adds or removes.
- Hovering an option in the guest inspector's choice table highlights that shop and the route the guest would take to it.
- The camera following an inspected guest.
- Shading exact at every ground point near a line's own bends, where the far side of the bend can show through. Gated on: the approximation being visible in captures.

## Open questions

None.

## Research notes

- Food availability is a gravity accessibility measure, a sum of decayed terms, so each shop's term is its contribution and an ordered sum reconstructs the value exactly.
- The tuning values come from fed.park's distances and waits: effective times run from near 0 to about 70 s.
- The band is drawn as a ribbon along each guest path with per-vertex values, falling from its line so the nearest line's ribbon lies highest where bands overlap, cheap and testable without a GPU. A ground grid and a shader were rejected.
- One resolution of warm.park costs at most about 30 ms, so a candidate per tick is expected to stay interactive, and candidate-previews measures it.
- Guests and shops are picked by a ray against their drawn boxes, not by the ground point, which misses a guest's body from a low camera.

Depth is in RESEARCH.md.
