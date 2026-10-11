# Implementation Plan: Yielding Furniture

## Goal

Furniture follows each furnished side around bends and tapers, yields to everything it would stand on, and spreads evenly within each free stretch, shown on a new park file.

## Approach

parkSurface's sides become lines: each side's body, and at an end where exactly two segments meet, half of the outline's turn there, taken from the outline buildPoint already makes. sim/furniture puts every obstacle into a uniform grid: paving and walkway pieces, box and entrance footprints, door frontage strips, and each piece as it is laid. It tests a piece's grown footprint at places every half meter along each side's line, and refines the ends of each free run by halving. It spreads pieces evenly in each run, as it does now over a whole side. A side's runs are found before any of its pieces is laid, so a run that nothing changed lays the same pieces.

## Placement

Decision 0027 places each behavior this feature adds:

- Each side's line around turns: sim/park, in sim/park/surface.h. surface.cpp builds the outline's turns, and only it knows their shape.
- Obstacles, stretches, and the even spread within them: sim/furniture, in sim/furniture/furniture.cpp, beside the laying it replaces. It reads walkways and doors from routes, which lies in a lower layer.
- The park that holds every case: tests/parks/furnished-paths.park, which the park world checks read.

## Tasks

### Task 1: Make the before parks

Files:
- Create, scratch: `build/yielding-furniture/before/`

Step 1: Build main's tpj_scenarios with windows-debug and tpj_bench with windows-release, and make the slice parks and the full park before any change.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && cmake.exe --build --preset windows-release --target tpj_bench && mkdir -p build/yielding-furniture/before && cp tests/parks/fed.park build/yielding-furniture/before/fed.park && (cd build/yielding-furniture/before && ../../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../../tests/parks/main-street.park warm-main-street.park ../../../tests/parks/facilities.park stocked.park day.park no-toilet.park noon.park evening.park) && build/windows-release/tpj_bench.exe --full-park build/yielding-furniture/before/full.park && ls build/yielding-furniture/before | wc -l`
Expected: `10`.

### Task 2: Spec the side lines and the laying

Files:
- Modify: `src/sim/park/SPEC.md`, `src/sim/furniture/SPEC.md`

Step 1: Make the park and furniture spec changes FEATURE.md lists, word for word.

### Task 3: Change the side interface

Files:
- Modify: `src/sim/park/surface.h`, `src/sim/park/surface.cpp`, `src/sim/furniture/furniture.cpp`

Step 1: In surface.h, replace BodySide with SegmentSide as FEATURE.md gives it, add SidePoints as PavedSurface's last member, and declare sidePoints after edgePoints. Update PavedSurface's comment: it also holds the positions the sides run through.

Step 2: In surface.cpp, define sidePoints as edgePoints is defined. Rename Output::bodySide to side. For now it appends the side's start and end to SidePoints and adds a SegmentSide over them with Body 0 and RunsOn false at both ends. In hideJoined, also move surface.SidePoints into the visible surface.

Step 3: In furniture.cpp, read a side's start and end as the first and last of its sidePoints, so the module builds unchanged.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 4: Run each side around its turns

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: Add `struct TurnSpan { size_t First = 0; size_t Count = 0; }`, a run of positions in a list. Scratch gains `std::vector<ParkPoint> TurnPoints` and `std::vector<std::array<std::array<TurnSpan, 2>, 2>> Turns`, one entry per shape, indexed by end and then by Side. A Count of 0 means that end has no turn on that side. buildSurface clears TurnPoints and gives Turns one empty entry per shape, before its two passes.

Step 2: Add `appendHalves(scratch, outline, a, b)`, where a and b are the two ends. Let T be the outline's length. The first half runs from outline[0] along the outline to the place T/2 along it, which it ends with. The second half runs from the outline's last position back along it to the same place. Append each to TurnPoints, and set Turns[a.Shape][a.Which][LEFT] to the first half's span and Turns[b.Shape][b.Which][RIGHT] to the second's. When T is 0, each half is its first position alone.

Step 3: In buildPoint, when the point has exactly two ends and they don't leave it in the same direction, call appendHalves for each pair, right after outlinePair and before the outline gains its middles. Two ends leave in the same direction when the absolute value of ccw of their outward directions is at most PARALLEL and their dot product is above 0. Only buildSurface's Scratch has Turns sized, so call it only when Turns isn't empty.

