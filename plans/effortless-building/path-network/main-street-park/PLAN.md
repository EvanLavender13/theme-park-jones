# Implementation Plan: Main Street Park

## Goal

Draw tests/parks/main-street.park through the tools, check that it looks and connects as FEATURE.md says, and settle the milestone's tuning values.

## Approach

A scratch program, kept in build/main-street/ and never checked in, stands in for the player's hand. It drives a ToolState from tools/tools.h on a new park, as the app does: pointer samples at a zoomed-in view's scale, presses, and releases. Each edit a release gives is committed with makeCandidate, which applies it and resolves without stepping, so no guest arrives and the tick stays 0. Where a snap should place a stop, the program aims a few tenths of a meter off, and it checks that the snap pulled the stop onto the intended place. It drags handles while reading the ghost's value from tentativeEdit, as a player watches the ghost. It writes the park with saveWorld.

## Placement

Decision 0027 places each behavior this feature adds:

- The park file: tests/parks/main-street.park. docs/conventions.md puts checked-in park files in tests/parks/, where the park world checks and the cross-build check read them.
- The drawing program: build/main-street/, outside the repository's tracked files. It is a one-off stand-in for the player, like the scratch checks of earlier features, and it adds no behavior to any module.

No code changes, so no module or component gains a behavior.

## Tasks

### Task 1: Create the branch

Run: `git switch -c main-street-park`
Expected: `Switched to a new branch 'main-street-park'`

### Task 2: Set up the drawing program

Files:
- Create: `build/main-street/build.sh`
- Create: `build/main-street/draw.cpp`

Step 1: Copy build/undo-check/build.sh to build/main-street/build.sh. It compiles a named .cpp against the windows-debug libtpj_tools.a and libtpj_sim.a.

Step 2: Write draw.cpp with these parts. All positions are ground positions in meters, x east and z toward the entrance.

- Start from `makeNewPark(1)` (sim/park_schema.h), resolved with resolveWorld. Each stroke below starts with selectTool for the tool it names. "Finish" means clicking the last stop again.
- move(tool, to): the hand travels from the last pointer position to `to` in steps of 0.5 m, calling movePointer at each step and at `to` with `PointerSample{ground, 0.1, false}`. 0.1 m per pixel is a view zoomed in to draw: the camera stands 90 m away with a vertical field of view of 0.9 radians, in a window 900 pixels tall. When main-street.park opens, the camera frames the whole park from about 190 m, at about 0.2 m per pixel, so snaps reach twice as far there.
- commit(edit, name): if `isAccepted(world, edit)` is false, print `refused: <name>` and exit with status 1. Otherwise replace the world with `makeCandidate(world, queue)`, where the queue holds the edit by queueEdit, and print `ok: <name>`.
- click(tool, at, name): move to `at`, pressPointer, then releasePointer. If the release gives an edit, commit it.
- expect(at, wanted, name): if the last stop's position lies more than 1e-9 from `wanted`, print `missed snap: <name>` and exit with status 1. Call it after each click marked "snaps" below. Take the position from the back of stopPositions(world, AddPath{kind, tool.Drawn}), which also places a stop that repeats an earlier one.
- dragHandle(kind, near, target, name): with Edit path chosen, take the handle of that kind in editHandles that lies nearest `near`. Press at its At. Step the pointer 0.002 m at a time along the line through the handle's point and its At, in whichever direction makes the value in tentativeEdit grow, found by trying 0.5 m each way. Release at the first step where the value reaches `target`, and commit the edit. If it hasn't reached it after 60 m, print `handle stuck: <name>` and exit with status 1. The value is the Radius of SetCornerRadius or SetEdgeRadius, or the Length of SetTaperLength.
- At the end, print `points <n> segments <n> guest <n> backstage <n> shops <n> depots <n>` from parkPathPoints, parkSegments, and parkBoxes, and write saveWorld's text to the path given as the first argument, in binary mode.

Step 3: Build it.

Run: `sh build/main-street/build.sh draw`
Expected: no output, and build/main-street/draw.exe exists.

### Task 3: Draw the main street

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: Add these strokes. Positions marked "snaps" aim off on purpose. Call expect after each with the position in brackets.

