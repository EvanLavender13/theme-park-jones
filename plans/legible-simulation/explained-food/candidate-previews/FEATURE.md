# Feature: Candidate Previews

## Summary

While the tool's tentative edit is accepted, the food overlay and its tooltip show the park as the edit would leave it, and a shop ghost says what it would find. tpj_legible gains previewEdit, which gives a Preview of an edit on a world: the edit, the candidate world makeCandidate gives with the edit queued when isAccepted takes it, and, for a shop ghost, its ShopContext. shopContext finds where the shop's guest connector meets the guest path in the candidate, the committed world's hungry footfall there, and the shop's nearest depot and supply route length in the candidate. keepPreview keeps a preview between frames and makes it again only when the world's tick or the edit changes. The app keeps its preview with it, and rebuilds the ghost and overlay meshes only when the preview is made again or the highlight or the overlay's checkbox changes, so frames between ticks rebuild nothing: the ghost's walkways and starved marks, the overlay, and the food tooltip all read the one kept candidate, and follow every cycle and every change of the edit. buildGhostMesh gains an overload taking the candidate. A new app component draws a shop ghost's context in the tooltip at the cursor. Integration tests check slice criterion 5 on warm.park.

## Acceptance criteria

Throughout, the candidate of an edit on a world is makeCandidate(world, a queue holding the edit through queueEdit), and a guest path is a path of parkPaths of kind Guest.

1. previewEdit(world, edit) holds the edit given as Edit. Its Candidate is worldsEqual to the candidate of the edit on the world when the edit is given and isAccepted(world, edit), and none otherwise. Its Shop is shopContext(world, candidate, edit) when Candidate holds one, and none otherwise. previewedWorld(world, preview) is the preview's candidate when it holds one, and the world otherwise.
2. buildGhostMesh(world, edit, candidate) is buildGhostMesh(world, edit)'s own ghost of the edit, followed, when candidate holds a world, by appendWalkways of that world with GHOST_ALPHA and then appendStarvedMarks of it with GHOST_ALPHA, and by nothing when it holds none. So buildGhostMesh(world, edit) equals buildGhostMesh(world, edit, the candidate of the edit on the world when isAccepted(world, edit), and none otherwise).
3. shopContext(world, candidate, edit) gives a ShopContext exactly when the edit is an AddBox of kind Shop and some box of parkBoxes(candidate) has a key that no box of parkBoxes(world) has, or a MoveBox whose Box is the key of a shop box of parkBoxes(world). Its Shop is, for the AddBox, the lowest such key, and for the MoveBox, its Box.
4. With C the carrier of parkNetwork(candidate, PathKind::Guest) keyed connectorKey(Shop, Face::Front), a ShopContext's Connection is the first place, in order, of stopPlaces of the node of C's last stop whose carrier is a guest path of the candidate, and none when there is no C or no such place. Its Footfall is fieldValue of HungryFootfall on parkNetwork(world, PathKind::Guest) at Connection, in the world given, not the candidate, and 0.0 when there is no Connection.
5. A ShopContext's Supply is nearestDepot(candidate, Shop).
6. keepPreview(kept, world, edit) makes the preview again exactly when kept's Tick is none or differs from the world's tick, or kept's Made.Edit differs from the edit, and returns whether it did. After it returns, kept's Tick is the world's tick, and Made equals previewEdit(world, edit): its Edit is the same, its Candidate is worldsEqual or none alike, and its Shop is the same. When it does not make the preview again, kept is unchanged.
7. previewEdit, keepPreview, and shopContext change nothing: however many are made, the world's hashWorld is the same before and after.
8. Opened tests/parks/warm.park and stepped one cycle, the candidate of each of two edits equals, by worldsEqual, the world a second copy of the opened park gives when stepped one cycle with the edit queued (slice criterion 5): A, an AddBox of a shop at (6.5, 106.8) facing (-1, 0), whose front door lies 3.5 m from guest path 2 and back door 2.5 m from backstage path 6, and whose footprint clears the curve of guest path 5 by 0.27 m beyond its half width and shop 7 by 0.2 m, and D, a DeletePath of backstage path 6.
9. In that stepped warm.park, previewEdit of A has a Shop with a Connection and a Supply, and foodAvailability's Value at the Connection is higher in the candidate than in the world. In previewEdit of D's candidate, foodAvailability's Value is 0.0 at the nodePlace of every node of the guest network, where in the world it is positive at some node.
10. In the running app with the Food overlay shown, while an accepted ghost is shown, the band and the food tooltip show the candidate's availability, with the ghost's shop among the tooltip's rows for a place-shop ghost, and a refused ghost leaves them showing the committed world's. Hovering warm.park's backstage path with the delete tool turns the band the zero color. A shop ghost's tooltip shows `Hungry footfall` with a value or `No guest connector`, and `Supply route` with a length and depot or `No supply route`. This is checked by hand, as slice criterion 10 is.

