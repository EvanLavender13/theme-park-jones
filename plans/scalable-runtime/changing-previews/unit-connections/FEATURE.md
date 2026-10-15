# Feature: Unit Connections

## Summary

unit-connections moves each door's connection and walkway, and the walked surface, onto units. A changed frame then finds connections and re-hides the walked surface only near what its edit changed.

Today, the path-networks resolver measures every door against every piece of paving, and the walked-surface resolver builds and hides the whole paved surface a second time with the walkways added. After this feature:

- Each box and entrance has a unit holding its doors' connections, found from the paving near each door.
- Each box and entrance has a unit holding its walkways, found from its connections and the other connections on the same paths.
- Each point has a unit holding its edge lines hidden again with the walkway pieces near them, used only when a walkway piece lies near.
- Each segment has a unit holding the walkways joined to it and their hidden edge lines.
- The walked-surface resolver copies the paved surface's run for each owner and adds what those units hold, only for the owners whose units changed.
- The path-networks resolver reads the connections from the units, and keyedWalkways reads the walkways from them.

Routes' units belong to park's boxes, entrances, points, and segments, whose intent types are private to park. So the unit mechanism gains one thing: a kind can be kept over earlier kinds' units instead of over an intent component. Park declares four of its kinds in a public header, by name only, for this. It also offers public functions that read its units for another module's derivation, recording what they read. Routes reads nothing else of park's units.

## Acceptance criteria