Step 4: In buildSurface's last loop, build each shape's left side and then its right. The left side uses Side LEFT at end 0 and RIGHT at end 1, and the right side uses RIGHT at end 0 and LEFT at end 1. Leave a side out when `dot(shape.trimmed(1, s1) - shape.trimmed(0, s0), shape.Along) < -SAME_POINT`. Otherwise, its positions are:
- end 0's turn half on its side, reversed, which ends at shape.trimmed(0, s0), or that corner alone when end 0 has no turn there;
- then end 1's turn half on its side in order, which starts at shape.trimmed(1, s1), or that corner alone when it has none.

RunsOn holds whether each end has a turn on that side. Output::side takes the segment, whether it is the left side, RunsOn, and the positions. It also takes which position is shape.trimmed(0, s0), where the body starts. It appends the positions to SidePoints, leaving out each position that repeats the one before by samePoint. Body is the index, among the side's positions, of the one the body's start landed on. It adds the SegmentSide when at least two positions remain, and otherwise drops them.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 5: Collect the obstacles

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Add the tuning values beside the others: `CLEARANCE = PIECE_GAP / 2.0`, the margin around a piece, so neighbours keep PIECE_GAP apart; `SAMPLE_STEP = 0.5`, how far apart the places along a side that are tested lie, in meters; `REFINE_HALVINGS = 10`, how many times the end of a stretch is halved; and `GRID_CELL = 8.0`, the side of the obstacle grid's square cells, in meters.

Step 2: Add `struct Outline { std::array<ParkPoint, 4> Corners{}; size_t Count = 0; }`, a convex polygon of at most four corners. Add `overlaps(a, b)`. It is true when, on the normal of every side of both polygons, their projections overlap by more than CONTACT_TOLERANCE (sim/park/edits.h), so polygons that only touch don't overlap.

Step 3: Add a class `Obstacles`. It holds every Outline added to it, and a grid of GRID_CELL cells covering the park's square from -PARK_SIZE/2 to PARK_SIZE/2. Each cell holds the indices of the outlines whose bounding rectangles reach into it, clamped to the grid. `add(outline)` stores the outline and enters its index in its cells. `blocks(outline)` is true when it overlaps any outline entered in a cell its bounding rectangle reaches.

Step 4: Add `obstaclesOf(world)`, which fills an Obstacles with:
- every piece of `parkSurface(world, walkways)`, where walkways is every walkway of `keyedWalkways(world, PathKind::Guest)` and then of `keyedWalkways(world, PathKind::Backstage)`;
- each box's footprint, `footprintOf(box.At, boxSize(box.Kind))`, for each box parkBoxes gives that has one;
- each entrance's footprint, `footprintOf(entrance.At, ENTRANCE_SIZE)`, for each entrance parkEntrances gives that has one;
- each door's frontage strip. For each kind and each of its KeyedWalkways, find the door in `parkDoors(world, kind)` whose `connectorKey(door.Entity, door.Side)` is the walkway's Connector. Let depth be `dot(walkway.Way.Meets - door.At, door.Out)`. When the depth is above 0, the strip is the rectangle from `door.At - across * (door.Width / 2)` to `door.At + across * (door.Width / 2)` along the face, and out from there by depth along door.Out, where across is leftOf(door.Out).

Return the surface's Sides and SidePoints with it, since the laying reads them.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 6: Lay pieces in each side's stretches

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Replace layStretch with laying along a side's line. Measure places on the line by station: the distance along the line, shifted so that on the body it equals the distance along the segment's direction from its From point. Let c be `dot(body - from, along)` less the line's length up to the body, where body is the side's position at Body, from is the From point's position, and along the segment's unit direction. Then the station u lies at the length u - c along the line. A change at one end of the side leaves the stations of every place on the body, and beyond it toward the other end, as they were.

Let w and depth be the kind's furnitureSize, and let out(d) be the quarter turn of d off the paving: counter-clockwise, seen from above, for Left, and clockwise for Right. `placeAt(u)` gives the position at station u, clamped to the line's ends. A piece at u has this pose:
- Let `a = placeAt(u - w/2)`, `b = placeAt(u + w/2)`, and d the unit direction from a to b.
- The pose stands at `placeAt(u) + out(d) * (VERGE_GAP + depth / 2)` and faces `-out(d)`.
- If a and b are the same place, nothing can stand at u.

`canStand(u)` is true when every corner of the pose's footprint lies within the park's square, and the obstacles don't block that footprint grown by CLEARANCE on every side. To grow it, move its front corners forward and its back corners back by CLEARANCE, and its right corners right and its left corners left by CLEARANCE.

