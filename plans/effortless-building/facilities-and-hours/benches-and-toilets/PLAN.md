# Implementation Plan: Benches and Toilets

## Goal

The player places benches and toilet blocks with Place bench and Place toilet, fronting guest paving as shops do, with no doors yet and the simulation of every existing park unchanged.

## Approach

BoxKind gains Bench and ToiletBlock, with footprints, saved names, heights, and colors. Every other box behavior, from validity, saves, and history to moving, deleting, picking, and drawing ghosts, already works for any kind. Routes changes so that only depots get a backstage front door, which leaves benches and toilet blocks with no doors. The tools gain two place tools that reuse the shop's frontage, facing guest paving.

## Placement

Decision 0027 places each behavior this feature adds:

- The bench and toilet block kinds, their saved names, and their footprints: sim/park, src/sim/park/intent.h. It owns box intent and the public sizes that routes, the tools, and render measure with.
- No doors for benches and toilet blocks: sim/routes, src/sim/routes/networks.cpp (doorsServing). It decides which faces of which entities have doors.
- Place bench and Place toilet, and fronting guest paving: tools, src/tools/tools.h with tools.cpp and frontage.cpp. They own every tool and the frontage snap.
- Each kind's height and color: render, src/render/park_mesh.h, which holds every kind's look.
- The two buttons: app UI, src/app/ui/tool_panel.cpp, which lists the tools.
- A park that holds both kinds: tests/parks/facilities.park, where the park world checks find every checked park.

## Tasks

### Task 1: Record the simulation before the change

Files:
- Create, scratch: `build/benches-and-toilets/before.txt`

Step 1: Build tpj_scenarios on main as it is, and write its output for the checked-in parks.

Run: `mkdir -p build/benches-and-toilets && cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/*.park tests/parks/stress/winding-path.park > build/benches-and-toilets/before.txt; echo "status $?"`
Expected: a timing line for each scenario and each park on the terminal, then `status 0`.

### Task 2: Describe the new kinds in the specs

Files:
- Modify: `src/sim/park/SPEC.md` (Intent)
- Modify: `src/sim/routes/SPEC.md` (Connectors)
- Modify: `src/tools/SPEC.md` (Boundary, Tools)
- Modify: `src/render/SPEC.md` (The park)
- Modify: `src/app/SPEC.md` (Tools and the ghost)

Step 1: Make each edit FEATURE.md's Spec changes gives.

### Task 3: Add the kinds

Files:
- Modify: `src/sim/park/intent.h:39-41`, `src/sim/park/intent.h:72-74`
- Modify: `src/tools/tools.h:17-27`

Step 1: In intent.h, give BoxKind the values Shop, Depot, Bench, ToiletBlock, in that order, and give enumNames the four names shop, depot, bench, and toilet-block.

Step 2: Make boxSize a switch over the four kinds: shop 8.0 by 6.0, depot 12.0 by 8.0, bench 2.0 by 1.0, and toilet block 8.0 by 5.0, width first. After the switch, return the shop's size, which only a value outside the enum reaches. Add a comment above it: "A box's footprint by kind: a bench is street furniture, and a toilet block is about a shop's size."

Step 3: In tools.h, add PlaceBench and PlaceToilet to ToolKind, after PlaceDepot.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

### Task 4: Give doors only to shops, depots, and entrances

Files:
- Modify: `src/sim/routes/networks.cpp:154-173`

Step 1: In doorsServing, give a backstage front door only to a box of kind Depot, in place of every box that is not a shop. A shop keeps its guest front door and backstage back door. Change the comment's end to "a shop's front guests and its back backstage, and a depot's front backstage. A bench or a toilet block has no doors."

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Give the kinds their looks

Files:
- Modify: `src/render/park_mesh.h:47-56`

Step 1: Make boxHeight a switch over the four kinds: shop 4.0, depot 6.0, bench 1.0, and toilet block 3.5. After the switch, return the shop's height.

Step 2: Make boxColor a switch over the four kinds: shop and depot keep their colors, a bench is Rgba{0.55f, 0.38f, 0.22f, 1.0f}, and a toilet block is Rgba{0.45f, 0.72f, 0.70f, 1.0f}. After the switch, return the shop's color.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build succeeds with no warnings.

### Task 6: Place benches and toilet blocks

Files:
- Modify: `src/tools/tools.cpp:79-85`
- Modify: `src/tools/frontage.cpp:447-448`
- Modify: `src/tools/tools.h` (the comments of FRONTAGE_PUSH and frontagePose)

