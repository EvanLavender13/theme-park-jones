# Feature: Unit Surface

## Summary

unit-surface moves the paved surface onto units, so a changed frame derives paving only where its edit reaches.

The paving splits into units that each read only their neighbours:

- Each point's asks.
- Each point's fit: its trims, pairs, and taper.
- Each point's paving: its pieces, its corner pieces, its edge lines before hiding, and its turn halves.
- Each segment's paving: its pieces and its sides.

Hiding each point's edge lines is a unit too. It finds the pieces that can hide them through a grid filed from the pieces, and compares groups through labels. A label is the lowest point key in a group, so a path drawn from an existing one changes no label.

Both surfaces store their runs per owner. Their walk emits exactly the words it emits today, so every hash stays the same. The paved-surface resolver changes only the owners whose units changed. Every reader of either surface reads the runs in owner order, so it reads what it reads today.

The unit mechanism gains three things this needs:

- An index filed from an earlier kind's values.
- A kind that updates its own units, for the labels.
- A list of the units a resolution changed.

The allocation check's drags and commits, which kept-checks added, watch the surface's units and both surface resolvers too.

## Acceptance criteria

1. Neither drag gets slower, measured over the whole changed frame, and each stage that gets slower has a later feature that removes its cause. The runtime report, which times windows-release only, compares build/unit-surface-3.report with build/unit-surface-before.report, written on main before the change. The user amended this criterion on 2026-10-14 after three reports. It first asked for both drags marked faster, with shop-drag's median falling by at least 0.3 ms and path-drag's by at least 0.25 ms. The drags gained 0.25 to 0.5 ms each, about as much as one report differs from the next, so neither is marked faster. The stages marked slower, and the features that remove their causes:
    - preview and path-preview on winding-path copy the whole world, whose surface and units now hold a list per owner. The app copies a world only on a drag's first frame. layered-candidate removes the copy.
    - path-ghost on full.park and ghost on winding-path build the surface around the edit from layouts, twice. layered-candidate and spatial-units make the ghost read the units.
    - park-mesh on full.park walks the surface's runs per owner, 8 µs more than the flat lists.

    (check: scripts/runtime-report.sh build/unit-surface-3.report, then --compare with build/unit-surface-before.report)