Step 2: Find the side's runs before laying any of its pieces.
- Let step be the larger of the side's spacing and the least step, as now. At an end where RunsOn holds, the nearest piece's center may stand step/2 from the end. Where it doesn't, it may stand END_GAP + w/2 from the end. Pieces may stand from station lo, that far past the line's start, to station hi, that far before its end. If hi < lo, the side has no runs.
- The places tested are lo, hi, and every whole multiple of SAMPLE_STEP between them. They don't depend on the line's length or where it starts.
- Each maximal run of tested places where a piece can stand is a run. Where a run starts just after a place where nothing can stand, halve the stretch between the two places REFINE_HALVINGS times. Always keep the half whose two ends differ, and start the run at the free end that remains. Find a run's end the same way. Each end then depends only on the two places around it and what blocks there.

Step 3: Lay each run as layStretch spread a stretch before.
- The room is the run's length, and the gaps are chosen as they are now.
- With no gap, one piece stands at the run's middle. Otherwise piece i stands at `start + i * room / gaps`.
- If canStand holds for a piece, lay it and add its grown footprint to the obstacles. Otherwise leave it out and go on. Its index in the stretch stays i either way.

Step 4: Key each piece as now, with createDerivedEntity(segment, PIECE_PURPOSE, index). The stretch key is the run's start station plus KEY_ORIGIN, in whole centimeters, rounded down. If that is not above the key of the side's run before it, use that key plus 1.

Step 5: layFurniture takes obstaclesOf(world) once when any segment is furnished, and lays each furnished segment in key order, its left side and then its right, as each side's SegmentSide in the surface gives it. It removes the pieces it didn't lay, as now. Delete sideOf's use of a body side's two corners, and anything else left unused.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build ends with no warnings and no errors.

### Task 7: Add furnished-paths.park

Files:
- Create: `tests/parks/furnished-paths.park`

Step 1: Write the park. It is the park the user played in furnished-segments, with these changes:
- Segment 12 is narrowed to MIN_PATH_WIDTH.
- Point 18's corner is tightened to a radius of 3 m.
- A path of MAX_PATH_WIDTH tapers into a 3 m path, at x = -80, with a shop fronting each.
- Every guest segment has furnishing.