- Guest path. Click (0.3, 103.2), snaps to the entrance path's end [(0, 103)]. Click (0.2, 42.6), snaps straight on at 60 m [(0, 43)]. Click (0, 43) again to finish. Name: street.
- Edit path. Drag the street's side: press at (1.5, 73), move to (5, 73), release. The width becomes 10. Name: street width.
- dragHandle(Taper, (0, 106), 14). Name: entrance taper.

Step 2: Build and run it.

Run: `sh build/main-street/build.sh draw && build/main-street/draw.exe build/main-street/out.park`
Expected: three `ok:` lines, then `points 3 segments 2 guest 2 backstage 0 shops 0 depots 0`.

### Task 4: Draw the side paths

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: Add these strokes after Task 3's.

- Guest path. Click (0.3, 42.7), snaps to the street's end [(0, 43)]. Click (-40.4, 43.3), snaps at 90° and 40 m [(-40, 43)]. Finish. Name: west arm.
- Guest path. Click (0.3, 43.4), snaps to (0, 43) [(0, 43)]. Click (65.4, 43.3), snaps straight on from the west arm at 65 m [(65, 43)]. Click (84.6, 23.5), snaps at 45° and 28 m [(65 + 28/√2, 43 − 28/√2)]. Finish. Name: east arm.
- dragHandle(Radius, (65, 43), 16). Name: east bend.
- Guest path. Click (-4, 73), on the street's paving, which snaps onto the street [(0, 73)]. Click (-40.3, 73.4), snaps at 90° and 40 m [(-40, 73)]. Click (-39.6, 43.4), snaps to the west arm's end [(-40, 43)]. Finish. Name: west block.
- dragHandle(Radius, (-40, 73), 8) and dragHandle(Radius, (-40, 43), 8). Names: block north, block south.

Step 2: Build and run it, as in Task 3.

Expected: nine `ok:` lines in all, then `points 8 segments 8 guest 8 backstage 0 shops 0 depots 0`.

### Task 5: Draw the loop

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: Add these strokes after Task 4's.

- Guest path. Click (28, 55). Click (52.3, 55.2), snaps along the world's x axis at 24 m [(52, 55)]. Click (51.6, 31.4), snaps at 90° and 24 m [(52, 31)]. Click (28.3, 30.7), snaps at 90° and 24 m [(28, 31)]. Click (27.6, 54.8), snaps to the first stop [(28, 55)]. Before the finishing click, print `crossings <n>`, where n is the number of places pathCrossings gives for the tentative AddPath. Click (28, 55) again to finish. Name: loop.
- dragHandle(Radius, corner, 8) at each corner (28, 55), (52, 55), (52, 31), and (28, 31). Names: loop corners.

Step 2: Build and run it.

Expected: `crossings 2` before `ok: loop`, fourteen `ok:` lines in all, then `points 14 segments 16 guest 16 backstage 0 shops 0 depots 0`. The crossings split the east arm and two sides of the loop.

### Task 6: Shape the edges

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: Add these handle drags after Task 5's. The inside corners of the street's junctions get curbs, and the outside edge of each rounded corner gets the corner's radius plus half the width, so it runs parallel to the inside edge.

- Edge handles at the T, nearest (6.5, 46) and (-6.5, 46), radius 4. Names: T east curb, T west curb.
- Edge handles at the west junction, nearest (-6, 76) and (-6, 70), radius 2. Names: junction north curb, junction south curb.
- Edge handles nearest (-43, 76) and (-43, 40), radius 9.5. Names: block outer edges.
- Edge handle nearest (66, 46), radius 17.5. Name: bend outer edge.
- Edge handles nearest (25, 58), (55, 58), (55, 28), and (25, 28), radius 9.5. Names: loop outer edges.

Step 2: Build and run it.

Expected: twenty-five `ok:` lines in all, then the same counts as Task 5.

### Task 7: Draw the backstage path and place the depot

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: Add these strokes after Task 6's.

- Backstage path. Click (-15, 100). Click (-15.3, 33.4), snaps along the world's z axis at 67 m [(-15, 33)]. Click (15.4, 33.2), snaps at 90° and 30 m [(15, 33)]. Click (15.3, 100.4), snaps at 90° and 67 m [(15, 100)]. Finish. Name: backstage.
- dragHandle(Radius, (-15, 33), 4) and dragHandle(Radius, (15, 33), 4). Names: backstage corners.
- Edge handles nearest (-17, 31) and (17, 31), radius 5. Names: backstage outer edges.
- Place depot. Click (0, 27.5). The frontage snap turns it to face the backstage path. Name: depot.

