# Implementation Plan: Drawn Edges

## Goal

Make the frontage snap place boxes against the paving as drawn, at every shape it takes, and make doors connect by their distance from that paving.

## Approach

sim/park gains pieceDistance, the distance from a point to a surface piece. routes builds the paved surface once per resolution and measures each door's distance to each segment's pieces and its points' pieces. frontagePose stops reading center lines. It finds the nearest place on the visible edge lines of the served kind, faces the paving there with a rule for corners. If the box doesn't fit there, it tries places along the nearby edge lines and fronts the one nearest the pointer where the box and a 1 m strip around its front and sides clear all paving, using the same separating-axis test the world uses. A scratch probe prints a table of every paving shape for shops and depots, and a sweep over main-street.park checks every pointer near paving.

## Placement

Decision 0027 places each behavior this feature adds:

- pieceDistance: sim/park, the surface component in sim/park/surface.h and surface.cpp. It owns surface pieces, and both routes and the tools need the same measure.
- Reach measured to the drawn paving: sim/routes, the networks component in sim/routes/networks.h and networks.cpp. routes derives every connector there. It reads the surface through sim/park's public header, which lies below it.
- Frontage against the drawn edge lines, the corner rule, and the search for a place that fits: tools, the frontage component in src/tools/frontage.cpp, declared in src/tools/tools.h. It already owns frontagePose.
- The probe and the sweep: build/drawn-edges/, outside the repository's tracked files. They add no behavior to any module.

## Tasks

### Task 1: Record the simulation and the timings before the change

Step 1: Build the scenarios runner and record its output on the small parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/drawn-edges && build/windows-debug/tpj_scenarios.exe --ticks 100 tests/parks/main-street.park tests/parks/warm.park tests/parks/fed.park tests/parks/cut.park tests/parks/supply.park tests/parks/routes.park tests/parks/sketch.park tests/parks/new.park > build/drawn-edges/before.txt 2> /dev/null; echo $?`
Expected: `0`

Step 2: Build the release bench and time both stress parks.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && for p in full winding-path; do build/windows-release/tpj_bench.exe --ticks 10 tests/parks/stress/$p.park; done > build/drawn-edges/bench-before.txt; grep -c "^stage resolution" build/drawn-edges/bench-before.txt`
Expected: `2`

### Task 2: Update the park and routes specs

Files:
- Modify: `src/sim/park/SPEC.md:11`, Paved surface
- Modify: `src/sim/routes/SPEC.md`, Boundary and Connectors

Step 1: Make the src/sim/park/SPEC.md changes FEATURE.md's Spec changes gives: add pieceDistance to the public list, and the sentence at the end of the paragraph starting "parkSurface gives the paved surface".

Step 2: Make the src/sim/routes/SPEC.md changes FEATURE.md's Spec changes gives: the Boundary sentence and the Connectors paragraph.

### Task 3: Update the tools spec

Files:
- Modify: `src/tools/SPEC.md:7`, `src/tools/SPEC.md:52`

Step 1: Make the src/tools/SPEC.md changes FEATURE.md's Spec changes gives: the Boundary sentence and the frontage paragraph.

### Task 4: Update the milestone, the capability brief, and the slice

Files:
- Modify: `plans/navigable-networks/wide-routes/MILESTONE.md`
- Modify: `plans/navigable-networks/CAPABILITY.md:24`
- Modify: `plans/slices/good-paths/SLICE.md:48`

Step 1: In MILESTONE.md's acceptance criterion 2, replace the sentences "A door connects to the segment of the kind it serves whose edge is nearest, when that edge lies within CONNECTION_REACH, 3 m. A segment's edge lies at the distance from the door to its center line, less half its width. Ties go to the lower segment key." with "A door connects to the segment of the kind it serves whose paving, as drawn, lies nearest, when that paving lies within CONNECTION_REACH, 3 m. A segment's paving is its own pieces of the paved surface and the pieces of the points it ends at. Ties go to the segment whose center line is nearer, then to the lower key."

