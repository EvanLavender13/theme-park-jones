# Feature: Keyed Access

## Summary

keyed-access gives World one way to reach components: by EntityKey. World gains get, tryGet, has, emplace, emplaceOrReplace, getOrEmplace, and remove, all taking a key. It also gains isLive for whether a key names a live entity, keysWith<T>() for the keys that hold one type in ascending order, and holds for a registered type named at run time. Every call site in src and tests moves to these. Registry, findEntity, and keyOf become private. After this feature, only World with its walk, save, and cycle functions, the schema, and the command queue name EnTT. The storage behind World is still EnTT. own-storage replaces it.

## Acceptance criteria

- Code outside World and its friend functions reaches components only through World's keyed functions. Registry, findEntity, and keyOf are private. Confirmed by the windows-debug build: a call site that still used them wouldn't compile.
- keysWith<T>() gives the keys of every entity that holds T, in ascending order. The order is confirmed by the reviewer reading keysWith's definition, which sorts the keys. That it finds every holder is confirmed by the migrated tests that used views: cycle_test's systems and save_test's derived routes now go through keysWith, and tpj_sim_tests passes.
- The simulation is unchanged. `tpj_scenarios --compare` on windows-debug output from before and after, over every park in tests/parks, exits 0.
- The existing tests pass. They change only in how they reach components, except that entity_key_test's "findEntity and keyOf are inverses for live entities" and "lookups of anything not live give entt::null and NULL_KEY" are deleted (decision 0039). Confirmed by `ctest.exe --preset windows-debug`.
- The windows-debug build has no warnings, and scripts/tidy.sh is clean.
- The runtime report from before this feature is kept as the milestone's baseline, and the report after it is compared against it and given to the user. Timings aren't gated.

## Tests

None.

## Medium

None. This feature changes how code reaches components, not which fields or flows any module samples, emits, draws, or supplies. Private components stay in their modules' internal headers.

## Spec changes

src/sim/SPEC.md, in "Entity keys", after the paragraph on createDerivedEntity, add:

> Code reaches an entity's components through World, by key. get, tryGet, has, emplace, emplaceOrReplace, getOrEmplace, and remove each take the entity's key. If the key isn't live, tryGet gives nothing, has is false, and remove does nothing. isLive says whether a key names a live entity.
>
> keysWith<T>() lists the keys of the entities that hold T, in ascending order. If a system visits them in that order, it visits them in the same order in a copy and after a load.

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/sim/world.h, src/sim/world.cpp
- Modify: src/sim/medium/field.h, src/sim/medium/flow.h, src/sim/medium/flow.cpp, src/sim/medium/kept_field.h, src/sim/medium/ground_field.h
- Modify: src/sim/park/intent.cpp, src/sim/park/layout.cpp, src/sim/park/surface.cpp, src/sim/park/edits.cpp
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/operations/operations.cpp
- Modify: src/sim/guests/ground.cpp, src/sim/guests/guests.cpp, src/sim/guests/footfall.cpp
- Modify: src/scenarios/beacons.cpp, src/scenarios/food_shop.cpp, src/scenarios/walkers.cpp, src/scenarios/stalls.cpp
- Modify: tests/sim/support/synthetic_types.h, tests/sim/cycle_test.cpp, tests/sim/save_test.cpp, tests/sim/walk_test.cpp, tests/sim/world_checks_test.cpp, tests/sim/entity_key_test.cpp, tests/sim/park_world_checks_test.cpp

## Dependencies

- Decision 0039 accepted: met.
- tpj_scenarios --compare and scripts/runtime-report.sh: met.

## Out of scope

- Replacing EnTT behind World, removing it from the build, and the schema's type identity. These are own-storage's.
- Moving the add-time check for unregistered types. own-storage does it, because it needs World's own stores.
- Converting loops that walk world.keys() and test each key for a type, such as parkGuests, to keysWith. Their order is already key order. The speedup is a scalable-runtime candidate.

## Open questions

None.