Step 2: Build and run it.

Expected: thirty-one `ok:` lines in all, then `points 18 segments 19 guest 16 backstage 3 shops 0 depots 1`.

### Task 8: Place the shops

Files:
- Modify: `build/main-street/draw.cpp`

Step 1: With Place shop, click at each position below. The frontage snap turns each shop to face the street, with its front face 1 m from the paving.

- East row: (9.5, z) for z = 97, 88, 79, 70, 61, and 52.
- West row: (-9.5, z) for z = 97, 88, 79, 61, and 52, mirroring the east row. The row leaves a gap where the side path leaves, at z = 73.

Names: shop east <z>, shop west <z>.

Step 2: Build and run it.

Run: `sh build/main-street/build.sh draw && build/main-street/draw.exe tests/parks/main-street.park`
Expected: forty-two `ok:` lines in all, then `points 18 segments 19 guest 16 backstage 3 shops 11 depots 1`.

Step 3: Confirm the file is a fresh park.

Run: `head -4 tests/parks/main-street.park; grep -c "^\[guest\]" tests/parks/main-street.park`
Expected: `tpj-park 1`, `seed 1`, `tick 0`, a next-key line, then `0`, since a park with no guests has no [guest] section.

### Task 9: Look at the park

Step 1: Build the app and capture the park, plain and with the graph view.

Run: `cmake.exe --build --preset windows-debug > /dev/null; cd build/windows-debug && timeout 60 ./ThemeParkJones.exe --park ../../tests/parks/main-street.park --capture ../main-street/plain.bmp; timeout 60 ./ThemeParkJones.exe --park ../../tests/parks/main-street.park --graph --capture ../main-street/graph.bmp; echo $?`
Expected: `0`

Step 2: Convert both captures to PNG in the scratchpad with Python's PIL, and view them with the Read tool.

Step 3: Check plain.png against FEATURE.md's criterion 3, and graph.png against criterion 4. If a detail is wrong, such as a curb that overlaps a shop or an outside edge that isn't parallel, change that stroke's positions or target in draw.cpp, run Task 8's command again, and capture again. Report each change. If a fix needs code, stop: that is a deviation.

### Task 10: Settle the tuning values

Files:
- Modify: `plans/effortless-building/path-network/MILESTONE.md`
- Modify: `plans/slices/good-paths/SLICE.md`

Step 1: For each value in the milestone's Tuning section, note what the drawing showed: the 3 m and 2 m defaults for the side paths and the backstage path, the 10 m street within the 1.5 m to 16 m limits, the sharp defaults for corners and edges, the natural taper length, the snaps that caught each aimed-off click at 0.1 m per pixel, which the user's play checks at other zooms, the 1 m setback of the rows, and Ctrl. If a value should change, stop and bring it to the user, because a fixed value takes shape control from the player.

Step 2: Make the plan changes listed in FEATURE.md's Spec changes.

Step 3: Check each value in the Tuning section against the constants in src/tools/tools.h (SNAP_CATCH_PIXELS, SNAP_RELEASE_PIXELS, FRONTAGE_SETBACK, MIN_PATH_WIDTH, MAX_PATH_WIDTH) and src/sim/park/intent.h (pathWidth, DEFAULT_CORNER_RADIUS, DEFAULT_EDGE_RADIUS, DEFAULT_TAPER_LENGTH), and the Ctrl test in snapsOff in src/app/input/input_map.cpp.

### Task 11: Run the checks

Step 1: Build, test, and tidy.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c "warning:"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed, 0 tests failed out of 107`, then `tidy: clean.`

Step 2: Build windows-release for the user to play criterion 6.

Run: `cmake.exe --build --preset windows-release 2>&1 | grep -c "warning:"`
Expected: `0`

### Task 12: Review and commit

Step 1: Stage only this feature's paths: `git add tests/parks/main-street.park plans/effortless-building/path-network/MILESTONE.md plans/slices/good-paths/SLICE.md plans/effortless-building/path-network/main-street-park/`. Don't use `git add -A`, because other sessions share the index, and leave the untracked parks in parks/ alone.

Step 2: Dispatch the reviewer through the reviewing skill with the staged diff, FEATURE.md, and docs/principles.md as context. Present its findings verbatim.

Step 3: After the user has dealt with the findings, commit with the commit-hygiene skill. Subject: `Parks: Add the main street park, drawn with the tools`.
