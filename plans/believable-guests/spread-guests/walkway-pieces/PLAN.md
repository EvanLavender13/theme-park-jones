# Implementation Plan: Walkway Pieces

## Goal

Publish each walkway's piece on its own and each walkway with its connector's key, and check once that the outline guests will walk inside closes.

## Approach

addWalkway in src/sim/park/surface.cpp already builds a walkway's piece and then its three edge lines. The piece-building moves into the public walkwayPiece, and addWalkway calls it and builds the edge lines from the piece's corners, so parkSurface's output can't change. In src/sim/routes/networks.cpp, the body of connectorWalkways becomes keyedWalkways, which records each carrier's key with its walkway, and connectorWalkways keeps only the walkways. The outline check is a scratch program in build/, built against the windows-debug sim library as earlier scratch checks were, and it checks the outline the way this feature's RESEARCH.md describes.

## Placement

Decision 0027 places each behavior this feature adds:

- walkwayPiece: sim/park, src/sim/park/surface.h. sim/park owns the paved surface and already builds every walkway's piece inside parkSurface.
- keyedWalkways and KeyedWalkway: sim/routes, src/sim/routes/networks.h. routes derives connectors and already makes their walkways in connectorWalkways.
- The outline check: a scratch program in build/paving-holes/, never committed. It runs once, and docs/testing.md allows no permanent test of one module's internals.

## Tasks

### Task 1: Record what the park looks like and simulates before the change

Files: none.

Step 1: Build the app and tpj_scenarios on the unchanged tree.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_scenarios`
Expected: the build succeeds with no warnings.

Step 2: Save the scenario output and a capture.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 600 tests/parks/*.park > build/windows-debug/walkway-before.txt && build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --capture build/windows-debug/walkway-before.bmp`
Expected: the text file starts with `scenario walkers tick 0 hash`, and the app prints `GPU driver: vulkan` and exits with status 0.

### Task 2: Describe walkwayPiece in the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: In the list of public functions under Boundary, the one that begins "segmentLine, cornerRadiusFor, arcOf", insert "walkwayPiece, " after "pieceDistance, ".

Step 2: At the end of the walkway paragraph under Paved surface, the one that begins "parkSurface can also add walkways", add this sentence:

    walkwayPiece gives the piece parkSurface adds for one walkway, given the park's segments as parkSegments gives them, or nothing when parkSurface adds nothing for it.

### Task 3: Describe keyedWalkways in the routes spec

Files:
- Modify: `src/sim/routes/SPEC.md`

Step 1: In Boundary, replace "and their walkways through connectorWalkways" with "and their walkways through connectorWalkways and keyedWalkways".

Step 2: At the end of Connectors, after the paragraph that begins "connectorWalkways gives a walkway", add this paragraph:

    keyedWalkways gives the same walkways as connectorWalkways, in the same order, each with the key of the connector it was made for.

### Task 4: Give a walkway's piece on its own

Files:
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park/surface.cpp`

Step 1: In surface.h, add `#include <optional>` to the standard includes. After the line declaring WALKWAY_OVERLAP, add the declaration of walkwayPiece exactly as FEATURE.md's Public interface gives it, with its comment.

Step 2: In surface.cpp, define walkwayPiece outside the anonymous namespace, after pieceDistance. Its body is addWalkway's up to and including the piece it builds. The joins check runs over `segments` instead of `ground.Segments`. Where addWalkway returns nothing, walkwayPiece returns std::nullopt. It returns the SurfacePiece that addWalkway pushes today: kind, joined segment, and the corners right, farRight, farLeft, and leftDoor, count 4. LEAST_WALKWAY_LEAN and the helpers it uses stay where they are in the anonymous namespace, which comes earlier in the file.

Step 3: Rewrite addWalkway to call `walkwayPiece(ground.Segments, walkway)`. If that gives nothing, it returns. Otherwise it pushes the piece onto surface.Pieces, and adds one edge line for each pair of corners (0, 1), (1, 2), and (2, 3), owned by the piece's Owner with the piece's Kind, exactly as its loop does today. Keep its comment, which still says what it adds. surface.h declares walkwayPiece, so addWalkway can call it although its definition comes later in the file.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Give each walkway with its connector's key

Files:
- Modify: `src/sim/routes/networks.h`
- Modify: `src/sim/routes/networks.cpp`

Step 1: In networks.h, after the declaration of connectorWalkways, add KeyedWalkway and keyedWalkways exactly as FEATURE.md's Public interface gives them, with their comments.

Step 2: In networks.cpp, rename connectorWalkways's definition to keyedWalkways, returning `std::vector<KeyedWalkway>`. Where it pushes a Walkway, push `KeyedWalkway{.Connector = carrier.Key, .Way = Walkway{...}}` with the same Walkway it builds today.

Step 3: Define connectorWalkways after keyedWalkways. It calls keyedWalkways(world, kind), reserves the result's size, and returns each entry's Way in order.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 6: Show that nothing changed

Files: none.

Step 1: Build, run the scenarios over the parks, and compare with Task 1's output.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 600 tests/parks/*.park > build/windows-debug/walkway-after.txt; build/windows-debug/tpj_scenarios.exe --compare build/windows-debug/walkway-before.txt build/windows-debug/walkway-after.txt; echo "status $?"`
Expected: the build's output and tpj_scenarios's timing lines come first. `--compare` prints no difference line, and the run ends with `status 0`.

Step 2: Capture main-street.park and compare it with Task 1's capture.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --capture build/windows-debug/walkway-after.bmp && cmp build/windows-debug/walkway-before.bmp build/windows-debug/walkway-after.bmp && echo identical`
Expected: `GPU driver: vulkan`, then `identical`.

### Task 7: Check that the outline guests walk inside closes

Files:
- Create, never committed: `build/paving-holes/build.sh`
- Create, never committed: `build/paving-holes/closure.cpp`

Step 1: Copy build/predict-check/build.sh to build/paving-holes/build.sh. It builds `<name>.cpp` in its own directory against build/windows-debug's libtpj_tools.a and libtpj_sim.a.

Step 2: Write build/paving-holes/closure.cpp. It reads only public headers: sim/park_schema.h, sim/save.h, sim/world.h, sim/park/surface.h, and sim/routes/networks.h. For each path on its command line:

- Read the file, load it with `loadWorld(makeParkSchema(), text)`, and call resolveWorld. If loading throws, print `<path>: cannot load: <message>` and go on to the next path.
- Take the walkways `connectorWalkways(world, PathKind::Guest)` and the surface `parkSurface(world, walkways)`. Collect each edge line of Kind PathKind::Guest as a polyline of its edgePoints. For each walkway that walkwayPiece(parkSegments(world), walkway) gives a piece for, add its door end, the line from the piece's last corner to its first.
- A line's end is met when it lies within 1e-5 m of a point on another line, on any of its straight runs. Count each start and end that isn't met, and print the first few with their positions.
- Print `<path>: <lines> lines, <count> open ends`.

Step 3: Build the check and run it over every park.

Run: `build/paving-holes/build.sh closure && build/paving-holes/closure.exe tests/parks/*.park tests/parks/stress/*.park parks/*.park`
Expected: one line for each park, each ending in `0 open ends`.

If any park has open ends or can't load, stop. That is a deviation of Major severity: report the lines to the user.

### Task 8: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 9: Commit

Files: none.

Step 1: Stage only this feature's paths: src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/park/SPEC.md, src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/sim/routes/SPEC.md, and plans/believable-guests/spread-guests/walkway-pieces/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Publish walkway pieces and keyed walkways`.
