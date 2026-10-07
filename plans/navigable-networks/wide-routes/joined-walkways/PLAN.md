# Implementation Plan: Joined Walkways

## Goal

Draw each door's walkway as part of the paved surface, joined to the segment it reaches, so no edge line crosses it there.

## Approach

sim/park gains a parkSurface overload that takes walkways. It builds the surface as today, adds each walkway's piece and its two side edge lines owned by the segment it joins, and then hides edge lines inside joined paving with the existing rule, which now sees the walkways as part of their segments. routes gives each connector's walkway from its network and doors. buildParkMesh draws the surface with the walkways and stops drawing committed walkways as separate strips. A top-down scratch plot checks the edge lines up close, and a rebuilt windows-release capture checks the app.

## Placement

Decision 0027 places each behavior this feature adds:

- A walkway's paving and edge lines, and how they join their segment: sim/park, the surface component in sim/park/surface.h and surface.cpp. It owns the paved surface and the rule that hides edge lines inside joined paving, and it takes walkways as plain values, so it reads nothing above it.
- Each connector's walkway: sim/routes, the networks component in sim/routes/networks.h and networks.cpp. routes derives connectors and doors, so it alone knows where each walkway runs and which segment it joins.
- Drawing the surface with the walkways: render, buildParkMesh in src/render/park_mesh.cpp. It already composes the park's mesh from parkSurface and routes' networks.
- The scratch plot: build/joined-walkways/, outside the repository's tracked files. It adds no behavior to any module.

## Tasks

### Task 1: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md`, Paved surface

Step 1: After the paragraph that starts "Edge lines run along each side of each segment", add the paragraph from FEATURE.md's Spec changes that starts "parkSurface can also add walkways."

Step 2: In Boundary, in the sentence listing what other modules read, after "the paved surface through parkSurface", add ", with or without walkways,".

### Task 2: Update the routes and render specs

Files:
- Modify: `src/sim/routes/SPEC.md`, Connectors
- Modify: `src/render/SPEC.md:39`

Step 1: At the end of routes' Connectors section, add the paragraph from FEATURE.md's Spec changes that starts "connectorWalkways gives a walkway".

Step 2: In routes' Boundary, after "and the doors connectors start from through parkDoors", add ", and their walkways through connectorWalkways".

Step 3: In src/render/SPEC.md's Boundary, in the bullet listing what render reads of the networks, add connectorWalkways beside parkNetwork.

Step 4: In src/render/SPEC.md's buildParkMesh paragraph, replace "If a door has a connector, the door gets a walkway to the path it reaches, in its kind's width and color." with "If a door has a connector, its walkway is drawn as part of the paved surface, joined to the segment it reaches (connectorWalkways)." Delete "A walkway ends round where it meets its path. A walkway starts flush with its door's face."

### Task 3: Add the feature to the milestone

Files:
- Modify: `plans/navigable-networks/wide-routes/MILESTONE.md`

Step 1: In Features, add:

    3. `joined-walkways`: each door's walkway is drawn as part of the paved surface, joined to the segment it reaches, so no edge line crosses it there. Depends on: feature 2.

Step 2: In Acceptance criteria, after criterion 3, add a criterion and renumber the rest:

    4. A door's walkway is drawn as part of the paving, joined to the segment it reaches like a path drawn onto it: no edge line crosses it there. (capture, and play by the user)

Step 3: In Features, replace "Neither feature adds a test (docs/testing.md)." with "None of these features adds a test (docs/testing.md)."

### Task 4: Record the simulation before the change

Step 1: Write the scenarios' output from the current code.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/joined-walkways && build/windows-debug/tpj_scenarios.exe --ticks 100 tests/parks/main-street.park tests/parks/warm.park > build/joined-walkways/before.txt 2> /dev/null; echo $?`
Expected: `0`

### Task 5: Add walkways to the paved surface

Files:
- Modify: `src/sim/park/surface.h`
- Modify: `src/sim/park/surface.cpp`

Step 1: In surface.h, after parkSurface, add Walkway, WALKWAY_OVERLAP, and the overload exactly as FEATURE.md's Public interface gives them.

Step 2: In surface.cpp, define the overload. Build the layout and ground as parkSurface does, and the surface with buildSurface(layout, ground, nullptr, true). For each walkway, skip it if no segment in ground.Segments has its Joins key, its kind, and a ground line, if its Width is not above 0, or if Door and Meets lie closer than MIN_SEGMENT_LENGTH. Otherwise:

- Let d be the unit direction from Door to Meets, n its left (leftOf), and h half the width.
- Each side's door corner is Door + n·h·s + d·t for s of -1 and +1, where t = -h·s·dot(n, Out) / dot(d, Out), which puts it on the face's line. Use t = 0 when dot(d, Out) is below 0.1, or when |t| is not below the walkway's length.
- Let E be Meets + d·WALKWAY_OVERLAP, the far end's middle.
- Add a SurfacePiece of the walkway's kind, owned by Joins, with 4 corners counter-clockwise: the right door corner, E − n·h, E + n·h, the left door corner.
- Add three edge lines of its kind, owned by Joins: the right side from the right door corner to E − n·h, the far end from E − n·h to E + n·h, and the left side from E + n·h to the left door corner. Each keeps the paving on its left. The far end lies inside its segment's paving wherever the walkway meets it, so hiding removes it there and keeps only the part outside, as where a walkway meets a free end at an angle.