Step 2: Replace criterion 3 with: "3. A shop or depot placed by the frontage snap beside paving of any shape, whether a straight side, a free end, a taper, a junction, or the inside or outside of a corner, sharp or rounded, is accepted wherever it has room and gets a front-door connector. (check: drawn-edges' probe; play; the slice's integration test 2 checks the widest width)"

Step 3: In Features, add after feature 3:

    4. `drawn-edges`: the frontage snap fronts the paving as drawn, at every shape it takes, and doors connect by their distance from the drawn paving. Depends on: features 2 and 3.

Step 4: Replace "None of these features adds a test (docs/testing.md)." with "Only drawn-edges adds a test, for the bug it fixes (docs/testing.md)."

Step 5: In Tuning, replace "Connection reach: 3 m from a segment's edge." with "Connection reach: 3 m from a segment's drawn paving."

Step 6: In CAPABILITY.md, replace "It connects to the path of that kind whose edge is nearest, within a short fixed reach of that edge, at the nearest point of the path's center line:" with "It connects to the path of that kind whose paving, as drawn, is nearest, within a short fixed reach of that paving, at the nearest point of the path's center line:".

Step 7: In MILESTONE.md's Medium, replace "Park intent (decision 0025): routes reads segments with their point keys and widths through parkSegments, doors through parkEntrances, parkBoxes, and footprintOf." with "Park intent (decision 0025): routes reads segments with their point keys and widths through parkSegments, doors through parkEntrances, parkBoxes, and footprintOf, and the drawn paving that reach is measured to through parkSurface and pieceDistance."

Step 8: In MILESTONE.md's Tuning, replace "Junction tolerance: 1 mm, now used only where a connector meets a segment near its end or near another connector, as where shops face each other across a street." with "Junction tolerance: 1 mm, used where a connector meets a segment near its end or near another connector, as where shops face each other across a street, and to tell when segments' paving lies equally near a door."

Step 9: In SLICE.md's Dependencies that are not fields or flows, replace "The paved surface, its rounded inside corners, and its edge line are EB's own rendering. No other capability reads them." with "The paved surface, its rounded inside corners, and its edge line are EB's, published by sim/park as park intent's derived shape (decision 0025). NN reads the surface's pieces to measure each door's reach, so a change to the paving changes which doors connect. EB's tools read it to front boxes."

Step 10: In SLICE.md's Medium, replace "A door connects if it is within reach of the edge of a segment of its kind, not of its center line." with "A door connects if it is within reach of the drawn paving of a segment of its kind, not of its center line."

### Task 5: Declare the interface