```
tpj-park 1
seed 1
tick 0
next-key 58

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path-point]
2 x=0 z=123 radius=0 taper=0
3 x=0 z=103 radius=0 taper=14.001999999999455
5 x=0 z=43 radius=0 taper=0
7 x=-40 z=43 radius=8.000703745704095 taper=0
9 x=65 z=43 radius=16.020933963598235 taper=0
10 x=84.79898987322333 z=23.20101012677667 radius=0 taper=0
13 x=0 z=73 radius=0 taper=0
14 x=-40 z=73 radius=8.000703745704083 taper=0
18 x=28 z=55 radius=3 taper=0
19 x=52 z=55 radius=8.000703745704095 taper=0
20 x=52 z=31 radius=8.000703745704095 taper=0
21 x=28 z=31 radius=8.000703745704088 taper=0
22 x=52 z=43 radius=0 taper=0
23 x=28 z=43 radius=0 taper=0
32 x=-15 z=100 radius=0 taper=0
33 x=-15 z=33 radius=4.002766086414594 taper=0
34 x=15 z=33 radius=4.002766086414594 taper=0
35 x=15 z=100 radius=0 taper=0
51 x=-80 z=100 radius=0 taper=0
52 x=-80 z=60 radius=0 taper=0
53 x=-80 z=30 radius=0 taper=0

[path-segment]
4 kind=guest from=2 to=3 width=3 from-edge-radius=0 to-edge-radius=0
6 kind=guest from=3 to=13 width=10 from-edge-radius=0 to-edge-radius=0
8 kind=guest from=5 to=7 width=3 from-edge-radius=4.000464399475602 to-edge-radius=9.500703745704095
11 kind=guest from=5 to=23 width=3 from-edge-radius=0 to-edge-radius=0
12 kind=guest from=9 to=10 width=1.5 from-edge-radius=0 to-edge-radius=0
15 kind=guest from=13 to=5 width=10 from-edge-radius=2.002357845008513 to-edge-radius=4.000464399475602
16 kind=guest from=13 to=14 width=3 from-edge-radius=2.002357845008513 to-edge-radius=0
17 kind=guest from=14 to=7 width=3 from-edge-radius=9.500703745704083 to-edge-radius=0
24 kind=guest from=23 to=22 width=3 from-edge-radius=0 to-edge-radius=0
25 kind=guest from=22 to=9 width=3 from-edge-radius=0 to-edge-radius=17.50165664811262
26 kind=guest from=18 to=19 width=3 from-edge-radius=0 to-edge-radius=9.500703745704095
27 kind=guest from=19 to=22 width=3 from-edge-radius=0 to-edge-radius=0
28 kind=guest from=22 to=20 width=3 from-edge-radius=0 to-edge-radius=9.500703745704095
29 kind=guest from=20 to=21 width=3 from-edge-radius=0 to-edge-radius=9.500703745704088
30 kind=guest from=21 to=23 width=3 from-edge-radius=0 to-edge-radius=0
31 kind=guest from=23 to=18 width=3 from-edge-radius=0 to-edge-radius=9.500703745704088
36 kind=backstage from=32 to=33 width=2 from-edge-radius=0 to-edge-radius=0
37 kind=backstage from=33 to=34 width=2 from-edge-radius=5.002250501237035 to-edge-radius=0
38 kind=backstage from=34 to=35 width=2 from-edge-radius=5.002250501237035 to-edge-radius=0
54 kind=guest from=51 to=52 width=16 from-edge-radius=0 to-edge-radius=0
55 kind=guest from=52 to=53 width=3 from-edge-radius=0 to-edge-radius=0

[box]
39 kind=depot x=0 z=27 facing-x=-0 facing-z=1
40 kind=shop x=9 z=97 facing-x=-1 facing-z=-0
41 kind=shop x=9 z=88 facing-x=-1 facing-z=-0
42 kind=shop x=9 z=79 facing-x=-1 facing-z=-0
43 kind=shop x=9 z=70 facing-x=-1 facing-z=-0
44 kind=shop x=9 z=61 facing-x=-1 facing-z=-0
45 kind=shop x=9 z=52 facing-x=-1 facing-z=-0
46 kind=shop x=-9 z=97 facing-x=1 facing-z=-0
47 kind=shop x=-9 z=88 facing-x=1 facing-z=-0
48 kind=shop x=-9 z=79 facing-x=1 facing-z=-0
49 kind=shop x=-9 z=61 facing-x=1 facing-z=-0
50 kind=shop x=-9 z=52 facing-x=1 facing-z=-0
56 kind=shop x=-68 z=80 facing-x=-1 facing-z=0
57 kind=shop x=-74.5 z=38 facing-x=-1 facing-z=0

[left-furnishing]
4 kind=bench spacing=6
6 kind=table spacing=8
8 kind=bench spacing=10
12 kind=table spacing=4
15 kind=bench spacing=10
16 kind=bench spacing=8
17 kind=table spacing=6
24 kind=bench spacing=8
25 kind=bench spacing=8
26 kind=table spacing=6
27 kind=bench spacing=8
29 kind=bench spacing=8
30 kind=bench spacing=8
31 kind=table spacing=10
54 kind=table spacing=6
55 kind=bench spacing=8

[right-furnishing]
4 kind=bench spacing=6
6 kind=table spacing=8
11 kind=bench spacing=10
12 kind=table spacing=4
15 kind=bench spacing=10
16 kind=bench spacing=8
24 kind=bench spacing=8
25 kind=bench spacing=8
26 kind=table spacing=6
27 kind=bench spacing=8
28 kind=bench spacing=8
29 kind=bench spacing=8
30 kind=bench spacing=8
31 kind=table spacing=10
54 kind=table spacing=6
```

Step 2: Run the park world checks, which now include it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 8: Capture the park

Files:
- Create, scratch: `build/yielding-furniture/furnished-paths.bmp`, `build/yielding-furniture/furnished-paths.png`