Then stable-sort the pieces and the edge lines by owner, so each list stays in ascending key order of its owner. Hide edge lines as surfaceOf does: copy the pieces, build Joins from the ground, and call hideJoined with them.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings or errors.

### Task 6: Give each connector's walkway

Files:
- Modify: `src/sim/routes/networks.h`
- Modify: `src/sim/routes/networks.cpp`

Step 1: In networks.h, include sim/park/surface.h and declare connectorWalkways after parkDoors, with the comment from FEATURE.md's Public interface.

Step 2: In networks.cpp, define it. Take parkNetwork(world, kind) and parkDoors(world, kind). For each carrier in key order whose first stop's node is anchored, which makes it a connector:

- Its entity is that anchor. Its face is Front when its key is connectorKey(entity, Face::Front), and Back otherwise. Its door is the ParkDoor with that entity and face. Skip it if there is none.
- Its segment is the carrier of the first place in stopPlaces of its last stop's node that is not itself and whose carrier's first stop is not anchored. stopPlaces lists places in ascending carrier key, so this is the lowest-keyed segment there. Skip it if there is none.
- Add Walkway{kind, that segment's key, the door's At, the door's Out, the carrier's last point, the carrier's Width}.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools`
Expected: the build ends with no warnings or errors.

### Task 7: Plot the walkways up close

Files:
- Create: `build/joined-walkways/build.sh`
- Create: `build/joined-walkways/view.cpp`

Step 1: Copy build/point-nodes/build.sh to build/joined-walkways/build.sh, and build/main-street/view.cpp to build/joined-walkways/view.cpp. Change view.cpp to gather connectorWalkways for both kinds into one vector and dump parkSurface(world, walkways) instead of parkSurface(world). Include sim/routes/networks.h.

Step 2: Dump and plot the street and the backstage path at 20 pixels per meter.

Run: `sh build/joined-walkways/build.sh view && build/joined-walkways/view.exe tests/parks/main-street.park build/joined-walkways/surf.txt && python3 build/main-street/plot.py build/joined-walkways/surf.txt build/joined-walkways/street.png -20 25 20 128 20 && echo plotted`
Expected: `valid 1`, then `plotted`.

Step 3: Look at build/joined-walkways/street.png. At every shop's front, a tan strip runs from the shop to the street, with an edge line along each side, and no edge line crosses the strip at the street's edge. The same holds at every back door and the backstage path, at the depot, and at the entrance, where the walkway meets the entrance path's free end and no edge line crosses there either. If an edge line crosses a walkway where it meets its path, stop and report it.

### Task 8: Draw the surface with the walkways

Files:
- Modify: `src/render/park_mesh.cpp`, buildParkMesh

Step 1: In buildParkMesh, gather connectorWalkways for the guest and backstage kinds into one vector. Draw parkSurface(world, walkways) in place of parkSurface(world). Delete the call appendWalkways(mesh, world, 1.0f). Leave the ghost's calls to appendWalkways as they are.

Step 2: Build the app.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build ends with no warnings or errors.

### Task 9: Check the simulation is unchanged

Step 1: Write the scenarios' output again and compare.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 100 tests/parks/main-street.park tests/parks/warm.park > build/joined-walkways/after.txt 2> /dev/null; build/windows-debug/tpj_scenarios.exe --compare build/joined-walkways/before.txt build/joined-walkways/after.txt; echo $?`
Expected: `0`

### Task 10: Capture in the release build

Step 1: Rebuild windows-release and capture main-street.park with it.

Run: `cmake.exe --build --preset windows-release && build/windows-release/ThemeParkJones.exe --park tests/parks/main-street.park --capture build/joined-walkways/release.bmp; echo $?`
Expected: the build has no warnings, and the last line is `0`.

Step 2: Crop the street from the capture and enlarge it three times, then look at it. Each shop's walkway shows as paving joined to its path, with no edge line across it there.

Run: `python3 -c "from PIL import Image; im=Image.open('build/joined-walkways/release.bmp'); im.crop((550,270,900,520)).resize((1050,750)).save('build/joined-walkways/release.png')" && echo cropped`
Expected: `cropped`

### Task 11: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest ends with `100% tests passed`, and tidy ends with `tidy: clean.`

Step 2: Confirm build/windows-release/ThemeParkJones.exe is newer than every source change in this feature, so the user plays it.

### Task 12: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/park/SPEC.md, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/routes/SPEC.md, src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/render/SPEC.md, src/render/park_mesh.cpp, plans/navigable-networks/wide-routes/MILESTONE.md, and plans/navigable-networks/wide-routes/joined-walkways/. Use the subject `Render: Join door walkways into the paved surface`.
