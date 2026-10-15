# Implementation Plan: Unit Connections

## Goal

Derive each door's connection and walkway, and the walked surface, in units, so a changed frame finds connections and hides the walked surface again only near what its edit changed.

## Approach

The unit mechanism learns to keep a kind over earlier kinds' units, named with UnitsOf, so routes can key units by park's boxes, entrances, points, and segments without naming park's private intent types. Park declares those kinds in a new public header and offers functions that read its units for another module's derivation. Routes then derives connections, walkways, their indexes, and the walked edge lines in units, and its two resolvers read them.

Write all the code, then build once and run each check once, in Task 12. Don't build or test between tasks.

## Placement

Decision 0027 places each behavior this feature adds:

- Kinds kept over other kinds: sim root, sim/units.h, with the table support in sim/unit_space.h and sim/schema.h. The sim root owns the unit mechanism.
- Nearest place and ground point on a line without a network: medium, sim/medium/network.h. The medium owns carrier geometry, and Network's own queries delegate to these.
- The box and entrance kinds and the unit reads: park, sim/park/unit_reads.h and unit_reads.cpp. Park owns its intent and the paving units, so it decides what other modules' units may read of them.
- changedPavingOwners: park, sim/park/surface.h. Park owns the paved surface's runs.
- Door connections, walkways, and their indexes: routes, sim/routes/internal/connections.h and connections.cpp. Routes owns the connector and walkway rules (routes SPEC, Connectors).
- The walked edge-line units and the walked-surface resolver: routes, sim/routes/internal/walked_surface.h and walked_surface.cpp. Routes owns the walked surface.
- Reading connections and walkways from units: routes, networks.cpp, which owns path-networks and keyedWalkways.
- Calling addParkUnits: src/sim/park_schema.cpp, composition only.

## Tasks

### Task 1: Write the specs

Files:
- Modify: `src/sim/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/park/SPEC.md`, `src/sim/routes/SPEC.md`

Step 1: Make each change FEATURE.md's Spec changes section gives, with its exact sentences.

### Task 2: Run the test pass

Dispatch the test-writer as the implementing-features skill says, with FEATURE.md, src/sim/SPEC.md, src/sim/park/SPEC.md, and src/sim/routes/SPEC.md, and no new public header. It extends tests/sim/unit_reach_test.cpp. Until Tasks 5 to 10 register the new kinds, derivedUnits throws for their names, so tests 3, 4, and 5 fail for that reason.

### Task 3: Keep kinds over other kinds

Files:
- Modify: `src/sim/units.h`, `src/sim/unit_space.h`, `src/sim/unit_space.cpp`, `src/sim/schema.h`, `src/sim/schema.cpp`

Step 1: In unit_space.h, give UnitTable two pure virtual functions: `holds(EntityKey) const`, true when the table has a unit or an entry for the key, and `appendKeys(std::vector<EntityKey> &) const`, which appends every such key in ascending order. KindTable implements them over Units. IndexTable implements them over Filed, in unit_space.cpp.

Step 2: In schema.h, UnitTableType gains `std::vector<TypeId> OverKindIds` and `std::vector<size_t> OverKinds`, with a comment: when OverKindIds isn't empty, the table is kept over those earlier unit kinds and Over is unused. addUnitTable in schema.cpp: when OverKindIds isn't empty, fill OverKinds with each id's position from unitTableOf, throwing std::invalid_argument naming the table when one isn't registered. Then set Reads from ReadIds alone and don't set Over. Otherwise keep what it does today. Update addUnitTable's comment in schema.h to match.

Step 3: In units.h, add UnitsOf with the comment FEATURE.md gives, and a trait IsUnitsOf. In unitTableType and ReadIdsOf, when Over is a UnitsOf, fill OverKindIds with typeIdOf of each of its kinds and leave OverId null. In updateUnits, when Over is a UnitsOf:
- At the first update, put in Dirty every key each over-kind table holds (appendKeys), then sortUnique.
- After that, put in Dirty the changed keys of each over-kind table (changedKeys), then the readers of the change list as today, then sortUnique. Logs aren't read.
- A key holds the kind's Over when any over-kind table holds it (holds), in place of `read.has<Over>(key)`.

Keep a typed Over exactly as it is. Update the UnitKind comment to say Over may be a UnitsOf.

### Task 4: Find places on a line without a network

Files:
- Modify: `src/sim/medium/network.h`, `src/sim/medium/network.cpp`

