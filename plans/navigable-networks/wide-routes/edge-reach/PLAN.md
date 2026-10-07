# Implementation Plan: Edge Reach

## Goal

Connect each door to the segment of its kind whose edge is nearest, within 3 m of that edge.

## Approach

findConnections stops using nearestPlace over all path carriers. For each door, it walks the path carriers in key order and finds the nearest place on each with nearestPlaceOn. It keeps the carrier whose distance less half its width is smallest, the first in key order winning a tie, and connects when that is within CONNECTION_REACH. A scratch program lists every connector in every checked-in park before and after the change, and places shops by frontage beside a 16 m and a 1.5 m segment.

## Placement

Decision 0027 places each behavior this feature adds:

- Reach measured from a segment's edge, and the choice of the nearest edge: sim/routes, the networks component in sim/routes/networks.h and networks.cpp. routes derives every connector there, and it already holds each carrier's width.
- The scratch check: build/edge-reach/, outside the repository's tracked files. It is a one-off check like earlier features' scratch checks, and it adds no behavior to any module.

## Tasks

### Task 1: Update the routes spec

Files:
- Modify: `src/sim/routes/SPEC.md:31`

Step 1: In the Connectors section, replace the paragraph starting "If a door is within CONNECTION_REACH of a path of the kind it serves" with:

    A door's distance from a segment is its distance from the segment's ground line, less half the segment's width: how far it stands from the segment's edge. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment, the lower key winning a tie. A connector is a short straight carrier keyed connectorKey(entity, face). It runs from the door to the nearest point on that segment's ground line, and stops that segment's carrier there at a node. If no segment of its kind is within reach, a door has no connector, and nothing else about its entity changes (principle 2). If a door is flush against a segment's edge, it still gets a connector. If it lies on the nearest segment's ground line, within JUNCTION_TOLERANCE, which only a hand-written save allows, it has no connector.

### Task 2: Update the capability brief and the slice

Files:
- Modify: `plans/navigable-networks/CAPABILITY.md:24`, `plans/navigable-networks/CAPABILITY.md:61`
- Modify: `plans/slices/good-paths/SLICE.md:89`

Step 1: In CAPABILITY.md's Foundation criteria, replace "It connects to the nearest point of a path of that kind, within a short fixed reach:" with "It connects to the path of that kind whose edge is nearest, within a short fixed reach of that edge, at the nearest point of the path's center line:".

Step 2: In CAPABILITY.md's Principles, replace "The one straight-line measure is a connection's fixed reach, from a box face to the nearest path point." with "The one straight-line measure is a connection's fixed reach, from a door to the nearest path edge."

Step 3: In SLICE.md's Open questions, replace the Door reach item with the line "None."

### Task 3: Write the scratch check and run it before the change

Files:
- Create: `build/edge-reach/build.sh`
- Create: `build/edge-reach/check.cpp`

Step 1: Copy build/point-nodes/build.sh to build/edge-reach/build.sh. It compiles a named .cpp against the windows-debug libtpj_tools.a and libtpj_sim.a.

Step 2: Write check.cpp. It takes park file paths as arguments. For each, it loads the park with loadWorld(makeParkSchema(), text), calls resolveWorld, and prints, for each kind:

- one line per connector, `<path> <guest|backstage> <entity> <front|back> <x> <z>`. A connector is a carrier whose first stop's node is anchored. The entity is that anchor, and the face is front when the carrier's key equals connectorKey(entity, Face::Front). x and z are the connector's last point, with %.9g.
- a line `<path> <guest|backstage>-connectors <n>`, with the number of connectors.

Then it prints two lines for the frontage snap, one for each width w of 16 and 1.5. Start from makeNewPark(1), resolved. Commit AddPath{PathKind::Guest, {NewPoint{{20, 0}}, NewPoint{{20, -40}}}}, then SetSegmentWidth on that path's segment with width w. Ask frontagePose(world, BoxKind::Shop, {20 + w / 2 + 3, -20}) for a pose, and commit AddBox{BoxKind::Shop, pose}. Commit each edit as build/main-street/draw.cpp does: check isAccepted, queue it with queueEdit, and apply it with makeCandidate. Print `frontage <w> connected` if the guest network anchors a node to the new box, and `frontage <w> unconnected` otherwise. If an edit is refused or frontagePose gives none, print `frontage <w> refused` instead.

