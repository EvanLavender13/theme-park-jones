# Research: kept-checks

## Why kept-checks comes before unit-surface

unit-surface stores the paved and walked surfaces per owner, and every builder refills storage its caller keeps. On a frame path, a surface built into fresh storage would allocate once per owner inside its loop, which docs/conventions.md forbids. The edit checks build surfaces into fresh storage today and have no kept storage to pass in. unit-surface's plan assumes kept-checks gives them that storage first.

## How often a changed frame plans the edit

A changed frame of the path drag plans the edit five times:

- keepPreview's isAccepted (src/legible/preview.cpp:88).
- makeCandidate applies the queued AddPath, and its applyCommand plans it again (src/sim/park/edits.cpp:2103).
- buildGhostMesh's own isAccepted (src/render/park_mesh.cpp:685).
- editedSurface for the ghost's paving (park_mesh.cpp:718).
- pathCrossings for the join marks (park_mesh.cpp:361).

A DeleteSegment or SetSegmentWidth ghost adds editedSegments, which plans once more. Each plan runs planOf and withCurves. Every plan but editedSurface's also runs acceptedPlan.

A changed frame of the shop drag checks the box three times: keepPreview's isAccepted, the candidate's applyCommand, and the ghost's isAccepted. Each check derives the whole layout and ground into fresh GroundLists, lists every solid, and builds the surface near the box.

In the derived-units profiles, the timed part of each drag is keepPreview plus buildGhostMesh. The rest of the samples under the drag are the tick between frames.

| Drag | Timed part | Planning and checks | Allocations outside resolution |
|---|---|---|---|
| shop | 55.6% | isAccepted 1.9%, shopContext 0.4% | about 1.2% |
| path | 62.3% | isAccepted 6.9%, applyCommand 2.6%, editedSurface 8.3%, pathCrossings 2.9% | about 3.7% |

The timed part is 9.48 ms for the shop drag and 13.33 ms for the path drag (build/flat-ground.report). One percent of the samples is about 0.17 ms of the shop frame and 0.21 ms of the path frame. Each plan's withCurves is about 1.2% of the path drag's samples, and each acceptedPlan about 1.1%. editedSurface's own derivation is about 7.2%. Planning once keeps one plan, one acceptance check, and one editedSurface derivation. That saves about 11% of the path drag's samples, about 2.4 ms. For the shop drag, one box check instead of three saves about 1.3% of the samples, about 0.2 ms.

## Every allocation on the frame's paths

In src/sim/park/edits.cpp, planning an edit:

- planPath, planMove, planInsert, planRemove, planEdge, planWidth, and planDeleteSegment copy parkSegments(world). That makes a new list with a new ground line for every segment in the park.
- segmentsAt and segmentsNotAt copy the matching segments, lines included. isBendOfKind, bendSplitOf, meetCorner, isTargetOfKind, planMove, and planRemove call them. addMeetings calls segmentsAt inside its loop over the park's corners, and again inside its loop over the plan's lines and the meeting corners.
- addMeetings copies parkCorners(world) and makes the list of meeting corners.
- stopPositions makes its positions, and parkCorners when a stop is an OnBend. stopPosition on an OnBend copies parkCorners through roundedCorner.
- planStops makes a list of lists, one per drawn segment. EditPlan holds lists of lists: each Split's At, each PlanLine's Through, and each BendSplit's At and Points. Every plan builds them new, and finishPlan takes the plan by value.
- withCurves copies layoutOf(world) and copies it again for the after layout. applyPlan makes its keys list, and withCurves its added list.
- keptCurves derives the whole ground of both layouts with groundOf. Ends makes its list of every segment end. For every path point in the park, keptCurves calls is.at and was.at, each of which makes a new list. keepInsides does the same for every corner. This makes two lists per point in a loop over the whole park, five times a frame.

In edits.cpp, checking a plan:

