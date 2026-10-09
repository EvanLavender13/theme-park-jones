# Implementation Plan: Own Storage

## Goal

World keeps each registered type's components in its own key-ordered store, and EnTT leaves the build, with the simulation unchanged.

## Approach

The schema makes one store per registered type. The store is a std::map from EntityKey to the component, behind a small virtual interface, so the walk, save, and load can reach it without naming the type. World holds the stores in a vector in registration order. Its keyed templates find T's store through the schema's index of type ids and use the map directly. The walk iterates each store in key order instead of testing every live key against every type, and it emits the same words, so hashes and saves don't change. Type ids are the address of a static per type, which replaces EnTT's type_id for components and commands alike.

Run every command from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones, in the main checkout.

## Placement

Decision 0027 places each behavior this feature adds:

- Per-type stores, keyed access over them, and the add-time checks: sim root, World in sim/world.h and sim/world.cpp, with the store interface in sim/component_store.h. World owns entity identity and component storage.
- The store each registered type gets: sim root, WorldSchema in sim/schema.h. The schema already instantiates each type's functions when a module registers it, so it makes the type's store too.
- Type ids: sim root, sim/type_id.h. The schema and the command queue both need them, and the queue mustn't depend on the schema.
- Copy, equality, hash, validation, save, and load over the stores: sim root, the walk in sim/walk.cpp and the save in sim/save.cpp. They already own these operations as World's friends.
- Dropping EnTT from the build: CMakeLists.txt and src/sim/CMakeLists.txt.

## Tasks

### Task 1: Record the baselines on main

Files:
- none in the tree; outputs go under build/keyed-storage/