Files:
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/tools/tools.h:36-37`, `src/tools/tools.h:216-218`
- Modify: `src/sim/routes/networks.h:18-22`

Step 1: In surface.h, after edgePoints, declare pieceDistance with the comment FEATURE.md's Public interface gives. In surface.cpp, define it to return 0.0 for now. Leave the stub's parameters unnamed, because -Wextra makes an unused parameter an error.

Step 2: In tools.h, include `<numbers>`. After FRONTAGE_SETBACK, add FRONTAGE_CORNER_TURN and FRONTAGE_STEP with their comments. Change FRONTAGE_SETBACK's comment to "A box that fronts paving has its front face this far outside it, in meters." Replace frontagePose's comment with the one FEATURE.md's Public interface gives.

Step 3: In networks.h, replace the comments of JUNCTION_TOLERANCE and CONNECTION_REACH with the ones FEATURE.md's Public interface gives.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build ends with no warnings or errors.

### Task 6: Run the test pass

Step 1: Dispatch the test-writer for the test FEATURE.md lists, as implementing-features describes. The test calls frontagePose, so tpj_integration_tests must link tpj_tools as well as tpj_sim. tpj_tools needs no window, so the tests still run headless. FEATURE.md's Tests entry says so, and the test-writer makes that change in tests/integration/CMakeLists.txt. Pass FEATURE.md, src/tools/SPEC.md, src/sim/routes/SPEC.md, src/sim/park/SPEC.md, and the headers src/tools/tools.h, src/sim/park/surface.h, src/sim/park/edits.h, and src/sim/routes/networks.h. Confirm the test builds and fails because the world refuses the pose, or because frontagePose gives none.

### Task 7: Measure a point's distance to a piece

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: Define pieceDistance. Walk the piece's sides, from each corner to the next, the last back to the first. If the point lies on the left of every side, or on it, as the inside of a counter-clockwise piece does, return 0.0. Otherwise return the least distance from the point to any side: for each side of length at least SAME_POINT, the distance to the nearest place on it, with the place's fraction clamped to 0 and 1. A side shorter than SAME_POINT counts as its first corner. Use the file's dot, ccw, and lengthOf.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings or errors.

### Task 8: Measure reach to the drawn paving

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: In resolvePathNetworks, before the loop over kinds, build `const PavedSurface surface = parkSurface(world);`. Pass `surface.Pieces` to deriveNetwork, which passes it to findConnections along with the PathCarriers' Ends. Include `sim/park/surface.h`.

Step 2: In findConnections, for each door:

- Find the least distance from the door to the pieces of each owner, using pieceDistance on each piece of the network's kind. The pieces are in ascending key order of their owner, so collect them as a list of owners, each with its least distance, in one pass.
- For each carrier, in key order, its paving distance is the least of the distances of three owners in that list: its own key, and the keys of the two points in its Ends entry. If an owner isn't in the list, it adds nothing. If none is, the carrier has no paving and can't connect.
- Let least be the smallest paving distance. If there is none, or least is above CONNECTION_REACH, the door has no connector.
- Of the carriers whose paving distance is at most least plus JUNCTION_TOLERANCE, take the one whose ground line lies nearest the door, measured as today with nearestPlaceOn and groundPoint. The first in key order wins a tie.
- Keep the rest as it is: the JUNCTION_TOLERANCE check on the center-line distance, the connector key check, and the connection to the nearest point on that carrier's ground line.

Replace the function's comment with: "The connectors of the doors, each the given width. A door connects to the path carrier whose paving lies nearest, when that paving lies within reach. A carrier's paving is the pieces its segment owns and those of the points it ends at. Of carriers whose paving lies equally near, the one whose line lies nearest wins, then the lower key."

Step 3: Build and compare the simulation.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 100 tests/parks/main-street.park tests/parks/warm.park tests/parks/fed.park tests/parks/cut.park tests/parks/supply.park tests/parks/routes.park tests/parks/sketch.park tests/parks/new.park > build/drawn-edges/after.txt 2> /dev/null; build/windows-debug/tpj_scenarios.exe --compare build/drawn-edges/before.txt build/drawn-edges/after.txt; echo $?`
Expected: `0`

warm.park's depot door stands exactly 3 m from its backstage path's square end, and the reach includes 3 m, so it stays connected. If the comparison differs, stop and report which door's connection changed.

### Task 9: Front the nearest place on the drawn edge lines

Files:
- Modify: `src/tools/frontage.cpp`

Step 1: Delete LinePlace and nearestOnLine. Rewrite frontagePose's search as follows. Let served be the kind the box's front door serves, as today, size be boxSize(kind), and surface be parkSurface(world).

- The pointer is on paving when pieceDistance gives 0 for some piece of the served kind.
- Each edge line of the served kind is a run of steps, from each of its edge points to the next. Skip steps shorter than MIN_SEGMENT_LENGTH. For each step, find the place on it nearest the pointer, with its fraction clamped to 0 and 1, and its distance. Keep the nearest place over all steps. Walk the edge lines in the surface's order, so a step of a lower-keyed owner wins a tie.
- If there is no step, or the nearest distance is above FRONTAGE_SETBACK plus size.Depth and the pointer is not on paving, return none.
- A step from a to b, with unit direction (dx, dz), has the paving on its left, so its outward direction is (-dz, dx).
- Collect the outward directions of the steps whose nearest place lies within 1e-6 m of the kept place, in the order the walk met them. If there is one, it is the outward direction. If there are two or more, the place is a corner: let n1 and n2 be the first two, and u the unit direction from the place to the pointer, if the pointer lies more than MIN_SEGMENT_LENGTH from it. If u exists, the angle between n1 and n2 is FRONTAGE_CORNER_TURN or less, and u lies between them, the outward direction is u. u lies between them when cross(n1, u) and cross(u, n2) both have the sign of cross(n1, n2), or are 0. Otherwise the outward direction is whichever of n1 and n2 has the larger dot product with u, n1 on a tie or when u doesn't exist.
- The front face's middle lies FRONTAGE_SETBACK from the place along the outward direction, and the box's center lies half of size.Depth beyond that. The box faces opposite the outward direction.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 10: Find the nearest place where the box fits