- acceptedPlan lists the solids with solidsOf, which copies parkEntrances and parkBoxes.
- editedBy copies the layout twice. editedBetween derives both grounds with groundOf, and makes pointsChanged's, withFarEnds', and pointsAffected's lists. It builds both surfaceAround surfaces and changedOwners' surface new, and copies the edited segments with their lines.
- isAccepted for AddBox and MoveBox derives the ground into a new GroundLists. It also lists the solids, and piecesNear makes its focus list and a new surfaceAround.
- crossingsOf makes a new list.
- isRestoreAccepted:
    - namesValid makes its names list.
    - applyRestore keeps a std::map.
    - restoredSolids makes a list, and the extra list is new.
    - editedBetween runs as above, the after ground is derived again, and piecesNear runs per box.
- setHours walks world.keys(), a new list of every key in the world.

In src/sim/park/surface.cpp, the builders the checks call:

- buildSurface makes its wanted, built, and asked lists, its shapes, its ends, its Scratch, and the surface it returns.
- Joins makes its points, parents, and segments.
- hideJoined makes its hider bounds, its near and hidden lists, and a new visible surface, which it then moves over the surface it was given. That frees the given surface's lists.
- PointReaches makes its list. pointsNear and pointsAffected make theirs, and surfaceAround makes its bounds list.
- surfaceOf copies the surface's pieces.

In src/legible/preview.cpp, the shop context:

- ghostShop copies parkBoxes of the world and of the candidate.
- connectionPlace copies parkSegments(candidate) to learn whether a carrier is a guest segment.
- footfallAt samples the field with sampleField, which makes its list. The kept preview already holds a KeptFieldSample for that.
- nearestDepot runs with TickBuffers of its own.

In src/render/park_mesh.cpp, the ghost's own work apart from the edit checks:

- appendJoinMarks calls stopPosition for a MovePoint's or InsertPoint's place. On an OnBend, that copies parkCorners. GhostLists already holds a Corners list.

Applying a command takes no buffers. The schema's CommandType.Apply is `void (*)(World &, const void *)`, and src/sim/cycle.cpp's applyCommands is its only caller. stepWorld, makeCandidate, and commitCommands call applyCommands, and all three hold TickBuffers. There are 24 applyCommand definitions: 16 in edits.cpp, PlaceBeacon in src/scenarios/beacons.cpp, PlaceStall and RemoveStall in src/scenarios/stalls.cpp, and five in tests/sim/cycle_test.cpp. Scenario builders and integration tests also call the park's applyCommand directly, outside frames.

The path tool's press asks isAccepted through acceptsStop (src/tools/tools.cpp:142), with a new AddPath copied from the drawn stops. That runs once per press.

## What stays whole-park after kept-checks

Planning still copies the layout and derives the hypothetical ground of the whole park, scans every segment for meetings, and builds the surface around the changed points through whole-layout passes. These no longer allocate once their lists have grown. They are criterion 3's whole-park work, and layered-candidate owns them: it reads only what the edit reaches.

The candidate's copy of the world and the resolvers no feature has moved yet also stay. layered-candidate removes the copy. Each later feature moves its resolvers.

## Why the plan is made once in this feature

layered-candidate's entry used to say a path edit is planned once per changed frame. Planning once changes the same signatures kept-checks changes: isAccepted, applyCommand, pathCrossings, editedSurface, and editedSegments. Making all five call paths allocation-free and then collapsing them to one plan later would rewrite the same functions twice. kept-checks therefore plans once. The acceptance check, applying the edit to the candidate, and the ghost read that one plan.

Rejected: caching the plan in TickBuffers, keyed by the edit and the world. A copy of the world is a different object with the same intent, so the cache key would have to compare intent. The cache would also be state hidden in buffers. Rejected: making Apply take the plan. Commands must stay the edit itself, because replay and undo apply them.

## Why the trace moves into kept-checks

kept-checks is the first feature whose drags the allocation check watches. Its own work runs outside resolution, but the candidate's resolution runs resolvers that no feature has moved yet, and they still allocate. The check watches the whole changed frame and pauses only inside those resolvers, and around the finishers. It tells them apart by a trace that resolution tells as each step starts and ends. unit-surface's plan added that trace for its own watch, so it moves here. unit-surface then only adds its units and resolvers to the steps the check watches.