Step 1: In tools.cpp, make isPlaceTool true for PlaceShop, PlaceDepot, PlaceBench, and PlaceToilet. Make placedKind give Depot for PlaceDepot, Bench for PlaceBench, ToiletBlock for PlaceToilet, and Shop for every other tool, as a switch whose default case gives Shop, since -Wswitch refuses a switch over ToolKind that leaves values out.

Step 2: In frontagePose, front backstage paving for a depot and guest paving for every other kind, in place of guest paving for a shop and backstage for every other kind.

Step 3: In tools.h, change FRONTAGE_PUSH's comment to "A fronting box that doesn't fit moves straight out by at most this, in meters, so the middle of its front face stays within CONNECTION_REACH of the paving it fronts." Change frontagePose's comment to "The pose of a box of the kind that fronts the paving near the point: backstage paving for a depot, and guest paving for every other kind. None when no such paving is near."

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build succeeds with no warnings.

### Task 7: Add the buttons

Files:
- Modify: `src/app/ui/tool_panel.cpp:13-21`

Step 1: Add {"Place bench", ToolKind::PlaceBench} and {"Place toilet", ToolKind::PlaceToilet} to TOOL_CHOICES, after Place depot.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 8: Add facilities.park

Files:
- Create: `tests/parks/facilities.park`

Step 1: Write the file as tests/parks/new.park's text with next-key 7, followed by a box section. The guest segment runs along x = 0 from z = 123 to z = 103 and is 3 m wide. The bench fronts it from the east, facing -x, its front face 1 m off the paving at x = 2.5. The toilet block fronts it from the west, facing +x, its front face at x = -2.5. Both are clear of the entrance, which starts at z = 125.

    tpj-park 1
    seed 1
    tick 0
    next-key 7

    [entrance]
    1 x=0 z=126.5 facing-x=0 facing-z=-1

    [path-point]
    2 x=0 z=123 radius=0 taper=0
    3 x=0 z=103 radius=0 taper=0

    [path-segment]
    4 kind=guest from=2 to=3 width=3 from-edge-radius=0 to-edge-radius=0

    [box]
    5 kind=bench x=3 z=113 facing-x=-1 facing-z=0
    6 kind=toilet-block x=-5 z=113 facing-x=1 facing-z=0

Step 2: Run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

Step 3: Capture the park.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/facilities.park --capture build/benches-and-toilets/facilities.bmp && python3 -c "from PIL import Image; Image.open('build/benches-and-toilets/facilities.bmp').save('build/benches-and-toilets/facilities.png')"`
Expected: the PNG shows the path with a small brown bench on one side and a larger, taller teal toilet block on the other, each facing the path from about 1 m off it, and no walkway at either. Only the entrance has a walkway.

### Task 9: Play the tools

Files: none.

Step 1: Play warm-main-street.park.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm-main-street.park`
Expected: Place bench and Place toilet show after Place depot. Moving the pointer freehand along the main street, a side street's curve, and beside a junction, each ghost fronts the guest paving as a shop's does, and holding Ctrl leaves it at the pointer. Over a backstage path alone it doesn't front. With Ctrl held, a ghost over a shop or the paving shows the invalid tint, and its click changes nothing. Placed benches and toilet blocks show no walkway, and the guests and shops go on as before. Move box moves each, Delete removes each, and Ctrl+Z and Ctrl+Y undo and redo each edit. Report what was seen, and leave the full check to the user's play.

### Task 10: Confirm the simulation is unchanged

Files:
- Create, scratch: `build/benches-and-toilets/after.txt`

Step 1: Write the same output with this feature's tpj_scenarios, over the same parks as before, and compare.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 $(ls tests/parks/*.park | grep -v facilities.park) tests/parks/stress/winding-path.park > build/benches-and-toilets/after.txt 2>/dev/null && build/windows-debug/tpj_scenarios.exe --compare build/benches-and-toilets/before.txt build/benches-and-toilets/after.txt; echo "status $?"`
Expected: `status 0`.

### Task 11: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 12: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/sim/park/intent.h, src/sim/park/SPEC.md, src/sim/routes/networks.cpp, src/sim/routes/SPEC.md, src/tools/, src/render/park_mesh.h, src/render/SPEC.md, src/app/ui/tool_panel.cpp, src/app/SPEC.md, tests/parks/facilities.park, and plans/effortless-building/facilities-and-hours/benches-and-toilets/. Commit them by pathspec with the commit-hygiene skill, subject `Tools: Place benches and toilet blocks beside guest paths`. If the format hook rewraps a file after the commit, add it and amend the commit before merging. Merge into main as implementing-features says, and rebuild windows-release.
