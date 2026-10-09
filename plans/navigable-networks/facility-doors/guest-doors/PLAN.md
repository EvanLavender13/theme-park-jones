# Implementation Plan: Guest Doors

## Goal

Give benches and toilet blocks guest front doors, with connectors and walkways no wider than the door's face.

## Approach

ParkDoor gains its face's width, which addDoor takes from the box size. doorsServing adds a guest front door for each bench and toilet block, and findConnections gives each connector the lesser of its kind's path width and its door's width, so the walkway made from the carrier follows. The ghost's walkway strips take each walkway's own width instead of the kind's path width.

## Placement

Decision 0027 places each behavior this feature adds:

- Which faces of which boxes are doors, and each door's width: routes, src/sim/routes/networks.h (parkDoors). routes already owns doors and connectors.
- A connector's width: routes, src/sim/routes/networks.cpp (findConnections). The connector carrier is made there, and its walkway copies the carrier's width.
- The ghost's walkway strip at the walkway's width: render, src/render/park_mesh.h (appendWalkway). The renderer already draws the strips there, from connectorWalkways.

## Tasks

### Task 1: Update the routes spec

Files:
- Modify: `src/sim/routes/SPEC.md:15` and `src/sim/routes/SPEC.md:23-30`

Step 1: In Networks, replace "It also has a carrier for each connector, with its kind's path width (pathWidth, sim/park/SPEC.md)." with "It also has a carrier for each connector, as wide as its kind's path (pathWidth, sim/park/SPEC.md), or as its door's face if that is narrower."

Step 2: In Connectors, replace the list item "A bench and a toilet block have no doors." with "A bench's and a toilet block's front doors serve the guest network."

Step 3: Replace "A ParkDoor has its entity, its face, its position, and the unit direction out of its face." with "A ParkDoor has its entity, its face, its position, the unit direction out of its face, and the width of its face."

### Task 2: Update the capability brief

Files:
- Modify: `plans/navigable-networks/CAPABILITY.md:26-28`

Step 1: After the line "- a depot's front connects to a backstage path;" add "- a bench's front and a toilet block's front connect to a guest path;".

### Task 3: Record the simulation before the change

Files: none.

Step 1: Build tpj_scenarios on main as it stands.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > build/guest-doors-build.log 2>&1; echo $?`
Expected: `0`

Step 2: Write its output for every park in tests/parks but facilities.park. The stress parks stay out, since the debug build steps full.park too slowly.

Run: `mkdir -p build/guest-doors && build/windows-debug/tpj_scenarios.exe --ticks 600 $(ls tests/parks/*.park | grep -v facilities) > build/guest-doors/before.txt 2>/dev/null; echo $?`
Expected: `0`

### Task 4: Give each door its face's width

Files:
- Modify: `src/sim/routes/networks.h:42-51`
- Modify: `src/sim/routes/networks.cpp:135-152`

Step 1: In ParkDoor, after `ParkPoint Out;`, add `double Width = 0.0;`, and make the comment above the struct end "the unit direction out of the face, and the face's width."

Step 2: In addDoor, add `.Width = size.Width` to the ParkDoor it pushes. Both faces of a footprint span its width. Extend addDoor's comment: "The door is as wide as the size."

### Task 5: Give benches and toilet blocks guest front doors

Files:
- Modify: `src/sim/routes/networks.cpp:154-172`

Step 1: In doorsServing, give a box a guest front door when its kind is Shop, Bench, or ToiletBlock, in place of `kind == PathKind::Guest && shop`. Keep the backstage branch as it is.

Step 2: Make its comment read: "The doors serving the kind: an entrance's front, a shop's, a bench's, and a toilet block's front serve guests, and a shop's back and a depot's front serve backstage."

### Task 6: Cap each connector at its door's width

Files:
- Modify: `src/sim/routes/networks.cpp:367-404` and `src/sim/routes/networks.cpp:468-469`
- Modify: `src/sim/park/intent.h:68-69`

Step 1: Drop findConnections' `double width` parameter. Give each connector `.Width = std::min(pathWidth(kind), door.Width)`. Make its comment's first sentence "The connectors of the doors, each as wide as the kind's path or its door, whichever is narrower."

Step 2: In deriveNetwork, call `findConnections(paths, pieces, kind, doors)`.

Step 3: Make pathWidth's comment read: "The width a new segment of the kind takes, in meters, and the widest routes makes the kind's connectors."

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim > build/guest-doors-build.log 2>&1; echo $?`
Expected: `0`

### Task 7: Draw the ghost's walkways at their own width

Files:
- Modify: `src/render/park_mesh.h:157-171`
- Modify: `src/render/park_mesh.cpp:390-432`

Step 1: Give both appendWalkway overloads a `double width` parameter after `points`, and use it wherever they use `pathWidth(kind)` now. Change the header comment's "pathWidth(kind) wide" to "width wide".

Step 2: In appendWalkways, pass `walkway.Width`.

Step 3: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app > build/guest-doors-build.log 2>&1; echo $?`
Expected: `0`

### Task 8: Confirm the criteria

Files: none in the repository. The scratch check lives in build/predict-check.

Step 1: Capture facilities.park with the graph view, and without it.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/facilities.park --graph --capture build/guest-doors/graph.bmp; build/windows-debug/ThemeParkJones.exe --park tests/parks/facilities.park --capture build/guest-doors/park.bmp`
Expected: graph.bmp shows connectors from the bench, the toilet block, and both shops to the guest path, each ending at an anchored node. park.bmp shows the bench's walkway as wide as the bench, and the toilet block's and shops' walkways as wide as the path.

Step 2: Write build/predict-check/doors_check.cpp. It loads the park file its argument names with makeParkSchema, resolves it, samples RouteDistance<PathKind::Guest> with sampleField on the guest network at the place on carrier 4 at distance 10, and prints each distinct source key in order. Compile and run it.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O0 doors_check.cpp ../windows-debug/src/sim/libtpj_sim.a -o doors_check.exe && ./doors_check.exe ../../tests/parks/facilities.park`
Expected: `1 5 6 7 8`

Step 3: Rebuild tpj_scenarios, write the simulation after the change, and compare.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > build/guest-doors-build.log 2>&1; build/windows-debug/tpj_scenarios.exe --ticks 600 $(ls tests/parks/*.park | grep -v facilities) > build/guest-doors/after.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/guest-doors/before.txt build/guest-doors/after.txt; echo $?`
Expected: `0`

Step 4: Run the full checks.

Run: `cmake.exe --build --preset windows-debug > build/guest-doors-build.log 2>&1; echo $?; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh > build/guest-doors-tidy.log 2>&1; echo $?`
Expected: `0`, then `100% tests passed`, then `0`. The build log has no warnings.

Step 5: Play the ghost. Choose Place bench, drag it beside a guest path, and see its walkway strip as wide as the bench.

### Task 9: Commit

Step 1: Commit the feature with the commit-hygiene skill, by pathspec, naming only this feature's files.