Files:
- Modify: `src/tools/frontage.cpp`

Step 1: Write a clearance test. For a pose, take its footprint with footprintOf, and make a clearance rectangle from it: the footprint grown by FRONTAGE_SETBACK forward and by FRONTAGE_SETBACK on each side, but not behind. It holds every place within FRONTAGE_SETBACK of the front face. The pose fits when no piece of either kind overlaps the clearance rectangle by more than CONTACT_TOLERANCE. Use the separating-axis test the world uses in src/sim/park/edits.cpp's meets: the axes are the rectangle's two directions and the unit normal of each of the piece's sides at least 1e-9 long. Test only pieces whose bounding rectangle overlaps the clearance rectangle's bounding rectangle, so far pieces cost nothing and a long segment body still counts wherever any part of it lies near. A box fronting a straight side touches the paving with the rectangle's front only, within the tolerance, so it fits.

Step 2: After Task 9 finds the first place and its pose, keep that pose if it fits. Otherwise try other places on the edge lines of the served kind, as follows.

- The candidates are places on the steps Task 9 walked: each step's two ends, and places every FRONTAGE_STEP along it between them. Keep only places that lie within twice size.Width of the first place, and whose step's outward direction lies within a quarter turn of the first place's: its dot product with it is not below -1e-9. The box then never jumps to the far side of a path, and can turn to a side at a right angle, as in a pocket. (Changed during implementation: the sweep showed a positive dot product left out the right-angled side of a pocket that had room.)
- Each candidate's pose fronts it as Task 9 fronts a place that isn't a corner: square to its step, the middle of the front face FRONTAGE_SETBACK out along the step's outward direction. A candidate at a step's end uses that step's outward direction.
- Before testing candidates, collect once the pieces whose bounding rectangle comes within twice size.Width plus the clearance rectangle's diagonal of the first place, and test candidates only against those.
- Sort the candidates by their distance from the pointer, the order Task 9 walked them breaking a tie, and return the pose of the first that fits.
- If none fits, return the first place's pose. The world refuses it if the box overlaps paving there, and the ghost shows that.

Step 3: Work the plan's three inside-corner rows through by reading the code against these numbers before building:

- Block corner at point 7, pointer (-33, 50): the inside edge is an arc of radius 6.5 around (-32, 51). A shop fronting the cross street's north side, z 44.5, fits once its clearance rectangle's left edge reaches the arc's tangent at x -32, so its place is near (-27, 44.5), about 9.8 m from the arc's middle and inside twice the shop's width, 16 m.
- The L's inside corner, pointer (-39, -51): the inside edge is an arc of radius 7 around (-42, -48). A shop fronting the first segment's inner side, z -55, fits with its place near (-47, -55), about 10.2 m from the arc's middle.
- The pocket at point 5, pointer (-8, 48), with main-street.park's boxes removed as Task 12 removes them: the inside edge has radius 4. A shop fronting the street's west side, x -5, fits with its place near (-5, 53.5), and one fronting the cross street's north side fits near (-14, 44.5). Both lie about 8 m from the arc's middle. The first lies nearer the pointer, so the shop fronts the street.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 11: Run the bug test

