# Feature: Derived Units

## Summary

derived-units builds the mechanism the rest of changing-previews moves resolution onto, and puts the path ground on it first.

A module registers unit kinds and indexes with the schema, in its resolver order:

- A unit kind derives one value for each entity that holds a given intent component.
- Its derivation reads only by key, through a recorder: intent components, earlier kinds' units, and the keys an index files under a key.
- The world logs each intent key a command touches.
- Resolution re-derives only the units whose recorded reads changed, in registration order.
- A unit that comes out equal stops there: the units that read it don't re-derive.

The path ground, meaning each segment's ground line and each point's corner, splits into five such steps. Every world reader of the ground takes it from the units when they are current, instead of deriving the whole layout's ground again. Today that happens about five times per resolution.

The feature also adds two bench stages, a scripted shop drag and a scripted path drawing on full.park. It measures them before the mechanism lands, so every later feature of the milestone reports against them.

## Acceptance criteria

1. tpj_bench times two new stages, after path-ghost: shop-drag and path-drag. Each runs WARM_UPS + REPETITIONS frames on a copy of the park that steps one tick before each frame. It times only the frames after the warm-ups. Each frame's edit differs from the last, and a frame times keepPreview with that edit and buildGhostMesh of what it made. Each stage's result is the last candidate's hash.
    - shop-drag moves the lowest-keyed shop along its face, DRAG_STEP metres a frame, out to DRAG_SPAN and back, so every frame is accepted.
    - path-drag draws a guest path from the middle of pathSpur's segment to a point that starts DRAG_START_REACH beyond its side and moves DRAG_STEP further out on each frame.

    Before the mechanism lands, the runtime report on this branch is written to build/derived-units-before.report. After the feature, a second report is written and compared with it. (check: scripts/runtime-report.sh build/derived-units-before.report, then build/derived-units-after.report, then --compare)