Step 1: Build the app and capture the park.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/furnished-paths.park --capture build/yielding-furniture/furnished-paths.bmp && python3 -c "from PIL import Image; Image.open('build/yielding-furniture/furnished-paths.bmp').save('build/yielding-furniture/furnished-paths.png')"`
Expected: the capture shows benches and tables along every guest segment, following the rounded corners and the taper, and none on paving, a shop, or a walkway, as FEATURE.md's criteria 1 and 2 say.

### Task 9: Check where the pieces stand

Files:
- Create, scratch: `build/yielding-furniture/build.sh`, `build/yielding-furniture/yieldcheck.cpp`, `build/yielding-furniture/plot.py`

Step 1: Copy build.sh and plot.py from build/furnished-segments/. Write yieldcheck.cpp. It loads a park file, resolves it, and writes the plot file furnplot wrote: paving pieces, boxes, edge lines, and each piece's footprint by kind. Then it checks each piece of parkFurniture against what FEATURE.md's criterion 2 names. It counts the pieces whose footprint overlaps, by more than CONTACT_TOLERANCE:
- a piece of `parkSurface(world)`;
- a walkway's piece, by walkwayPiece, over both kinds' connectorWalkways;
- a box's footprint;
- an entrance's footprint;
- a door's frontage strip, as the furniture spec defines it;
- another piece's footprint.

It also counts the pieces with a corner outside the park's square. It prints one count per kind of obstacle. For each segment's side, it prints how far apart neighbouring pieces stand along the side's line, to the centimeter, measured between the places on the line nearest their centers.

Run: `build/yielding-furniture/build.sh yieldcheck && build/yielding-furniture/yieldcheck.exe tests/parks/furnished-paths.park build/yielding-furniture/pieces.txt && python3 build/yielding-furniture/plot.py build/yielding-furniture/pieces.txt build/yielding-furniture/whole.png -95 15 90 130 6`
Expected: every overlap count is 0, and the distances along each side's line repeat one value within each stretch. The plot shows furniture around each corner, along the taper, and on across bends, as criterion 1 says. Look at the tight corner at point 18 and the taper at point 52 in close plots, from `-5 50 50 60 20` and `-100 25 -60 105 12`.

### Task 10: Check that a shop moves only its stretch

Files:
- Create, scratch: `build/yielding-furniture/shopcheck.cpp`

Step 1: Write shopcheck.cpp. It loads tests/parks/furnished-paths.park, resolves it, and keeps parkFurniture. It then checks whether isAccepted holds for `AddBox{BoxKind::Shop, Pose{-68, 93, -1, 0}}` and commits it. This shop stands in the first stretch of segment 54's right side, before shop 56. The check keeps parkFurniture again and reports:
- whether the edit was accepted;
- how many pieces of segment 54's right side with z above 84 changed, by key or pose, or went;
- how many pieces anywhere else changed their key or pose, or went;
- how many pieces are new.

Step 2: From the park as loaded again, it commits `SetTaperLength{52, 6}`, which reshapes the taper at the far end of segment 54. It reports how many pieces of segment 54's right side with z above 84, the stretch before shop 56, changed by key or pose, or went.

Run: `build/yielding-furniture/build.sh shopcheck && build/yielding-furniture/shopcheck.exe`
Expected: accepted, at least 1 piece changed in segment 54's right side above z = 84, 0 pieces changed elsewhere, and the new pieces all on segment 54's right side above z = 84. After the taper edit, 0 of those pieces changed.

### Task 11: Play it

Files: none.

Step 1: Rebuild the whole windows-release preset, and play.

Run: `cmake.exe --build --preset windows-release && build/windows-release/ThemeParkJones.exe`
Expected: do FEATURE.md's criterion 4 freehand on a new park, then open tests/parks/furnished-paths.park and place a shop beside the benches on segment 4. Each ghost shows the pieces removed in the delete tint and the re-spread pieces in their colors, and each commit gives what its ghost showed. Report what was seen.

### Task 12: Confirm the parks are unchanged

Files:
- Create, scratch: `build/yielding-furniture/after/`

Step 1: Make the same parks with this feature's build and compare them with Task 1's.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && cmake.exe --build --preset windows-release --target tpj_bench && mkdir -p build/yielding-furniture/after && cp tests/parks/fed.park build/yielding-furniture/after/fed.park && (cd build/yielding-furniture/after && ../../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../../tests/parks/main-street.park warm-main-street.park ../../../tests/parks/facilities.park stocked.park day.park no-toilet.park noon.park evening.park) && build/windows-release/tpj_bench.exe --full-park build/yielding-furniture/after/full.park && for f in fed warm cut warm-main-street stocked day no-toilet noon evening full; do cmp build/yielding-furniture/before/$f.park build/yielding-furniture/after/$f.park && echo "$f same"; done`
Expected: ten lines, from `fed same` to `full same`.

### Task 13: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 14: Commit and merge

Files:
- Modify: `plans/effortless-building/furnished-paths/MILESTONE.md`

Step 1: In MILESTONE.md's Medium, change the sentence on what sim/furniture reads to: "sim/furniture reads park intent through parkSegments, parkPathPoints, parkBoxes, parkEntrances, footprintOf, and parkSurface, and doors and walkways through routes' parkDoors and keyedWalkways."

Step 2: Stage only this feature's paths: src/sim/park/SPEC.md, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/furniture/, tests/parks/furnished-paths.park, plans/effortless-building/furnished-paths/MILESTONE.md, and plans/effortless-building/furnished-paths/yielding-furniture/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Make furniture follow curves and yield to obstacles`. If the format hook rewraps a file, add it and amend before merging. Merge into main as implementing-features says, and rebuild windows-release.