Step 3: Build the windows-debug libraries, then build and run the check on every checked-in park against today's routes.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools && sh build/edge-reach/build.sh check && build/edge-reach/check.exe tests/parks/*.park tests/parks/stress/*.park > build/edge-reach/before.txt && grep -E "main-street.park (guest|backstage)-connectors|frontage" build/edge-reach/before.txt`
Expected:

```
tests/parks/main-street.park guest-connectors 1
tests/parks/main-street.park backstage-connectors 12
frontage 16 unconnected
frontage 1.5 connected
```

If another line differs, stop and report it.

### Task 4: Set the reach

Files:
- Modify: `src/sim/routes/networks.h:20-21`

Step 1: Replace CONNECTION_REACH and its comment with:

    // A door farther than this from the edge of every segment of its kind has no connector.
    inline constexpr double CONNECTION_REACH = 3.0;

### Task 5: Reach from the nearest edge

Files:
- Modify: `src/sim/routes/networks.cpp`, findConnections and the comment above it

Step 1: In findConnections, replace the nearestPlace call with a walk over the path carriers in key order. For each carrier, take lines.nearestPlaceOn(carrier.Key, door) and its groundPoint, and the straight distance d from the door to that point. The carrier's edge distance is d less half the carrier's Width. Keep the carrier with the smallest edge distance, replacing the kept one only when strictly smaller, so the lower key wins a tie. Skip a carrier when nearestPlaceOn or groundPoint gives none.

Step 2: After the walk, skip the door when no carrier was kept, when the kept edge distance is above CONNECTION_REACH, or when the kept d is not above JUNCTION_TOLERANCE. Build the connector as today from the kept place, point, and d. The connector's length is still d, from the door to the ground line.

Step 3: Change the comment above findConnections to say that each door connects to the path carrier whose edge is nearest, when that edge lies within reach, the lower key winning a tie.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 6: Run the scratch check after the change

Step 1: Rebuild and run the check, then compare with the listing from before.

Run: `sh build/edge-reach/build.sh check && build/edge-reach/check.exe tests/parks/*.park tests/parks/stress/*.park > build/edge-reach/after.txt && diff build/edge-reach/before.txt build/edge-reach/after.txt | grep -v "main-street.park guest " | grep "^[<>]"`
Expected:

```
< tests/parks/main-street.park guest-connectors 1
> tests/parks/main-street.park guest-connectors 12
< frontage 16 unconnected
> frontage 16 connected
```

The filter hides main-street.park's eleven new front-door connector lines. Any other difference means a door in another park connects differently. Stop and report it.

Fixed during implementation: the run also showed three lines for tests/parks/degenerate-intent.park, whose guest connectors went from 0 to 1. Its shop 17's front door lies about 4.49 m from the free end of a 3 m guest segment. That is beyond the old 4 m reach and 2.99 m from the edge, inside the guest reach's extra 0.5 m that FEATURE.md's Summary describes. The park is hand-written and broken on purpose, and no regenerated park reads it.

Step 2: Confirm the eleven new lines.

Run: `diff build/edge-reach/before.txt build/edge-reach/after.txt | grep -c "^> tests/parks/main-street.park guest .* front"`
Expected: `11`

### Task 7: Capture main-street.park's networks

Step 1: Build the app and capture main-street.park with the graph view.

Run: `cmake.exe --build --preset windows-debug && build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --graph --capture build/edge-reach/graph.bmp`
Expected: the app exits with status 0, and build/edge-reach/graph.bmp exists.

Step 2: Look at the capture. Each shop's front door has a connector to the street's center line, beside the back-door connectors to the backstage path.

### Task 8: Check the slice parks and the full park

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

If a comparison prints a difference instead, stop and report it.

### Task 9: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest ends with `100% tests passed`, and tidy ends with `tidy: clean.`

Step 2: Read src/sim/routes/SPEC.md, plans/navigable-networks/CAPABILITY.md, and plans/slices/good-paths/SLICE.md beside networks.cpp, and confirm each describes connectors as built.

### Task 10: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/routes/SPEC.md, src/sim/routes/networks.h, src/sim/routes/networks.cpp, plans/navigable-networks/CAPABILITY.md, plans/slices/good-paths/SLICE.md, and plans/navigable-networks/wide-routes/edge-reach/. Use the subject `Routes: Reach doors from a segment's edge`.