2. A changed frame derives the ground again only where its edit reaches. Take eight Very Sleepy profiles of windows-release tpj_bench --ticks 1 on full.park after the feature, and look at the samples inside resolveWorld under each drag's timing function:
    - Under the path drag's, the samples that hold straightLineOf, armsAt, cornerAt, or groundLineOf, the functions both groundOf and the ground units derive with, are under 0.15% of them. Deriving the whole layout's ground once is about 0.6% of a resolution, so a frame that derived every ground unit fails this. Edits on hypothetical layouts, such as the acceptance check's, still derive the whole layout's ground outside resolveWorld until layered-candidate.
    - Under the shop drag's, none hold those functions. A moved box changes nothing the ground reads, and the candidate carries the world's units.

    (check: the profiles PLAN.md's last implementation task runs)
3. Resolution is exact:
    - The save checks pass on every chosen edit, including a new one that moves warm.park's shop. A world that commits each edit resolves its units incrementally. The world loaded from its save resolves every unit from nothing. The two compare equal.
    - The candidate check passes: a candidate copies the world's units and resolves incrementally, and it equals the world that commits the same commands.
    - tpj_scenarios --compare of windows-debug output before and after the feature shows no difference, for its scenarios, every park in tests/parks, and full.park over 300 ticks.
    - The runtime report's --compare marks no stage result-changed.

    (ctest, including the drag check in Tests; tpj_scenarios --compare; the report's --compare)
4. Units never reach a world's walk, so hashes, saves, and comparisons are as before. (ctest: the copy, save, and hash checks) A copy carries the units and their recorded reads, so a candidate resolves only what its commands changed. (check: criterion 2's shop drag)
5. Once their lists have grown, these allocate nothing, as docs/conventions.md requires of per-frame work:
    - the units' updates: the change logs, the dirty lists, the recorded reads, the readers' lists, and the ground units' values;
    - the shared ground the world readers fill from the caller's buffers. The five resolvers that call returning readers today, for the paved surface, the walked surface, the networks, guest ground, and furniture, pass their buffers instead. The surfaces they build still allocate until unit-surface;
    - the kept preview's refill of its candidate world, units included, and its command queue. On each changed frame, the preview refills one kept candidate from the world, instead of making a new world.

    The resolvers' own reader lists, the acceptance check's, and the shop context's stay with unit-surface, layered-candidate, and spatial-units. (check: read each function the listed work calls)
6. src/sim/SPEC.md describes units, and src/bench/SPEC.md the two new stages. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

- The park world checks gain one chosen edit of warm.park: its lowest-keyed shop moved 1 m along its face, so that its door's connection meets its guest path at a new place. The edit must be accepted. The save checks and the candidate check then run on it as on every chosen edit, with no new kind of check. Until now no chosen edit moved a box. This is how the world checks compare an incremental resolution after a box move with a resolution from nothing.
- The candidate check gains a drag, on warm.park, the way the preview makes candidates. The world steps one tick before each of eight frames. Each frame refills one kept candidate with makeCandidate(world, commands, buffers, into), using the frame's edit, and keeps the same buffers and kept world across frames. There are two drags:
    - The lowest-keyed shop moved 0.5 m further along its face on each frame.
    - A guest path from the middle of a guest segment whose end moves 0.5 m further out from the segment's side on each frame.

    After each frame, the kept candidate equals the candidate the returning makeCandidate gives for the same edit, and it equals the world loaded from its own save and resolved. Each frame's edit must be accepted. The second comparison is what catches a kept world that holds stale units from an earlier frame, because a loaded world derives every unit from nothing.

## Medium

None. Units, indexes, change logs, and recorded reads are how resolution stores its work. They never appear in the walk, so they add no field or flow and change no value the simulation or the display gives.

## Spec changes

src/sim/SPEC.md gains a section after The tick cycle:

    ## Units

    A module can register unit kinds and indexes with the schema, in its resolver order. A unit kind derives one value for each entity that holds a given intent component. Its derivation reads intent components, earlier kinds' units, and the keys an index files under a key, each by key, and nothing else. An index files each entity that holds a given intent component under the keys it names for that component.

    Resolution keeps each unit's value and what its derivation read. A unit derives again only when its entity gains or loses the component, or when something it read has changed since. A command's use of an intent component counts as a change to it. If a unit derives again and its value comes out equal, word for word as the walk compares values, the units that read it don't derive again for it. So a resolution gives the same units as deriving every unit, whatever units the world kept.

    A new world and a loaded world hold no units, so their first resolution derives every unit. copyWorld carries the units and what they read, so a copy resolves only what changed since. copyWorld(world, into) makes into equal to the world, units included, reusing into's memory. It throws std::invalid_argument when into doesn't share the world's schema object. makeCandidate(world, commands, buffers, into) makes into the candidate. Units are never part of the walk, so hashes, saves, and comparisons never see them. unitValue gives a unit's value as the last resolution left it. unitsCurrent says whether a kind's units hold every change made to the world.

src/sim/park/SPEC.md's Boundary gains, after the sentence on addParkEdits:

    addPathGround registers the units the path ground derives in, named segment-ends, straight-lines, point-arms, corners, and ground-lines, so those names are taken. makeParkSchema calls it before addPavedSurface.

The Boundary gains, after the sentence on parkSegments, parkCorners, parkEdges, and parkTapers keeping their lists in TickBuffers:

    parkSurface, with or without walkways, can also take TickBuffers the caller keeps, and keeps the lists that deriving the segments' lines works with there. It still returns a new surface, so it allocates on every call.

parkSegments, parkCorners, parkSurface, parkEdges, and parkTapers give what they gave.

src/bench/SPEC.md: the stage list in benchPark gains, after path-ghost:

    - shop-drag, only when the park holds a shop: a drag of the lowest-keyed shop along its face, DRAG_STEP metres a frame, out to DRAG_SPAN from where it stands and back, on a copy of the park that steps one tick before each frame. Each frame times keepPreview with the frame's MoveBox and buildGhostMesh of what it made.
    - path-drag, only when pathSpur finds a segment: a guest path drawn from the middle of that segment to a point that starts DRAG_START_REACH beyond its side and moves DRAG_STEP further out each frame, on its own stepped copy, timed as shop-drag's frames are.

The SPEC also says that each of these runs WARM_UPS + REPETITIONS frames and times the last REPETITIONS, and that its result is the last candidate's hash. benchPark throws std::runtime_error naming the shop or the segment when the world refuses a frame's edit.

Public interface, in src/sim/units.h:

    // What a unit's derivation reads. Each read is recorded, by table and key.
    class UnitReads {
    public:
      // The entity's T, or null. T must be one of the kind's Reads.
      template <typename T> const T *component(EntityKey key);
      // The unit of an earlier kind for the key, or null when there is none.
      template <typename K> const typename K::Value *unit(EntityKey key);
      // The keys the index files under the key, ascending.
      template <typename I> std::span<const EntityKey> filed(EntityKey key);
    };

    // A unit kind: a Name, an intent component Over that each unit's entity holds, a Value with a
    // visitFields function, compared by the walk's words, Reads, a std::tuple of the intent types its derivation reads, and
    // derive(UnitReads &, EntityKey, Value &).
    template <typename K> concept UnitKind = ...;
    // An index: a Name, an intent component Over, and keysOf(const Over &, std::vector<EntityKey> &),
    // which clears the list and puts in the keys to file the entity under.
    template <typename I> concept UnitIndex = ...;

    template <UnitIndex I> void addUnitIndex(WorldSchema &schema);
    template <UnitKind K> void addUnits(WorldSchema &schema);
    template <UnitKind K> const typename K::Value *unitValue(const World &world, EntityKey key);
    template <UnitKind K> bool unitsCurrent(const World &world);

## Files affected

- Create: src/sim/units.h, src/sim/unit_space.h, src/sim/unit_space.cpp
- Create: src/sim/park/internal/ground_units.h, src/sim/park/ground_units.cpp
- Modify:
    - src/sim/component_store.h
    - src/sim/schema.h
    - src/sim/schema.cpp
    - src/sim/world.h
    - src/sim/world.cpp
    - src/sim/walk.cpp
    - src/sim/cycle.cpp
    - src/sim/CMakeLists.txt
    - src/sim/SPEC.md
- Modify:
    - src/sim/park/layout.cpp
    - src/sim/park/internal/layout.h
    - src/sim/park/intent.cpp
    - src/sim/park/surface.h
    - src/sim/park/SPEC.md
    - src/sim/routes/networks.cpp
    - src/sim/guests/ground.cpp
    - src/sim/furniture/furniture.cpp
    - src/sim/park/surface.cpp
    - src/sim/park/edits.cpp
    - src/sim/park_schema.cpp
- Modify: src/legible/preview.h, src/legible/preview.cpp
- Modify: src/bench/stages.h, src/bench/stages.cpp, src/bench/SPEC.md
- Modify, in the test pass: tests/sim/park_world_checks_test.cpp

## Dependencies

- lean-routes: resolvers take the caller's kept buffers. Merged.
- The milestone's decisions don't bear on this feature.

## Out of scope

- The paved and walked surfaces on units: unit-surface.
- The candidate as a layer, ticks between frames re-deriving only what changed, and one plan per changed frame: layered-candidate. Until then, the preview refills its kept candidate from the whole world on each changed frame, without allocating.
- Units that read state, and change logs on state stores: layered-candidate.
- Edits on hypothetical layouts, such as acceptedPlan's, keep calling groundOf on the whole layout: layered-candidate.

## Open questions

None.