## Medium

The feature produces nothing the simulation consumes. It reads:

- Candidate resolution (deterministic-simulation, decision 0025): makeCandidate of the world with the tentative edit queued, through isAccepted and queueEdit (effortless-building).
- Hungry footfall (believable-guests): fieldValue in the committed world at a shop ghost's connection place.
- Backstage route distance (navigable-networks), through operations' nearestDepot in the candidate, for a shop ghost's supply route.
- The guest network (navigable-networks, through shared-medium's Network queries): the shop's connector, its last stop's node, and stopPlaces.
- Park intent (effortless-building): parkBoxes and parkPaths, to find the ghost's shop and guest paths, and the tool's tentative edit.
- Food availability (food-overlay) on the candidate.

## Principle checks

- Principle 1 and 10: previewEdit, keepPreview, and shopContext leave hashWorld unchanged (criterion 7), and a candidate equals the committed result of its edit (criterion 8), so a preview shows what committing will do.
- Principle 2: a shop ghost whose door reaches no guest path, or whose supply route is cut, gives a context with no Connection or no Supply, never an error (criteria 4 and 5), and an edit that is refused or not a shop's gives no candidate or no context (criteria 1 and 3).
- Principle 4: the connection is where the candidate's own connector meets the path (criterion 4), and the supply route is a route distance (criterion 5).
- Principle 8: the overlay and tooltip explain the candidate as they explain the committed world (criteria 1, 9, and 10), and cutting the park's only supply route shows as zero availability (criterion 9).

## Spec changes

- src/legible/SPEC.md: the contract adds candidates made with makeCandidate and the owning modules' queries, and a Previews section defines Preview, previewEdit, previewedWorld, KeptPreview, keepPreview, ShopContext, and shopContext.
- src/render/SPEC.md, Ghosts: buildGhostMesh's overload taking the candidate, and the two-argument form defined by it.
- src/app/SPEC.md, Park and Tools: the kept preview, and the ghost and overlay rebuilt only when it is made again or the highlight or checkbox changes. Tooling UI: the overlay and food tooltip on previewedWorld, and the shop context in the tooltip.

The exact text is in PLAN.md, Tasks 1 to 3.

## Files affected

- Modify: src/legible/SPEC.md, src/legible/CMakeLists.txt
- Create: src/legible/preview.h, src/legible/preview.cpp
- Modify: src/render/SPEC.md, src/render/park_mesh.h, src/render/park_mesh.cpp
- Modify: src/app/SPEC.md, src/app/CMakeLists.txt, src/app/main.cpp
- Create: src/app/shop_context_tooltip.h, src/app/shop_context_tooltip.cpp
- Tests, from the test pass: tests/legible/, tests/render/, tests/integration/

## Dependencies

- food-overlay and overlay-attribution, merged: foodAvailability, the overlay, and the food tooltip.
- makeCandidate, worldsEqual, and hashWorld (deterministic-simulation); nearestDepot (plausible-operations); HungryFootfall (believable-guests); connectorKey and parkNetwork (navigable-networks).

## Out of scope

- Making a candidate incrementally, or off the frame's thread, so a moving ghost costs less than a copy and a resolution per new pose (RESEARCH.md).
- Marking the ghost's own contribution in the tooltip: the milestone's deepening candidate.
- Context for a depot ghost or a path ghost.
- A capture of a ghost, since a capture has no pointer.

## Test pass decisions

- Footfall's "in the world, not the candidate" has no test that tells them apart. A candidate's footfall is what the last step published, carried by resolution, so no park gives a known different value at the connection. The test checks that Footfall equals the world's fieldValue there and that it is positive.
- At a junction, the first guest path place of stopPlaces and the connector's own connection place agree, since nearestPlace and stopPlaces both order by carrier key. The junction test rules out a later place and the connector's own stop, which is what the rule protects against.
- For an AddBox of a shop, the lowest new key counts whatever its box's kind, as criterion 3 reads. A candidate of the AddBox alone adds only its shop, so no test covers a candidate from other commands.
- keepPreview trusts that a world changes only as it ticks. A caller that replaces its world empties the kept preview itself, as the app does on New and Open, so a replaced world at the same tick is the app's to handle and is checked by hand with criterion 10.
- Criterion 10, the tooltip's lines, and when the app rebuilds its meshes are checked by hand.

## Open questions

None.
