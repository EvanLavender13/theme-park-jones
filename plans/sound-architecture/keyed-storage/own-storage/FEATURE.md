# Feature: Own Storage

## Summary

own-storage replaces EnTT behind World. World keeps one store per registered component type, holding that type's components by EntityKey in ascending key order. The schema names each type by an id of its own, and it makes each type's store. The walk, save, and load go through the stores. World's keyed functions keep their signatures, so no call site changes. EnTT leaves the build, and the specs, conventions, and the private header check cite decision 0039 instead of 0016. If code adds a component of an unregistered type, the world now refuses it when it's added, in every build.

## Acceptance criteria

- No file under src or tests includes an EnTT header, and the build doesn't fetch EnTT. tpj_sim links only tpj_core. Confirmed by the windows-debug and windows-release builds after reconfiguring: with EnTT gone from CMakeLists.txt and from tpj_sim's link, any EnTT include would fail to compile.
- World's interface names no entity handle besides EntityKey. findEntity, keyOf, and the registry are gone. Confirmed by the reviewer reading sim/world.h.
- keysWith<T>() gives every holder of T in ascending key order, read from T's store. Confirmed by the tests that iterate with it, cycle_test and save_test, passing in tpj_sim_tests.
- If code adds a component of a type the schema doesn't register, World throws WorldInvariantError in every build, and the world doesn't change. Confirmed by the listed test, run in both windows-debug and windows-release.
- The simulation is unchanged. `tpj_scenarios --compare` on windows-debug output over every park in tests/parks and tests/parks/stress, from main before and from the branch after, exits 0. scripts/cross-build-check.sh passes.
- The existing tests pass. They change only as decision 0039 says: world_checks_test's case is replaced by the listed test. Confirmed by `ctest.exe --preset windows-debug`. The windows-debug build has no warnings, and scripts/tidy.sh is clean.
- The runtime report is compared against a baseline taken on main before this feature, and against the milestone's baseline, build/keyed-storage/before.report. Both comparisons are given to the user. If a stage is marked slower, the user decides before the merge whether it ships. Timings aren't gated.
- src/sim/SPEC.md, src/sim/park/SPEC.md, docs/conventions.md, and cmake/check_private_headers.cmake cite 0039 for the private-header rule. Confirmed by the reviewer.

## Tests

- Replace the case in tests/sim/world_checks_test.cpp, "in debug builds, the walk refuses a world holding state it cannot cover, and accepts every world it can", as decision 0039 says. The new case runs in every build, with no skip. Set up the synthetic world from tests/sim/support/synthetic_types.h, and take its hash. Change: add a component of a type the synthetic schema doesn't register to one of its live entities. Expected: the add throws WorldInvariantError, and the world's hash afterwards equals the hash before the add.

## Medium

None. The feature changes where the world keeps its components, not which fields or flows any module samples, emits, draws, or supplies. Private components stay in their modules' internal headers.

## Spec changes

src/sim/SPEC.md:

- In the opening paragraph, "decisions 0016, 0019, 0022, and 0025" becomes "decisions 0019, 0022, 0025, and 0039".
- In "Boundary", "The target tpj_sim links only tpj_core and EnTT." becomes "The target tpj_sim links only tpj_core."
- In "Boundary", "only shared-medium components are public (decision 0016)" becomes "(decision 0039)".
- In "Entity keys", the first two sentences become: "Every entity has a stable EntityKey, and code names an entity only by its key. When a component refers to another entity, it holds the entity's key."
- In "Entity keys", after the keysWith paragraph, add:

  > If code adds a component of a type the world's schema doesn't register, emplace, emplaceOrReplace, and getOrEmplace throw WorldInvariantError in every build, and the world doesn't change. No entity ever holds such a type. In debug builds, they also throw WorldInvariantError if the key isn't live.

- In "Copies, equality, and hashes", "walk registered types in registration order and entities in key order, never in EnTT storage order." becomes "walk registered types in registration order and entities in key order."
- In "Copies, equality, and hashes", "In debug builds, if a world holds state the walk cannot cover, the walk refuses it with WorldInvariantError. Such state is an unregistered component, an entity with no key, or a NaN." becomes "In debug builds, if a registered component holds a NaN, the walk refuses the world with WorldInvariantError."

src/sim/park/SPEC.md: "Their components are private to this module (decision 0016)." becomes "(decision 0039)".

Public interface. sim/type_id.h is new:

```cpp
// Names a C++ type within one run of the program. It differs between runs, so never save or hash it.
using TypeId = const void *;
// One value for every call with T, and a different value for every other type.
template <typename T> TypeId typeIdOf();
```

World's public functions keep their declarations. Its private section loses Registry, findEntity, keyOf, KeyByEntity, and EntityRecord::Entity, and it gains one store per registered type. ComponentType and CommandType in sim/schema.h, and QueuedCommand in sim/command_queue.h, replace entt::id_type with TypeId. ComponentType's registry functions become one function that makes the type's store. QueuedCommand and CommandType drop TypeName.

## Files affected

- Create: src/sim/type_id.h, src/sim/component_store.h
- Modify: src/sim/SPEC.md, src/sim/park/SPEC.md
- Modify: src/sim/world.h, src/sim/world.cpp, src/sim/schema.h, src/sim/schema.cpp, src/sim/walk.cpp, src/sim/save.cpp, src/sim/cycle.cpp, src/sim/command_queue.h, src/sim/entity_key.h
- Modify: CMakeLists.txt, src/sim/CMakeLists.txt
- Modify: docs/conventions.md, cmake/check_private_headers.cmake
- Modify: plans/shared-medium/CAPABILITY.md, plans/sound-architecture/CAPABILITY.md
- Modify: tests/sim/world_checks_test.cpp (the test pass), tests/sim/save_test.cpp

## Dependencies

- keyed-access merged: met (d6d48fe). Every call site already reaches components by key.
- The milestone's RESEARCH.md settles the first store (a std::map per type) and the type id (the address of a static per type).

## Out of scope

- Faster stores, and a per-type cache of the store's index. These are scalable-runtime's "Faster component store" candidate.
- Converting loops that walk world.keys() and test each key, such as parkGuests, to keysWith. This is scalable-runtime's "Guests found by type" candidate.
- A printable name for command types. The two errors that used EnTT's type name no longer name the type.

## Open questions

None.