Step 1: Build and run the integration tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe`
Expected: the last line reads `All tests passed`, followed by the assertion and test case counts.

### Task 12: Probe every paving shape

Files:
- Create: `build/drawn-edges/build.sh`
- Create: `build/drawn-edges/probe.cpp`

Step 1: Copy build/frontage-shapes/build.sh to build/drawn-edges/build.sh. Give it a second argument, the preset, defaulting to windows-debug, and use it in both library paths.

Step 2: Write probe.cpp. Copy commit and the connector listing's use of network.carriers() and nodeAnchor from build/edge-reach/check.cpp. For each row below, the probe loads its park, then commits DeleteBox for every box in it, so each row tests the paving's shape and not the room the park's own shops leave. It then calls frontagePose with the row's kind and pointer, and prints the row's name, the pointer, and the pose. If there is a pose, it prints `accepted` or `REFUSED` from isAccepted with AddBox. If accepted, it commits the AddBox on a copy, finds the new box as the box in parkBoxes with the highest key, and prints `connected` when the served kind's network has a carrier keyed connectorKey(box, Face::Front), and `UNCONNECTED` otherwise. It also prints the gap: the least pieceDistance to a piece of either kind from any of 9 places spread evenly along the footprint's front face, its two front corners included.

The L park is tests/parks/new.park with a guest path added through new points at (-60, -60), (-30, -60), and (-30, -30), both of its segments set to width 10, and the middle point's corner radius set to 12, each by commit. If a commit is refused, print that and stop.

Rows, each tried at all three pointers:

- shop, main-street.park, straight side of the cross street: (-25, 50), (-22, 49.2), (-28.4, 51.5)
- shop, main-street.park, free end past point 10: (87.5, 20.5), (86, 19), (89, 22.5)
- shop, main-street.park, the taper at point 3: (5.5, 110), (6.5, 106), (-5, 112)
- shop, main-street.park, outside of the rounded bend at point 9: (66.9, 45.6), (66.1, 46), (68.4, 44.9)
- shop, main-street.park, inside of the bend: (60, 36), (63, 34.4), (66.3, 35)
- shop, main-street.park, outside of the block corner at point 14: (-40.8, 73.8), (-40.9, 73.8), (-41.1, 73.5)
- shop, main-street.park, inside of the block corner at point 7: (-33, 50), (-34, 52.2), (-32.3, 49)
- shop, main-street.park, the pocket where the street meets the cross street at point 5: (-8, 48), (-9.1, 50), (-7, 47.3)
- shop, the L park, its sharp outside corner: (-22.5, -67), (-24, -68.2), (-21.4, -66)
- shop, the L park, its curved inside corner: (-39, -51), (-40.2, -50), (-38, -52.3)
- depot, main-street.park, straight side of the backstage path: (-20, 60), (-21, 68), (-19.5, 80)
- depot, main-street.park, outside of the rounded corner at point 33: (-19, 29), (-18, 28.2), (-20.1, 30)
- depot, main-street.park, free end past point 32: (-15, 104), (-16.5, 103.5), (-13.8, 104.6)

(Changed during implementation: the pointers first given for the outside of the bend at point 9 and of the block corner at point 14 lay 7 to 10 m from the paving, because those corners' outside edges are rounded with radii of 17.5 m and 9.5 m. They were beyond frontage's reach, so they gave no pose. The rows now use pointers about 3 m from the drawn paving.)

Then add one row by hand, for reach. In the L park, the sharp outside corner V lies at (-25, -65). Add a shop facing (-0.7071, 0.7071) with its center at V + 5.5 · (0.7071, -0.7071), so its front door stands 2.5 m beyond the corner. Print whether it is accepted and connected.

Step 3: Build and run it.

Run: `sh build/drawn-edges/build.sh probe && build/drawn-edges/probe.exe`
Expected: every row ends with `accepted connected` and a gap of at least 0.99. If any row is refused, has no pose, or is unconnected, stop and report the table.

### Task 13: Sweep main-street.park and time frontage

Files:
- Create: `build/drawn-edges/sweep.cpp`

Step 1: Write sweep.cpp. Load main-street.park and commit DeleteBox for every box. For each kind, shop and depot, and each pointer on a 2 m grid from -128 to 128 on both axes, call frontagePose. For each pose, classify it as accepted and connected, accepted and unconnected, refused while overlapping paving, or refused otherwise. A refused pose overlaps paving when the middle of any side of its footprint, any corner, or its center lies on a piece of either kind, by pieceDistance. Print each kind's four counts, and print every refused pointer that overlaps paving.

Then load tests/parks/stress/full.park. Time frontagePose for a shop at each pointer on a 10 m grid from -120 to 120, with std::chrono::steady_clock, and print the average milliseconds per call.

Step 2: Build it against windows-release and run it.

Run: `cmake.exe --build --preset windows-release --target tpj_tools && sh build/drawn-edges/build.sh sweep windows-release && build/drawn-edges/sweep.exe`
Expected: it takes a few minutes. Both kinds print `accepted unconnected 0`. The average time is under 5 ms. Report every count and every listed pointer. If any accepted pose is unconnected, or the average is 5 ms or more, stop and report.

### Task 14: Time the stages after the change

Step 1: Rebuild the release bench and time the stress parks again.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && for p in full winding-path; do build/windows-release/tpj_bench.exe --ticks 10 tests/parks/stress/$p.park; done > build/drawn-edges/bench-after.txt; grep "stage resolution\|stage preview" build/drawn-edges/bench-before.txt build/drawn-edges/bench-after.txt`
Expected: two resolution lines and two preview lines from each file. Report the medians side by side.