1. Both drags get faster. The runtime report, which times windows-release only, compares a report of the feature with build/unit-surface-3.report, unit-surface's last report, written on the code main holds now. It marks shop-drag and path-drag faster, and each one's median falls by at least 1 ms. In derived-units' profiles, finding the connections and the walked surface took about 2.5 ms of each drag's frame. (check: scripts/runtime-report.sh build/unit-connections.report, then --compare build/unit-surface-3.report build/unit-connections.report)
2. A changed frame finds connections and hides the walked surface only near what its edit changed. The path-networks resolver measures no door against any piece. It reads each door's connection from the units. The walked-surface resolver changes only the owners whose paved run or walked units changed, and reads no whole-park list: no parkSurface, worldGround, layoutOf, parkSegments, or keyedWalkways. (check: read both resolvers; the listed tests)
3. The result is exact:
    - The save checks pass on every chosen edit. A world that commits each edit resolves the connection and walked units incrementally, and the world loaded from its save resolves them from nothing.
    - The candidate check passes, including the drags on warm.park.
    - tpj_scenarios --compare of windows-debug output before and after the feature shows no difference, for its scenarios, every park in tests/parks, and full.park over 300 ticks. That includes every world hash, so the walked surface, the networks, furniture, and guest ground are what they were.
    - The runtime report's --compare marks no stage result-changed.

    (ctest; tpj_scenarios --compare; the report's --compare)
4. The allocation check's drags and commits watch every unit kind and index this feature registers, as well as walked-surface. They still pause only around path-networks and the other resolvers no feature has moved yet, and around the finishers. (ctest: the allocation check)
5. Routes reads park only through park's public headers. Its units are kept over park's kinds through the declarations in sim/park/unit_reads.h, and read park's units only through that header's functions. (ctest: "private header check passes on the tree")
6. Edits derive the units predicted, as the listed tests say.
7. src/sim/SPEC.md describes kinds kept over other kinds. src/sim/medium/SPEC.md, src/sim/park/SPEC.md, and src/sim/routes/SPEC.md describe their new public functions and unit names. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

The save checks and the candidate check's drags already compare incremental units with units derived from nothing. tests/sim/unit_reach_test.cpp checks which units an edit derives. It gains this feature's kinds: park-boxes, park-entrances, door-connections, door-walkways, walked-point-edges, and walked-segment-edges.

1. Nothing changed: the existing test already covers every kind the schema registers. No change.
2. An edit no unit of the ground or the surface reads: the existing test, moving a shop, keeps its ten kinds. This feature's kinds do derive for that edit, which test 5 covers.
3. Park size: the existing test, drawing a path in a small park and in a copy with a long branch, also checks this feature's six kinds. For each of them, the units derived for keys that existed before the edit are the same in both worlds, and as many derive for keys the edit made.
4. A change nothing downstream sees: the existing test, setting a taper length where two segments of the same width meet, also checks this feature's six kinds. Each of them derives units only for that point, its segments, and the points at their other ends.
5. Moving a shop derives connection units only near it. On full.park, commit a MoveBox that moves a shop 3 m along its face, as test 2 does, and check the box moved. Record, before and after the move, the shop's position and each guest and backstage network from parkNetwork. Then:
    - park-boxes and door-connections derive only the shop's key, and park-entrances derives nothing.
    - door-walkways derives only for the shop and for entities that share a path with it. An entity shares a path with the shop when, in the network before or after, one of its connectors' last stop is at a node that also holds a segment's carrier that the last stop of one of the shop's connectors holds. A connector is a carrier whose first stop's node is anchored. nodeAnchor gives its entity, and stopPlaces gives the carriers at a node.
    - walked-point-edges and walked-segment-edges derive only for keys that are owners of the walked surface, before or after, whose pieces or edge-line positions come within 40 m of the shop's position before or after. Segments that a walkway of an entity in the door-walkways list joins, before or after, are allowed too. keyedWalkways gives each walkway's Joins.

## Medium

None. Units, their indexes, and the reads park offers are how resolution stores its work. They never appear in a world's walk. The walked surface, the networks, and the walkways keep the values they had, so no field or flow changes.

## Spec changes

src/sim/SPEC.md's Units section gains, after the paragraph on kinds that update their own units:

    A unit kind can also be kept over earlier kinds instead of over an intent component. It keeps a unit for each key that any of those kinds holds a unit for, and its units are added and removed with theirs. Its module names those kinds with UnitsOf. The module that owns them can declare them in its public header without defining them, so the kind's module learns nothing of their values except what that module's public functions give.

src/sim/medium/SPEC.md's paragraph on groundPoint and nearestPlaceOn gains, after the sentence on nearestPlaceOn:

    nearestPlaceOnLine and groundPointOnLine do the same on one carrier's line, given by its key and points, without a network. They give exactly what nearestPlaceOn and groundPoint give for a carrier with that key and those points.

and its list of straight-line measures names nearestPlaceOnLine with nearestPlace and nearestPlaceOn: "The first three and that check are used only to snap a ground position onto a network".

src/sim/park/SPEC.md's Boundary gains, after the sentence on addPavedSurface:

    addParkUnits registers the units park keeps for other modules' units, named park-boxes, one for each box, and park-entrances, one for each entrance, so those names are taken. makeParkSchema calls it after addPavedSurface. sim/park/unit_reads.h declares BoxUnits, EntranceUnits, PointPavingUnits, and SegmentPavingUnits, so that another module's unit kinds can be kept over them: one unit for each box, each entrance, each path point, and each path segment.

    Another module's unit derivations read park's units only through the functions in sim/park/unit_reads.h. Each records what it reads, so the derivation runs again when that changes. boxOf gives the box a key holds, as parkBox gives it, and entranceOf the entrance, as parkEntrances gives it, or null when the key holds none. groundSegmentOf gives the segment a key holds, as parkSegments gives it without its furnishing, or null. segmentsAt gives the keys of the segments that end at a point, ascending. pavingNear gives every piece of the paved surface whose corners' bounding rectangle meets a rectangle. It can also give other pieces of the paved surface, and a piece more than once. pointEdgeLines gives a point's edge lines before hiding. hideJoined hides edge lines as the paved surface hides an owner's, in a surface that also holds the extra pieces it is given. It keeps the parts that no piece joined to the owner covers. pavingCells gives the keys of the cells of a fixed grid that a rectangle covers, so two rectangles that meet share a key. changedPavingOwners gives the owners whose run in the paved surface the running resolution added, removed, or changed.

and drops the sentence "The form with walkways can also refill a surface the caller keeps, and then allocates nothing once that surface and the lists have grown."

src/sim/routes/SPEC.md's Boundary changes its first sentence to:

    routes reads intent only through parkSegments, parkEntrances, parkBoxes, footprintOf, parkSurface, resolvedSurface, changedPavingOwners, pieceDistance, and walkwayPiece, and its units read park's units only through the functions in sim/park/unit_reads.h (sim/park/SPEC.md, decision 0025).

and gains, after the sentence on addRoutes:

    addRoutes also registers the units its connectors and walked surface derive in, named door-connections, connection-paths, door-walkways, walkway-cells, walkway-joins, walked-point-edges, and walked-segment-edges, so those names are taken.

Its paragraph on keyedWalkways changes its second sentence to:

    Both can also append their walkways to a list the caller keeps, keeping the entrances and boxes they read in TickBuffers the caller keeps.

Its other paragraphs on connectors and walkways don't change. keyedWalkways and connectorWalkways still give the same walkways in the same order.

Public interface, in src/sim/units.h:

    // Names the earlier unit kinds a kind is kept over, as its Over, in place of an intent
    // component: it keeps a unit for each key any of them holds a unit for. A kind's module can
    // declare it without defining it.
    template <typename... Kinds> struct UnitsOf {};

In src/sim/medium/network.h:

    // The place nearestPlaceOn gives on a carrier with the key and points, or none when the point
    // isn't finite or there are no points.
    std::optional<Place> nearestPlaceOnLine(EntityKey carrier, std::span<const CarrierPoint> points,
                                            GroundPoint point);
    // The ground position groundPoint gives at the distance on a carrier with the points, or none
    // when the distance is outside the line or not a number.
    std::optional<GroundPoint> groundPointOnLine(std::span<const CarrierPoint> points,
                                                 double distance);

In src/sim/park/unit_reads.h, created:

    // Unit kinds park keeps, which another module's unit kinds can be kept over with UnitsOf: one
    // unit for each box, each entrance, each path point, and each path segment.
    struct BoxUnits;
    struct EntranceUnits;
    struct PointPavingUnits;
    struct SegmentPavingUnits;

    // Registers park-boxes and park-entrances.
    void addParkUnits(WorldSchema &schema);

    // A point's edge lines before hiding, and the positions they run through.
    struct EdgeRun {
      std::span<const EdgeLine> Edges;
      std::span<const ParkPoint> Points;
    };

    // These read park's units for another module's unit derivation, recording each read.
    const ParkBox *boxOf(UnitReads &reads, EntityKey key);
    const ParkEntrance *entranceOf(UnitReads &reads, EntityKey key);
    const ParkSegment *groundSegmentOf(UnitReads &reads, EntityKey key);
    std::span<const EntityKey> segmentsAt(UnitReads &reads, EntityKey point);
    // Puts in pieces, clearing it first.
    void pavingNear(UnitReads &reads, ParkPoint least, ParkPoint most,
                    std::vector<const SurfacePiece *> &pieces);
    EdgeRun pointEdgeLines(UnitReads &reads, EntityKey point);
    // Appends to the out lists. Each edge line's First counts from the start of points.
    void hideJoined(UnitReads &reads, EntityKey owner, std::span<const EdgeLine> edges,
                    std::span<const ParkPoint> points, std::span<const SurfacePiece> extra,
                    std::vector<EdgeLine> &outEdges, std::vector<ParkPoint> &outPoints);
    // Appends the cells' keys.
    void pavingCells(ParkPoint least, ParkPoint most, std::vector<EntityKey> &cells);

In src/sim/park/surface.h:

    // Adds to the owners those whose run in the paved surface the running resolution added,
    // removed, or changed, keeping the list ascending without repeats.
    void changedPavingOwners(const World &world, std::vector<EntityKey> &owners);

and drops `void parkSurface(const World &world, std::span<const Walkway> walkways, PavedSurface &into, TickBuffers &buffers)`, which nothing calls after this feature.

## Files affected

- Modify: src/sim/units.h, src/sim/unit_space.h, src/sim/unit_space.cpp, src/sim/schema.h, src/sim/schema.cpp, src/sim/SPEC.md
- Modify: src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/medium/SPEC.md
- Create: src/sim/park/unit_reads.h, src/sim/park/unit_reads.cpp
- Modify: src/sim/park/internal/surface_units.h, src/sim/park/surface_units.cpp, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/park/SPEC.md
- Modify: src/sim/park_schema.cpp, composition only: it calls addParkUnits after addPavedSurface.
- Create: src/sim/routes/internal/connections.h, src/sim/routes/connections.cpp, src/sim/routes/internal/walked_surface.h, src/sim/routes/walked_surface.cpp
- Modify: src/sim/routes/networks.h, src/sim/routes/networks.cpp, src/sim/routes/SPEC.md
- Modify: src/sim/CMakeLists.txt
- Modify: tests/checks/allocations/allocation_check.cpp
- Modify: tests/sim/unit_reach_test.cpp, in the test pass

## Dependencies

- unit-surface: the paving units, the cell indexes, the group labels, and the per-owner runs. Merged.
- kept-checks: the resolve trace and the allocation check's drags and commits. Merged.
- The milestone's decisions don't bear on this feature.

## Out of scope

- The path-networks resolver still builds both networks whole, numbering every node from every carrier and connection: spatial-units.
- Furniture and guest ground still read keyedWalkways and the walked surface whole: spatial-units.
- The acceptance check, editedSurface, surfaceAround, and the ghost still derive surfaces on hypothetical layouts: layered-candidate.
- walkedSurfaceOf and the returning parkSurface form with walkways stay for the crossing test, which builds a walked surface without resolving.

## Open questions

None.