Step 1: Declare nearestPlaceOnLine and groundPointOnLine as FEATURE.md gives. Change projectOnto to take the carrier's key and points instead of the Carrier. nearestPlaceOnLine returns none for a point that isn't finite or no points, and otherwise projects as nearestPlaceOn does. groundPointOnLine returns none for a distance that is NaN, below 0, or past the last point's distance, and otherwise interpolates as groundPoint does. Network::nearestPlaceOn and Network::groundPoint find their carrier as today, then call these.

### Task 5: Declare park's kinds and register boxes and entrances

Files:
- Create: `src/sim/park/unit_reads.h`, `src/sim/park/unit_reads.cpp`
- Modify: `src/sim/park/internal/surface_units.h`, `src/sim/park/surface_units.cpp`, `src/sim/park_schema.cpp`

Step 1: Write unit_reads.h as FEATURE.md gives, with a comment on each function taken from park's SPEC.md Boundary sentences of Task 1. It includes sim/units.h, sim/park/intent.h, and sim/park/surface.h.

Step 2: In surface_units.cpp, move GroupLabel, GroupLabels, PointPavingUnits, SegmentPavingUnits, PointCells, SegmentCells, HiddenEdges, and SurfaceUnitLists out of the anonymous namespace into namespace tpj, with their definitions in internal/surface_units.h. Keep each derive's body in the .cpp file, declared in the struct. Move cellsOf there too, as `void cellsOf(const Bounds &bounds, std::vector<EntityKey> &keys)`.

Step 3: In unit_reads.cpp, define BoxUnits, named park-boxes, over BoxIntent, reading ShopStockIntent, whose value holds the ParkBox parkBox gives. Define EntranceUnits, named park-entrances, over EntranceIntent, whose value holds the ParkEntrance. Give each value a visitFields that walks every field, the pose's four numbers one by one. addParkUnits registers BoxUnits then EntranceUnits. In park_schema.cpp, addPark calls addParkUnits after addPavedSurface, and its comment names the units park keeps for other modules.

### Task 6: Read park's units for other modules

Files:
- Modify: `src/sim/park/unit_reads.cpp`, `src/sim/park/surface_units.cpp`, `src/sim/park/surface.h`, `src/sim/park/surface.cpp`

Step 1: In unit_reads.cpp, define the read functions. Each reads only units and indexes through reads, never a component:
- boxOf and entranceOf read the unit of BoxUnits or EntranceUnits.
- groundSegmentOf reads GroundLines' unit and gives its Segment.
- segmentsAt reads filed<SegmentEnds>.
- pavingNear clears pieces and makes the rectangle a Bounds. For each cell cellsOf gives, it reads filed<PointCells> and filed<SegmentCells>. Then, over the points and segments found, sorted without repeats, it puts in pieces every piece of each point's PointPavingUnits value, and each segment's body piece from its SegmentPavingUnits value when it has one.
- pointEdgeLines reads PointPavingUnits' unit and gives its Edges and EdgePoints, or empty spans.
- pavingCells calls cellsOf on the rectangle.

Step 2: In surface_units.cpp, split HiddenEdges::derive. A function `void hideNearJoined(UnitReads &reads, EntityKey label, std::span<const EdgeLine> edges, std::span<const ParkPoint> points, std::span<const SurfacePiece> extra, std::vector<EdgeLine> &outEdges, std::vector<ParkPoint> &outPoints)` holds today's body from the cells onward, taking its SurfaceUnitLists from reads.scratch(). It adds to the hiders each extra piece whose owner's label is the label, then calls hideOwner. HiddenEdges::derive calls it with no extra pieces. An owner's label comes from `const GroupLabel *labelOf(UnitReads &reads, EntityKey owner)`: the owner's GroupLabels unit when it has one, which a point with a ground-lined segment does, or else, when GroundLines has a unit for the owner, its segment's From point's GroupLabels unit, or null. hideNearJoined compares each extra piece's labelOf(piece.Owner) with the label. hideJoined in unit_reads.cpp takes labelOf(owner) and calls hideNearJoined, appending nothing when it is null. Declare hideNearJoined and labelOf in internal/surface_units.h.

Step 3: Add changedPavingOwners to surface.h with FEATURE.md's comment, defined in surface_units.cpp with the three changedUnits calls resolvePavedSurface makes. resolvePavedSurface calls it.

Step 4: Drop the parkSurface form that refills a surface with walkways from surface.h and surface.cpp.

### Task 7: Derive each door's connections

Files:
- Create: `src/sim/routes/internal/connections.h`, `src/sim/routes/connections.cpp`
- Modify: `src/sim/routes/networks.cpp`

Step 1: Move from networks.cpp into connections.cpp, declared in internal/connections.h where networks.cpp still uses them: addDoor, doorsServing, groupStarts, groupHolding, LineMeeting, pieceEntry, and connectorRoute. Drop linesOnly, ownerDistances, distanceOf, pavingDistances, nearestLine, rayEntry, and findConnections. Their work moves into this task's derivation.