### Task 15: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest ends with `100% tests passed`, and tidy ends with `tidy: clean.`

Step 2: Rebuild the app the user plays.

Run: `cmake.exe --build --preset windows-release`
Expected: the build ends with no warnings or errors. build/windows-release/ThemeParkJones.exe is newer than every source change in this feature.

## Fixes from play

The user played the first version in the windows-release build. On the outside of a curve, a walkway slanted from the door toward the center line. On the inside of a curve, the box jumped to the curve's ends instead of sliding along it. The tasks below fix both, and the code review's one finding. Tasks 1 to 15 are done.

### Task 16: Update the specs and the plan documents for the fixes

Files:
- Modify: `src/sim/park/SPEC.md`, `src/sim/park/surface.h`
- Modify: `src/sim/routes/SPEC.md`
- Modify: `src/tools/SPEC.md`
- Modify: `plans/navigable-networks/wide-routes/MILESTONE.md:18`
- Modify: `plans/effortless-building/path-network/MILESTONE.md:47`

Step 1: Make the changes FEATURE.md's Spec changes gives that the first pass didn't: the park walkway sentence, the routes connector sentences and connectorWalkways sentence, and the tools paragraph as it now reads. In surface.h, change Walkway's comment "where it meets the segment's ground line" to "where it ends inside the segment's paving".

Step 2: In wide-routes' MILESTONE.md criterion 6, replace "This capability's brief describes nodes from points and reach from a segment's edge." with "This capability's brief describes nodes from points and reach to a segment's drawn paving."

Step 3: In path-network's MILESTONE.md Medium, replace "Frontage: the place tools read published segments' center lines and widths, not the drawn surface." with "Frontage: the place tools read the paved surface's edge lines and pieces, as wide-routes' drawn-edges changed them to."

### Task 17: Run connectors straight out of the door to the paving

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: In findConnections, after nearestLine chooses the carrier and before the connection is built, find the entry: the least distance e along the ray from door.At in direction door.Out at which the ray enters a piece of the kind. For each such piece, clip the ray's stretch [0, CONNECTION_REACH + JUNCTION_TOLERANCE] against each side of length at least 1e-9: the piece's inside lies to the left of each counter-clockwise side, as surface.cpp's insideOf measures it, with no depth. The entry is the start of the clipped stretch, when it isn't empty. Keep the least over all pieces.

Step 2: Build the connector:

- If there is no entry, e is 0 (the door stands on paving), or the direction from the door to the meeting nearestLine found has a dot product with door.Out of at least 1 - 1e-9, build the connector exactly as today: two points, the door and that meeting.
- Otherwise let entry be door.At + door.Out · e. Find where the chosen carrier's line lies nearest entry, with nearestPlaceOn and groundPoint. The connector has three points: the door at distance 0, entry at distance e, and that ground point at distance e plus its distance from entry. The connection's PathDistance is that place's distance. If the ground point lies within JUNCTION_TOLERANCE of entry, use the two points door and entry, with entry as the meeting.

