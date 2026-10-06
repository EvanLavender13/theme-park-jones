# Implementation Plan: Point Nodes

## Goal

Derive each network's nodes from the path points segments share and the connectors' ends, and give each segment's carrier the segment's width.

## Approach

pathCarriers sets each carrier's width from its segment and records the keys of the segment's two points. The geometric meeting search is replaced by meetings made from shared point keys: each carrier's start meets every other carrier end at the same point. addStops is kept as it is, so stops still group within JUNCTION_TOLERANCE and nodes are still numbered by first stop in key order. Connectors still join their segment through a meeting, as today. A scratch program checks the widths, the nodes, and an overlapping path, both before and after the change.

## Placement

Decision 0027 places each behavior this feature adds:

- Nodes from shared points, and carriers with their segments' widths: sim/routes, the networks component in sim/routes/networks.h and networks.cpp. routes derives every park network there, and it already reads segments through parkSegments.
- The scratch check: build/point-nodes/, outside the repository's tracked files. It is a one-off check like earlier features' scratch checks, and it adds no behavior to any module.

## Tasks

### Task 1: Update the routes spec

Files:
- Modify: `src/sim/routes/SPEC.md:15-24`, `src/sim/routes/SPEC.md:36`

Step 1: In the Networks section, replace the paragraph at line 15 with:

    A kind's network has a carrier for each segment of that kind whose ground line is not empty, keyed by the segment's key, following its ground line, and with the segment's width. It also has a carrier for each connector, with its kind's path width (pathWidth, sim/park/SPEC.md). Every edge is a stretch of a segment or a connector, measured along it (principle 4). Paths of different kinds never share a node, even where they cross.

Step 2: Replace the paragraph at line 17 and its five bullets (lines 17 to 22) with:

    Carriers join only at path points and at connectors' ends. Every carrier whose segment ends at a point stops there, and all of them share one node. Segments of one kind that cross, touch, or overlap without sharing a point don't join. At a rounded corner, the node lies at the middle of the arc, where the two segments' ground lines meet.

Keep the following sentence, "If nodes are closer together along a carrier than JUNCTION_TOLERANCE, they are one node."

Step 3: In the Connectors section, in the paragraph starting "If a door is within CONNECTION_REACH", replace "and joins the path's network there like any other meeting" with "and stops that path's carrier there at a node".

### Task 2: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md:18`, `src/sim/park/SPEC.md:77`

Step 1: In Intent, replace "Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md)." with "Routes joins segments only where they share a point (sim/routes/SPEC.md)."

Step 2: In Physical validity, replace "Segments never conflict with each other. Where they cross, routes derives a junction." with "Segments never conflict with each other, even where they cross or overlap."

### Task 3: Update the capability brief

Files:
- Modify: `plans/navigable-networks/CAPABILITY.md:20-27`

Step 1: In Foundation criteria, replace the sentence "Resolving path intent derives one graph for each network kind: guest and backstage. Nodes sit at:" and its four bullets with:

    Resolving path intent derives one graph for each network kind: guest and backstage. Nodes sit at the path points that segments end at, and every segment of the kind that ends at a point shares its node. Nodes also sit at both ends of each connector. Paths of one kind that cross or overlap without sharing a point stay apart.

Keep the paragraph that follows, starting "Each edge's length is measured along the path's ground line."

### Task 4: Write the scratch check and run it before the change

Files:
- Create: `build/point-nodes/build.sh`
- Create: `build/point-nodes/check.cpp`

Step 1: Copy build/main-street/build.sh to build/point-nodes/build.sh. It compiles a named .cpp against the windows-debug libtpj_tools.a and libtpj_sim.a.

Step 2: Write check.cpp. It reads tests/parks/main-street.park, loads it with loadWorld(makeParkSchema(), text), and calls resolveWorld. Then it prints four lines.

- `width street <a> side <b> connector <c>`, each with %g. a is widthAt at distance 1 on the carrier of the first guest segment whose width is 10. b is the same for the first guest segment whose width is 3. c is the same for the first backstage carrier whose first stop's node is anchored, which is a connector.
- `guest other-nodes <n> unshared-points <m>`, and the same line for backstage. n counts the nodes none of whose stopPlaces lies at distance 0 or at the full length of its carrier. m counts, over each pair of segments of the kind with ground lines that share a point key, the pairs whose carriers' stops at that point have different nodes. A carrier's stop at its segment's From point is its first stop, and at its To point its last.
- `street stops: <same|changed>`. Record the stops of both 10 m street carriers. Commit AddPath{PathKind::Guest, {NewPoint{{0, 100}}, NewPoint{{0, 80}}}}: check isAccepted, queue it with queueEdit, and apply it with makeCandidate. Then compare the two carriers' stops with the recorded ones, distance and node.