Make the moved helpers allocate nothing once their lists have grown. groupStarts sorts a distance list it is given in place and fills a starts list the caller keeps. connectorRoute fills a points list the caller keeps and returns the distance along the path. addStops keeps its own lists for them, since the network it builds still allocates.

Step 2: In connections.h, define DoorConnection: the door (ParkDoor), the network kind, the connector's points, its width, the key of the path it meets, and the distance along that path. Define DoorConnectionList, holding a list of them, guest before backstage, each with a visitFields over every field.

Step 3: Define DoorConnections in connections.cpp, named door-connections, over UnitsOf<BoxUnits, EntranceUnits>, with no Reads. derive reads the entity through boxOf or entranceOf, and finds its doors with doorsServing's rules for each kind. For each door, it gives what findConnections gives for that door, from only the pieces near it:
- pavingNear over the square of half side CONNECTION_REACH plus JUNCTION_TOLERANCE around the door. Keep the pieces of the door's kind.
- Each owner's least pieceDistance to the door.
- The candidate paths: each owner that groundSegmentOf finds, and each segment segmentsAt gives for an owner that is a point. Keep those of the door's kind with a ground line, in ascending key order, without repeats.
- Each candidate's paving distance is the least of its own, its From point's, and its To point's owner distance. If the least over all candidates is above CONNECTION_REACH, the door has no connection.
- Of the candidates within JUNCTION_TOLERANCE of the least, the one whose line lies nearest, by nearestPlaceOnLine and groundPointOnLine, the first in key order on a tie.
- No connection when that reach is not above JUNCTION_TOLERANCE, or when groundSegmentOf finds a segment of the door's kind with a ground line at the connector's key.
- connectorRoute's route, with its ray entry taken over the kept pieces within CONNECTION_REACH plus JUNCTION_TOLERANCE, and its nearest place found with nearestPlaceOnLine on the met path's line.

Keep every list it fills in a struct from reads.scratch().

Step 4: Define ConnectionPaths, named connection-paths, a value index over DoorConnections that files an entity under the path of each of its connections.

### Task 8: Derive each door's walkways and their indexes

Files:
- Modify: `src/sim/routes/internal/connections.h`, `src/sim/routes/connections.cpp`

Step 1: In connections.h, define DoorWalkway: the connector's key, the KeyedWalkway's Walkway, whether it has a piece, and the piece. Define DoorWalkwayList, holding a list of them in the order of the entity's connections, with visitFields.

Step 2: Define DoorWalkways, named door-walkways, over UnitsOf<BoxUnits, EntranceUnits>, with no Reads. For each connection in the entity's DoorConnections unit:
- The path's stop distances are 0, the length of its ground line, and the distance of each connection on that path. Those come from the DoorConnections unit of each entity that filed<ConnectionPaths> files under the path. groupStarts groups them, and groupHolding finds the connection's group.
- The walkway joins the lowest key among the path and, when the group is the first, every segment of the kind with a ground line at the path's From point. When the group is the last, the same holds for its To point. Each segment so added whose ground line is no longer than JUNCTION_TOLERANCE adds the segments at its far point the same way.
- Its door, direction, and width come from the connection. It meets where keyedWalkways' rule puts it today, from the connector's points.
- Its piece is walkwayPiece over the joined segment alone, from groundSegmentOf.

Step 3: Define WalkwayCells, named walkway-cells, a value index over DoorWalkways that files an entity under the cells pavingCells gives for each piece's corners' bounding rectangle. Define WalkwayJoins, named walkway-joins, a value index that files it under the segment each walkway with a piece joins.

Step 4: `void addConnectionUnits(WorldSchema &schema)` registers door-connections, connection-paths, door-walkways, walkway-cells, and walkway-joins, in that order. Declare it in connections.h.

### Task 9: Read connections and walkways from the units

Files:
- Modify: `src/sim/routes/networks.h`, `src/sim/routes/networks.cpp`

Step 1: deriveNetwork takes, in place of the surface and the doors, the kind's connections in doorsServing's order: entrances by key, then boxes by key. resolvePathNetworks fills them into a list it keeps in its buffers, from parkEntrances and parkBoxes into kept lists and each entity's DoorConnections unit through unitValue. Each connection becomes a connector carrier keyed connectorKey(entity, face) and a meeting with its path's carrier, found by key, as findConnections' output did.