Step 3: In connectorWalkways, set Meets as follows. If the carrier has two points, use its last point, as today. If it has three, let entry be its second point, last its third, and h half its width. Meets is entry + Out · min(h, distance from entry to last).

Step 4: Build and compare the simulation.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 100 tests/parks/main-street.park tests/parks/warm.park tests/parks/fed.park tests/parks/cut.park tests/parks/supply.park tests/parks/routes.park tests/parks/sketch.park tests/parks/new.park > build/drawn-edges/after.txt 2> /dev/null; build/windows-debug/tpj_scenarios.exe --compare build/drawn-edges/before.txt build/drawn-edges/after.txt; echo $?`
Expected: `0`. Every door in these parks faces its meeting straight on, so each connector keeps its two points.

### Task 18: Draw the ghost's walkways as the park draws them

Files:
- Modify: `src/render/park_mesh.cpp`, appendWalkways

Step 1: In appendWalkways, for each kind, draw each walkway connectorWalkways gives, instead of each connector's points: a ribbon of two points, the walkway's Door and Meets, with the walkway's Out as the face normal, in the given color. Its distances are 0 and the length between the two. A ghost's walkway then shows the strip the placed box will get, not the connector's turn inside the paving.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build ends with no warnings or errors.

### Task 19: Back a box off the paving until it fits

Files:
- Modify: `src/tools/tools.h`
- Modify: `src/tools/frontage.cpp`

Step 1: In tools.h, include `sim/routes/networks.h`, and add FRONTAGE_PUSH after FRONTAGE_STEP with the comment FEATURE.md's Public interface gives.

Step 2: Replace the clearance test. A pose fits when:

- its footprint overlaps no nearby piece by more than CONTACT_TOLERANCE, by the separating-axis test fits uses today on the footprint itself;
- the strip in front of its front face, as wide as the face and FRONTAGE_SETBACK deep, overlaps no nearby piece by more than CONTACT_TOLERANCE, by the same test;
- each of the front face's two corners lies at least FRONTAGE_SETBACK - CONTACT_TOLERANCE from every nearby piece, by pieceDistance.

Together these keep every part of the front face at least FRONTAGE_SETBACK from paving. Keep the bounds check that skips far pieces, using the bounds of the footprint and the strip grown by FRONTAGE_SETBACK.

Step 3: Write a function that, for a place and an outward direction, gives the pose fronting it moved straight out by the least push that fits, or none. Try pushes of 0, FRONTAGE_STEP, 2 · FRONTAGE_STEP, and so on up to FRONTAGE_PUSH, with FRONTAGE_PUSH itself last. At the first that fits, if it isn't 0, bisect 10 times between it and the one before, keeping the end that fits, and use the fitting end. A pose pushed by p lies p farther along the outward direction than poseFronting gives.

Step 4: In frontagePose, use it for the first place, then for each candidate in order. Return the first pose it gives. If none, return the first place's unpushed pose. Widen piecesNear's reach by FRONTAGE_PUSH.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 20: Turn smoothly along the inside of a curve

Files:
- Modify: `src/tools/frontage.cpp`

Step 1: Give each step the outward direction it has at each of its two ends:

- Find the step before it, the one whose end lies within SAME_PLACE of its start, and the step after it, the one whose start lies within SAME_PLACE of its end. Search only when a step is the first place's step, so this costs one pass over the steps.
- A corner between two steps turns away from the paving when the later step's direction has a positive dot product with the earlier step's outward direction. It is gentle when the angle between their outward directions is FRONTAGE_CORNER_TURN or less.
- At an end where the corner with the neighbouring step is gentle and turns away from the paving, the outward direction there is the normalized sum of the two steps' outward directions. At any other end, it is the step's own.

Step 2: When the first place lies inside a step, at fraction t of its length, its outward direction is the normalized blend (1 - t) · start direction + t · end direction. When the first place lies at a corner that is gentle and turns away from the paving, its outward direction is the normalized sum of the two steps' outward directions. Every other case keeps Task 9's rule. Candidates keep their step's own outward direction.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 21: Check the probe, the walkways, the sweep, and a slide along curves

Files:
- Modify: `build/drawn-edges/probe.cpp`
- Create: `build/drawn-edges/slide.cpp`

Step 1: In the probe, for each accepted row, find the new box's front walkway in connectorWalkways and print the angle in degrees between Meets - Door and Out. Count a row as failing if it is 1° or more.

Run: `sh build/drawn-edges/build.sh probe && build/drawn-edges/probe.exe | tail -1`
Expected: `failures 0`

Step 2: Write slide.cpp. For each of main-street.park's corners at points 9, 7, and 14, with its boxes removed, and for the L park's corner, take the corner's arc with arcOf. For the inside, place the pointer on the circle about the arc's center whose radius is the arc's radius less half the segments' width less 3 m. For the outside, use the arc's radius plus half the width plus 3 m, but leave out the L's outside, which is a sharp corner. Sweep the pointer around the circle in 0.5° steps, from 10° before the direction of the arc's first touch to 10° past its second. At each step, call frontagePose for a shop and record the pose. Print, for each sweep, its number of steps without a pose, the largest move of the pose's center between consecutive steps, and the largest turn of its facing in degrees.

Step 3: Build and run it.

Run: `sh build/drawn-edges/build.sh slide && build/drawn-edges/slide.exe`
Expected: seven sweeps. Each prints 0 steps without a pose, a largest move under 0.5 m, and a largest turn under 5°. If one doesn't, stop and report the sweep.

Step 4: Rerun the sweep.

Run: `cmake.exe --build --preset windows-release --target tpj_tools && sh build/drawn-edges/build.sh sweep windows-release && build/drawn-edges/sweep.exe | grep -v "otherwise at"`
Expected: both kinds print `accepted unconnected 0` and `refused overlapping paving 0`, and the average time is under 5 ms.

### Task 22: Confirm the fixes

Step 1: Run the full checks, the bug test, and the release build.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh && cmake.exe --build --preset windows-release`
Expected: no warnings, `100% tests passed`, `tidy: clean.`, and the release build ends with no warnings or errors.