If the AddPath is refused, print `refused` and exit with status 1.

Step 3: Build the windows-debug libraries, then build and run the check against today's routes.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools && sh build/point-nodes/build.sh check && build/point-nodes/check.exe`
Expected:

```
width street 3 side 3 connector 2
guest other-nodes 0 unshared-points 0
backstage other-nodes 0 unshared-points 0
street stops: changed
```

The last line shows that today's geometric search joins the overlapping path to the street. If another line differs, stop and report it.

### Task 5: Reword the junction tolerance and the path width

Files:
- Modify: `src/sim/routes/networks.h:17`
- Modify: `src/sim/park/intent.h:59-60`

Step 1: Replace the comment above JUNCTION_TOLERANCE with:

    // Connectors that meet a carrier this close to each other, or to its end, stop there at one node.

Step 2: In `src/sim/park/intent.h:59-60`, replace the comment above pathWidth with:

    // The width a new segment of the kind takes, in meters, and the width routes gives the kind's
    // connectors.

### Task 6: Take nodes from shared points

Files:
- Modify: `src/sim/routes/networks.cpp:21-174`, `src/sim/routes/networks.cpp:375-396`

Step 1: Make pathCarriers set each carrier's Width to its segment's Width. Have it also return each carrier's two point keys, From and To, in a vector parallel to the carriers, for example as a small struct holding both vectors.

Step 2: Add pointMeetings in the anonymous namespace. For each carrier, it lists two ends: its From key at distance 0 and its To key at its length. It sorts the ends by point key, then carrier index, then distance. For each run of ends with the same point key, it adds a Meeting from the run's first end to each other end in the run. Give it a comment saying that carriers ending at one point meet there.

Step 3: Delete the geometric search, which nothing else uses: sideOf, oppositeSigns, distanceAt, projectionWithin, MEETING_MARGIN, farApart, addMeetings, SegmentSpan, and findMeetings. Keep Meeting, groupStarts, groupHolding, StopJoins, and addStops unchanged.

Step 4: In deriveNetwork, make the meetings with pointMeetings instead of findMeetings. Connectors keep adding their meetings after it, as they do today.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 7: Run the scratch check after the change

Step 1: Rebuild and run the check.

Run: `sh build/point-nodes/build.sh check && build/point-nodes/check.exe`
Expected:

```
width street 10 side 3 connector 2
guest other-nodes 0 unshared-points 0
backstage other-nodes 0 unshared-points 0
street stops: same
```

### Task 8: Capture main-street.park's networks

Step 1: Build the app and capture main-street.park with the graph view.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --graph --capture build/point-nodes/graph.bmp`
Expected: the app exits with status 0, and build/point-nodes/graph.bmp exists.

Step 2: Look at the capture. It shows a node at each path point and at both ends of each connector: the shops' back doors and the depot to the backstage path, and the entrance to its path. It shows no node where the backstage path crosses a guest path.

### Task 9: Check the slice parks and the full park

Step 1: Regenerate warm.park and cut.park from fed.park into the build directory, and compare them with the checked-in ones.

Run: `mkdir -p build/windows-debug/regen && cp tests/parks/fed.park build/windows-debug/regen/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks build/windows-debug/regen/fed.park build/windows-debug/regen/warm.park build/windows-debug/regen/cut.park; for f in fed warm cut; do cmp build/windows-debug/regen/$f.park tests/parks/$f.park && echo "$f same"; done`
Expected:

```
fed same
warm same
cut same
```

Step 2: Regenerate the full park into the build directory and compare it with the checked-in one. It takes about three minutes.

Run: `build/windows-debug/tpj_bench.exe --full-park build/windows-debug/regen/full.park && cmp build/windows-debug/regen/full.park tests/parks/stress/full.park && echo "full same"`
Expected: `full same`

If a comparison prints a difference instead, one of these parks joined segments by geometry alone. Stop and report it.

### Task 10: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest ends with `100% tests passed`, and tidy ends with `tidy: clean.`

Step 2: Read src/sim/routes/SPEC.md, src/sim/park/SPEC.md, and plans/navigable-networks/CAPABILITY.md beside networks.cpp, and confirm each describes the networks as built.

### Task 11: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/routes/SPEC.md, src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/sim/park/SPEC.md, src/sim/park/intent.h, plans/navigable-networks/CAPABILITY.md, and plans/navigable-networks/wide-routes/point-nodes/. Use the subject `Routes: Join segments only at shared points`.