Step 2: keyedWalkways takes the same entities in kept lists, appends each walkway of the kind from their DoorWalkways units, and sorts what it appended by connector key. It no longer reads the network or the doors. Reword the comment on the appending forms in src/sim/routes/networks.h to say they keep the entrances and boxes they read in the buffers.

Step 3: addRoutes registers the components, then calls addConnectionUnits, then registers path-networks, then calls addWalkedSurface (Task 10), then registers route distance.

### Task 10: Derive the walked surface in units

Files:
- Create: `src/sim/routes/internal/walked_surface.h`, `src/sim/routes/walked_surface.cpp`
- Modify: `src/sim/routes/networks.cpp`

Step 1: Move WalkedSurface, WALKED_SURFACE_PURPOSE, resolveWalkedSurface, and walkedSurface from networks.cpp into walked_surface.cpp. Drop WalkedWalkways.

Step 2: Define WalkedPointEdges, named walked-point-edges, over UnitsOf<PointPavingUnits>, with no Reads. Its value says whether the point's own edge lines are walked, and holds them and their positions. derive takes pointEdgeLines. It finds the entities filed<WalkwayCells> files under the cells pavingCells gives for each edge line's positions, and the pieces of their DoorWalkways units. If there is no edge line or no such piece, the value says no and holds nothing. Otherwise it says yes and holds hideJoined's result for the point's edge lines with those pieces as the extra pieces.

Step 3: Define WalkedSegmentEdges, named walked-segment-edges, over UnitsOf<SegmentPavingUnits>, with no Reads. Its value holds the walkway pieces joined to the segment, and their edge lines after hiding with the positions. derive takes the walkways that join the segment and have a piece, from the DoorWalkways units of the entities filed<WalkwayJoins> files under it, in ascending connector key order. It lists their pieces, and for each piece the edge lines addWalkway makes: from each corner to the next, the last corner excepted, owned by the segment. It finds the walkway pieces near those edge lines as Step 2 does, and holds hideJoined's result for them.

Step 4: resolveWalkedSurface gathers the owners from changedPavingOwners and from changedUnits of both kinds. For each owner, it refills the owner's run in its component from the owner's run in resolvedSurface, or empties it when there is none. When the point's walked edges say yes, they replace the run's edge lines. When the segment's unit holds pieces, it appends them to the run's pieces and puts its edge lines in the run. When the run then holds no piece, edge line, or side, it erases the owner. Keep the owner list in its buffers.

Step 5: `void addWalkedSurface(WorldSchema &schema)` registers walked-point-edges, walked-segment-edges, and the walked-surface resolver, depending on path-networks. Declare it in internal/walked_surface.h. walked_surface.cpp includes internal/connections.h for the walkway types it reads.

Step 6: Add connections.cpp, walked_surface.cpp, and unit_reads.cpp to src/sim/CMakeLists.txt.

### Task 11: Watch the new steps in the allocation check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Add park-boxes, park-entrances, door-connections, connection-paths, door-walkways, walkway-cells, walkway-joins, walked-point-edges, and walked-segment-edges to WATCHED_STEPS, and update its comment.

### Task 12: Confirm the feature

Files: none.

main hasn't changed since unit-surface's final checks, so its outputs are the before outputs: build/unit-surface/scenarios-split2.txt, build/unit-surface/full-split2.txt, and build/unit-surface-3.report.

Step 1: Build both presets, run the tests, tidy, and the scenarios.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release && ctest.exe --preset windows-debug && scripts/tidy.sh && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/unit-connections-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/unit-connections-full.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/scenarios-split2.txt build/unit-connections-scenarios.txt && build/windows-debug/tpj_scenarios.exe --compare build/unit-surface/full-split2.txt build/unit-connections-full.txt`
Expected: no warning, every test passes, tidy prints "tidy: clean.", and both comparisons exit with status 0.

Step 2: Write the after report and compare it.

Run: `scripts/runtime-report.sh build/unit-connections.report && scripts/runtime-report.sh --compare build/unit-surface-3.report build/unit-connections.report`
Expected: no stage marked result-changed. shop-drag and path-drag are marked faster, each median at least 1 ms below the before report's.

Step 3: Read resolvePathNetworks, resolveWalkedSurface, keyedWalkways, and every unit derivation and read function this feature adds. Confirm criterion 2, that routes includes no park internal header, and that nothing they run allocates once its lists have grown.

### Task 13: Review and commit

Step 1: Stage only this feature's paths by pathspec, never with `git add -A`, and dispatch the reviewer on the staged diff through the reviewing skill. Present its findings verbatim.

Step 2: Commit with the commit-hygiene skill, subject `Sim: Derive door connections and the walked surface in units`. Check `git diff --stat` and amend with any files the hook formatted.