Step 2: Time the stress parks again, as Task 14 does, and report the medians beside Task 1's.

Implementation notes for Tasks 19 to 21. The slide showed three things the tasks above didn't foresee, and the code differs from them as follows. Blending at each corner left 5.5° steps on the inside of curves, because the nearest place jumps from one piece to the next there. The box now fronts a weighted blend of the nearby pieces of the same inside curve instead. Blending on the outside of curves tilted boxes on straight sides near a curve, so outside corners keep Task 9's rule. A push alone made the box jump 1.4 m beside a junction, so the box takes the least move that fits, straight out or along the edge, before it tries other places. The slide sweeps run 3° past each curve's ends, not 10°, because 10° reached the junction at point 22, where the box rightly flips at a sharp inside corner. The code review then found that a blended place on a tight inside curve could push a shop's door beyond reach, so a pose now fits only when its door lies within CONNECTION_REACH of served paving. The probe and slide gained a narrow L (width 3, corner radius 7). In that corner an 8 m shop can't stand at the curve's middle, so it shifts between the places that fit and flips once between the arms. Fallback places now first face as a pointer straight out from them would make them face, and otherwise square to their step, as tools' spec says. Its slide is reported, not held to the 0.5 m and 5° limits, and neither is the narrow L's sharp outside corner, where the box rightly flips to the other side.

### Task 23: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/park/SPEC.md, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/routes/SPEC.md, src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/tools/SPEC.md, src/tools/tools.h, src/tools/frontage.cpp, src/render/park_mesh.cpp, the test pass's files under tests/integration/, including tests/integration/CMakeLists.txt, plans/navigable-networks/wide-routes/MILESTONE.md, plans/navigable-networks/CAPABILITY.md, plans/slices/good-paths/SLICE.md, plans/effortless-building/path-network/MILESTONE.md, and plans/navigable-networks/wide-routes/drawn-edges/. Use the subject `Tools: Front and connect boxes to the paving as drawn`.