2. A changed frame derives paving only where its edit reaches. Take eight Very Sleepy profiles of windows-release tpj_bench --ticks 1 on full.park, and look at the samples inside resolveWorld under each drag's timing function:
    - No sample holds buildSurface or hideJoined without also holding walkedSurfaceOf. Only the walked surface still builds and hides the whole park, until unit-connections.
    - Under the shop drag's, no sample holds the paving units' derivations, askAt, fitAt, paveAt, paveSegment, or hideOwner, without walkedSurfaceOf. A moved box changes no path intent.
    - Under the path drag's, the samples that hold them without walkedSurfaceOf are under 1.5% of the samples inside resolveWorld under the drag. In the derived-units profiles, the paved surface was about 7.7% of those samples, so this is under a fifth of it.

    (check: the profiles PLAN.md's last implementation task runs)
3. The result is exact:
    - The save checks pass on every chosen edit. A world that commits each edit resolves the surface's units incrementally, and the world loaded from its save resolves them from nothing.
    - The candidate check passes, including the drags on warm.park that compare each frame's kept candidate with its own save, loaded and resolved.
    - tpj_scenarios --compare of windows-debug output before and after the feature shows no difference, for its scenarios, every park in tests/parks, and full.park over 300 ticks. That includes every world hash, so both surfaces walk the words they walked.
    - The runtime report's --compare marks no stage result-changed.

    (ctest; tpj_scenarios --compare; the report's --compare)
4. The allocation check's drags and commits watch the surface's eight units and the paved-surface and walked-surface resolvers, which they paused around until now. They still pause only around the resolvers no feature has moved yet and the finishers. The check fails if watched work frees a block that watched work allocated. (ctest: the allocation check)
5. Every reader of either surface reads its runs in owner order: the path networks, guest ground, furniture, the acceptance check and edited surface, the ghost, the render meshes, the paving overlay, and the tools. The paved-surface resolver reads no whole-park list: no worldGround, layoutOf, or parkSegments. (check: read each reader and the resolver)
6. Edits derive the units predicted: nothing when nothing changed or the edit is one no unit reads, the same units however much else the park holds, and none past what a change's values reach. (the listed tests)
7. src/sim/SPEC.md describes the new mechanisms, and src/sim/park/SPEC.md the surface's runs and the new unit names. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

The save checks and the candidate check's drags already compare incremental units with units derived from nothing, and the allocation check gains the drags as criterion 4 says. These tests check which units an edit derives, through derivedUnits, for the ground's kinds and the surface's: straight-lines, point-arms, corners, ground-lines, group-labels, point-asks, point-fits, point-paving, segment-paving, and hidden-edges.

1. Nothing changed: load full.park, which resolves it, then resolve it again with no command. No unit of any kind the world's schema registers derives, so a kind a later feature registers is covered without a change to this test.
2. An edit no unit reads: on full.park, commit a MoveBox that moves a shop a few meters along its paving, and check the box moved. No unit of any of the ten kinds derives.
3. Park size: take a small park from tests/parks, and a copy of it with a long branch added by AddPath commands. The branch starts from a point of the small park's network, so it joins that network, and runs out to at least 100 meters from where the edit happens. Resolve both. In both, commit the same AddPath from the same existing point of the small park to a new point a few meters away, far from the branch. For each kind, the units derived for keys that existed before the edit are the same in both worlds, and as many units derive for keys the edit made. A kind that refilled the whole joined network would derive more in the copy.
4. A change nothing downstream sees: in a park with a point where exactly two segments of the same width meet, commit SetTaperLength on that point with a length above 0, and check the point holds it. The taper only shapes a point where widths differ, so units derive only for that point, its two segments, and the points at their other ends. No group-labels unit derives.

## Medium

None. Units, indexes, and labels are how resolution stores its work. They never appear in a world's walk. The surfaces keep the words they walk, so no field or flow changes.

## Spec changes

src/sim/SPEC.md's Units section gains, after its second paragraph:

    An index can also file each unit of an earlier kind under keys it names from the unit's value. When the unit is added, removed, or derives a different value, the index files it again.

    A module can also register a kind that updates its own units. At each resolution, its update reads what earlier kinds and indexes changed, reads their units and entries by key, and sets or removes its own units. At its first update, it is given every entity that holds its intent component. Resolution records a change for each unit the update adds, removes, or sets to a value whose words differ, so the units that read it derive again, as for any kind. A module that registers such a kind makes its units equal to what deriving them from nothing gives.

    changedUnits gives the keys of a kind's units that the running resolution added, removed, or changed, in ascending order. A resolver that runs after the kind reads it to change only what changed.

src/sim/SPEC.md's Memory in play section gains, after its first paragraph:

    KeptMap holds values by key in ascending order. It keeps the nodes of removed values for later adds, so adds and removes allocate nothing once it has grown. Copying one copies its values. Assigning one to another reuses the other's nodes and their memory.

src/sim/park/SPEC.md's Boundary gains, after the sentence on addPathGround:

    addPavedSurface also registers the units the paved surface derives in, named group-labels, point-asks, point-fits, point-paving, segment-paving, point-cells, segment-cells, and hidden-edges, so those names are taken.

The Boundary's sentence on parkSurface taking TickBuffers gains:

    The form with walkways can also refill a surface the caller keeps, and then allocates nothing once that surface and the lists have grown.

src/sim/park/SPEC.md's Paved surface section changes its first sentence to:

    parkSurface gives the paved surface the park's segments make. For each segment or point that owns paving, in ascending key order, it holds that owner's pieces and edge lines. The surface's Owners list them, and pavingOf gives one owner's, or none when it owns no paving.

Its sentence on sidePoints, in the paragraph on sides, becomes:

    sidePoints gives the positions a side of an owner's paving runs through, from its From end to its To end, and the side says which of them is where its body starts. edgePoints gives the positions an edge line of an owner's paving runs through.

It gains, after the sentence on resolvedSurface before a world first resolves:

    A surface walks as one list of every owner's pieces, one of every edge line, and one of every side, each in ascending key order of its owner, with each edge line's and side's positions counted from the start of all of them.

Public interface, in src/sim/kept_map.h, moved from src/sim/unit_space.h:

    // Values by key, in ascending key order, whose removed nodes are kept for later adds, so that
    // adds and removes allocate nothing once the map has grown. A copy holds the same values without
    // the kept nodes. Assigning reuses this map's nodes and their memory.
    template <typename Value> class KeptMap {
    public:
      KeptMap() = default;
      KeptMap(const KeptMap &other);
      KeptMap &operator=(const KeptMap &other);
      KeptMap(KeptMap &&) noexcept = default;
      KeptMap &operator=(KeptMap &&) noexcept = default;
      [[nodiscard]] Value *find(EntityKey key);
      [[nodiscard]] const Value *find(EntityKey key) const;
      std::pair<Value &, bool> getOrAdd(EntityKey key);
      bool erase(EntityKey key);
      void assignFrom(const KeptMap &other);
      [[nodiscard]] size_t size() const;
      [[nodiscard]] bool empty() const;
      // The values in ascending key order, as (key, value) pairs.
      [[nodiscard]] auto begin() const;
      [[nodiscard]] auto end() const;
    };

In src/sim/units.h:

    // An index over an earlier unit kind: a Name, the kind Of, and
    // keysOf(const typename Of::Value &, std::vector<EntityKey> &), which clears the list and puts in
    // the keys to file the unit under.
    template <typename I> concept UnitValueIndex = ...;
    template <UnitValueIndex I> void addUnitIndex(WorldSchema &schema);

    // What a kind that updates its own units reads and writes in its update.
    template <typename Value> class UnitUpdate {
    public:
      // True at the kind's first update, when it holds no units.
      [[nodiscard]] bool first() const;
      // Appends every key that holds the kind's intent component.
      void overKeys(std::vector<EntityKey> &keys) const;
      // Appends the keys of an earlier kind's units, or of an earlier index's entries, that the
      // running resolution changed, ascending.
      template <typename T> void changed(std::vector<EntityKey> &keys) const;
      template <typename K> const typename K::Value *unit(EntityKey key) const;
      template <typename I> std::span<const EntityKey> filed(EntityKey key) const;
      // The kind's own unit for the key, or null.
      [[nodiscard]] const Value *held(EntityKey key) const;
      // Sets the key's unit, recording a change when it adds the unit or its words differ.
      void set(EntityKey key, const Value &value);
      // Removes the key's unit, recording a change when it held one.
      void remove(EntityKey key);
      TickBuffers &scratch();
    };

    // A kind that updates its own units: a Name, an intent component Over, a Value with a visitFields
    // function, and update(UnitUpdate<Value> &).
    template <typename K> concept SelfUpdatingKind = ...;
    template <SelfUpdatingKind K> void addUnits(WorldSchema &schema);

    // Appends the keys of the kind's units that the running resolution added, removed, or changed,
    // ascending.
    template <typename K> void changedUnits(const World &world, std::vector<EntityKey> &keys);

In src/sim/park/surface.h, replacing PavedSurface and the two span functions:

    // One owner's paving: its pieces, its edge lines, the positions they run through, its sides, and
    // the positions they run through. Each EdgeLine's and SegmentSide's First counts from the start of
    // this owner's positions.
    struct OwnerPaving {
      std::vector<SurfacePiece> Pieces;
      std::vector<EdgeLine> Edges;
      std::vector<ParkPoint> EdgePoints;
      std::vector<SegmentSide> Sides;
      std::vector<ParkPoint> SidePoints;
    };

    // The paving the park's segments make, by owner, in ascending key order. Owners with no paving
    // are left out.
    struct PavedSurface {
      KeptMap<OwnerPaving> Owners;
    };

    // Walks every owner's pieces as one list, then every edge line, its positions, every side, and
    // its positions, each list in owner order, with each First counted from the start of the whole
    // list. A surface is derived, so saves never hold it and it is never read back.
    template <typename Visitor> void visitFields(Visitor &visitor, PavedSurface &surface);

    // As the form with walkways, refilling into and keeping the lists it works with in the buffers.
    // Allocates nothing once into and the lists have grown.
    void parkSurface(const World &world, std::span<const Walkway> walkways, PavedSurface &into,
                     TickBuffers &buffers);

    // The owner's paving, or null when it owns none.
    const OwnerPaving *pavingOf(const PavedSurface &surface, EntityKey owner);
    // The positions the edge line of the owner's paving runs through.
    std::span<const ParkPoint> edgePoints(const OwnerPaving &paving, const EdgeLine &edge);
    // The positions the side of the owner's paving runs through.
    std::span<const ParkPoint> sidePoints(const OwnerPaving &paving, const SegmentSide &side);

## Files affected

- Create: src/sim/kept_map.h
- Create: src/sim/park/internal/paving.h, src/sim/park/paving.cpp
- Create: src/sim/park/internal/surface_units.h, src/sim/park/surface_units.cpp
- Modify:
    - src/sim/unit_space.h
    - src/sim/units.h
    - src/sim/schema.h
    - src/sim/schema.cpp
    - src/sim/CMakeLists.txt
    - src/sim/SPEC.md
- Modify:
    - src/sim/unit_space.cpp
    - src/sim/park/surface.h
    - src/sim/park/surface.cpp
    - src/sim/park/internal/layout.h
    - src/sim/park/internal/ground_units.h
    - src/sim/park/ground_units.cpp
    - src/sim/park/edits.cpp
    - src/sim/park/SPEC.md
- Modify: src/sim/routes/networks.cpp, src/sim/guests/ground.cpp, src/sim/furniture/furniture.cpp
- Modify: src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/paving_overlay.cpp
- Modify: src/tools/frontage.cpp, src/tools/internal/frontage.h, src/tools/tools.cpp
- Modify: tests/integration/crossing_split_test.cpp, only to read the runs
- Modify: tests/checks/allocations/allocation_check.cpp

## Dependencies

- derived-units: units, recorded reads, early cutoff, the ground units, and the drag stages. Merged.
- kept-checks: the acceptance check, applying a command, and the ghost plan once in the caller's kept lists, so every surface they build refills kept storage. It also adds the resolve trace and the allocation check's drags and commits.
- flat-ground: the kept lists of the walkways and the ghost. Merged.
- kept-checks: the edit checks planned once in kept lists, the resolve trace, and the allocation check's drags and commits. Merged.
- The milestone's decisions don't bear on this feature.

## Out of scope

- The walked surface's walkways, its hiding near them, and the doors' connections: unit-connections. Until then, the walked-surface resolver lists every walkway and builds and hides the whole walked surface, with buildSurface. It refills its component's runs in place, so it allocates nothing once they have grown.
- Deriving surfaces on hypothetical layouts only where the edit reaches, for the acceptance check, editedSurface, surfaceAround, pointsAffected, and the ghost: layered-candidate. Until then they derive the whole layout, through the same per-point functions the units use, into the surfaces and lists kept-checks keeps for them.
- The path networks', guest ground's, and furniture's whole-park reads of the surfaces and of the ground: unit-connections and spatial-units.
- keepToolPark's parkEdges and parkTapers fit the whole layout once per world change. That isn't a changed frame, and the tools' lists hold every point and segment by design.

## Open questions

None.