Step 1: Build tpj_scenarios on windows-debug and write the scenarios baseline over every park. The stress parks take about eight minutes.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && build/windows-debug/tpj_scenarios.exe tests/parks/*.park tests/parks/stress/*.park > build/keyed-storage/own-storage-before-scenarios.txt 2>/dev/null; echo $?`
Expected: the build's last line, then `0`.

Step 2: Write the runtime baseline. It takes a while.

Run: `scripts/runtime-report.sh build/keyed-storage/own-storage-before.report`
Expected: the report printed against the budget, with an `over-budget <n> of <m>` line, then `runtime-report: wrote build/keyed-storage/own-storage-before.report in <m>m <s>s`.

### Task 2: Specs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/sim/park/SPEC.md`

Step 1: Make each edit listed under FEATURE.md's Spec changes, word for word.

### Task 3: Decision references

Files:
- Modify: `docs/conventions.md` (the internal-headers paragraph)
- Modify: `cmake/check_private_headers.cmake:2`
- Modify: `plans/shared-medium/CAPABILITY.md:60,108,120`
- Modify: `plans/sound-architecture/CAPABILITY.md:19`

Step 1: In docs/conventions.md, cmake/check_private_headers.cmake, and plans/shared-medium/CAPABILITY.md lines 60 and 108, replace "decision 0016" with "decision 0039". In plans/sound-architecture/CAPABILITY.md line 19, do the same.

Step 2: In plans/shared-medium/CAPABILITY.md line 120, "tpj_sim with EnTT (decisions 0016 and 0022): met." becomes "tpj_sim (decisions 0022 and 0039): met."

### Task 4: Test pass

Step 1: Run the test pass for the test FEATURE.md lists, as implementing-features says. Until Task 7, the test fails because the add doesn't throw.

### Task 5: Type ids for commands

Files:
- Create: `src/sim/type_id.h`
- Modify: `src/sim/command_queue.h`, `src/sim/schema.h`, `src/sim/schema.cpp`, `src/sim/cycle.cpp`

Step 1: Create sim/type_id.h with the declarations in FEATURE.md's Spec changes. Define typeIdOf<T>() in the header as the address of a function-local `static char` that isn't const. Each instantiation has its own static, so each type gets its own address. If the static were const, the compiler could merge identical ones.

Step 2: In command_queue.h, drop the EnTT include. QueuedCommand becomes `TypeId Id = nullptr;` and `std::any Value;`, with no TypeName. push stores typeIdOf<T>().

Step 3: In schema.h, CommandType becomes `TypeId Id = nullptr;` and Apply, with no TypeName. addCommand stores typeIdOf<T>(). findCommand takes a TypeId. In schema.cpp, addCommandType's error becomes "a command type is already registered". In cycle.cpp, requireRegistered's error becomes "a queued command's type is not registered with the world's schema", and both functions look commands up by Id.

The build waits until Task 8, because ComponentType still names EnTT until then.

### Task 6: The store and the schema's index

Files:
- Create: `src/sim/component_store.h`
- Modify: `src/sim/schema.h`, `src/sim/schema.cpp`

Step 1: Create sim/component_store.h with the type-erased interface the walk and save use. It includes sim/entity_key.h and forward-declares WordSink and TextCursor. Delete copying and moving, as WordSink does.

```cpp
// One registered type's components, held by EntityKey in ascending key order. World owns one per
// registered type, and the walk and save reach it through this interface.
class ComponentStore {
public:
  virtual ~ComponentStore() = default;
  [[nodiscard]] virtual std::unique_ptr<ComponentStore> clone() const = 0;
  [[nodiscard]] virtual bool has(EntityKey key) const = 0;
  [[nodiscard]] virtual size_t size() const = 0;
  // The held keys, ascending.
  [[nodiscard]] virtual std::vector<EntityKey> keys() const = 0;
  virtual void erase(EntityKey key) = 0;
  // The held component's fields as words. An empty type emits none.
  virtual void emit(EntityKey key, WordSink &sink) const = 0;
  // The held component's fields for its save line, each as a space and name=value.
  virtual void write(EntityKey key, std::string &line) const = 0;
  // Reads what write wrote and adds the component under key. Throws LoadError.
  virtual void read(TextCursor &cursor, EntityKey key) = 0;
};
```

Step 2: In schema.h, after emitField, add `template <typename T> class TypedStore final : public ComponentStore` with a public `std::map<EntityKey, T> Items;`. Its overrides do what the current ComponentType lambdas do, keeping their comments on const_cast and on empty types. Empty types are held in the map like any other type, so has, keys, clone, and erase need no special case.

Step 3: ComponentType becomes Name, Kind, `TypeId Id = nullptr;`, and `std::unique_ptr<ComponentStore> (*MakeStore)() = nullptr;`. addComponent sets Id to typeIdOf<T>() and MakeStore to a function returning a new TypedStore<T>. Drop the EnTT include.

Step 4: Replace findComponent with `[[nodiscard]] std::optional<size_t> indexOf(TypeId id) const;`, which gives the type's position in components(). Back it with a `std::unordered_map<TypeId, size_t>` that addComponentType fills. addComponentType refuses a repeated type by Id, and sameComponents compares Id.

### Task 7: World over its stores

Files:
- Modify: `src/sim/world.h`, `src/sim/world.cpp`, `src/sim/entity_key.h:10`

Step 1: In world.h, drop the EnTT include and include sim/component_store.h. Remove Registry, findEntity, keyOf, KeyByEntity, and EntityRecord::Entity. EntityRecord keeps only Origin, and addEntity(key) returns nothing. Add `std::vector<std::unique_ptr<ComponentStore>> Stores;`, one per registered type in registration order. The constructor fills it with each type's MakeStore().

Step 2: Add two private helpers. `template <typename T> TypedStore<T> *findStore()`, with a const overload, gives T's store through Schema->indexOf(typeIdOf<T>()), or nullptr if T isn't registered. `template <typename T> TypedStore<T> &storeToAdd(EntityKey key)` throws WorldInvariantError if T isn't registered, and, when WORLD_CHECKS, if the key isn't live. The messages are "entity <key> was given a component of a type the world's schema doesn't register" and "a component was added to key <key>, which isn't live".

Step 3: Rewrite the keyed templates over the stores. emplace, emplaceOrReplace, and getOrEmplace go through storeToAdd, so a refused add changes nothing. emplace constructs T in place from its arguments with try_emplace. emplaceOrReplace uses insert_or_assign. getOrEmplace uses try_emplace with no arguments. get uses the map's at. tryGet, has, remove, and keysWith treat an unregistered type as held by no entity. keysWith copies the store's keys, which are already ascending, so it no longer sorts.

Step 4: In world.cpp, destroyEntity erases the key from every store. holds finds the store through Schema->indexOf(type.Id). Update WorldInvariantError's comment in world.h to list what throws it now: a NaN in registered state, two derived origins sharing a key, an added component of an unregistered type, an added component on a key that isn't live in debug builds, and a world created under the wrong floating-point environment.

Step 5: In entity_key.h line 10, the comment becomes "An entity's identity: stable across copies, saves, and loads."

### Task 8: The walk and the save over the stores

Files:
- Modify: `src/sim/walk.cpp`, `src/sim/save.cpp`

Step 1: In walk.cpp, validateWorld keeps only the NaN check, visiting each type's store by its keys. The checks for unregistered storages and for entities made or destroyed outside World go, because nothing outside World can do either now.

Step 2: emitWords keeps its order of words. For each type in registration order, it emits the name hash, the kind, the store's size, and then each held key with its emitted fields. copyWorld copies ByKey and each store's clone().

Step 3: In save.cpp, holdsSaved takes a key and asks each non-derived type's store. Each type's section lists its store's keys in order. loadWorld adds the entity with addEntity when the key isn't live and reads the line into the type's store.

Step 4: Build the sim.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error, such as `[n/n] Linking CXX static library src/sim/libtpj_sim.a`.

### Task 9: Drop EnTT from the build

Files:
- Modify: `CMakeLists.txt:36-40`
- Modify: `src/sim/CMakeLists.txt:1-2,32`

Step 1: Delete the CPMAddPackage block for EnTT. In src/sim/CMakeLists.txt, tpj_sim links only tpj_core, and the opening comment ends "so this target links nothing beyond tpj_core."

Step 2: Reconfigure both Windows builds and build everything on windows-debug.

Run: `cmake.exe --preset windows-debug > /dev/null && cmake.exe --preset windows-release > /dev/null && cmake.exe --build --preset windows-debug 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 10: The save test's comment

Files:
- Modify: `tests/sim/save_test.cpp:161`

Step 1: In save_test.cpp line 161, the comment's "EnTT's identifiers churned first" becomes "an entity created and destroyed first".

Step 2: Build and run the sim's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -1 && build/windows-debug/tpj_sim_tests.exe 2>&1 | tail -2`
Expected: the build's last line with no warning or error, then `All tests passed` with the assertion and case counts.

### Task 11: Confirm the acceptance criteria

Step 1: Show the simulation unchanged. The stress parks take about eight minutes.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park tests/parks/stress/*.park > build/keyed-storage/own-storage-scenarios.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/keyed-storage/own-storage-before-scenarios.txt build/keyed-storage/own-storage-scenarios.txt; echo $?`
Expected: `0`

Step 2: Check that the release builds simulate the same.

Run: `scripts/cross-build-check.sh 2>&1 | tail -1`
Expected: `cross-build-check: all 3 builds wrote the same <n> lines; passed.`

Step 3: Show the refusal in a release build.

Run: `cmake.exe --build --preset windows-release --target tpj_sim_tests 2>&1 | tail -1 && build/windows-release/tpj_sim_tests.exe -# "[#world_checks_test]" 2>&1 | tail -2`
Expected: the build's last line with no warning or error, then `All tests passed` with one test case.

Step 4: Run the full suite and tidy.

Run: `ctest.exe --preset windows-debug 2>&1 | tail -3 && scripts/tidy.sh 2>&1 | tail -1`
Expected: `100% tests passed, 0 tests failed out of <n>` in ctest's summary, then `tidy: clean.`

Step 5: Write the after report and compare it with both baselines.

Run: `scripts/runtime-report.sh build/keyed-storage/own-storage.report && scripts/runtime-report.sh --compare build/keyed-storage/own-storage-before.report build/keyed-storage/own-storage.report && scripts/runtime-report.sh --compare build/keyed-storage/before.report build/keyed-storage/own-storage.report`
Expected: two comparisons, each ending with `clear <n> of <m>`. Give both to the user. Say that before.report predates the opening-hours merge. If any stage is marked slower, say which, and let the user decide before the merge. If any stage is marked `result-changed`, the simulation changed: stop and report it.

### Task 12: Review and commit

Step 1: Dispatch the reviewer on the staged diff, as implementing-features says. Then commit the feature once with commit-hygiene. Stage only the files listed in FEATURE.md.
