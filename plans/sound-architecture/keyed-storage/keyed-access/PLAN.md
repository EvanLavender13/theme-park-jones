# Implementation Plan: Keyed Access

## Goal

Every call site reaches components through World by EntityKey, and Registry, findEntity, and keyOf become private.

## Approach

World gains keyed member templates in sim/world.h, each a one-line call into the registry through the private findEntity. The verbs follow EnTT's, so each call site changes one to one, and the walk, save, and cycle functions, which are World's friends, keep using the registry until own-storage. Views become loops over keysWith<T>(). None of the replaced views depends on order: surface.cpp sorts what it collects, and the tests' views either change each component on its own or collect and then create derived entities, whose keys don't depend on order.

Run every command from the worktree root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones-own-storage.

## Placement

Decision 0027 places each behavior this feature adds:

- Keyed component access, isLive, keysWith, and holds: sim root, World in sim/world.h and sim/world.cpp. World owns entity identity and the component storage, so it's the one place that turns a key into a component.
- Each call site's change: the module that already owns that call site. No behavior moves between components.

## Tasks

### Task 1: Configure the worktree and record the baseline

Files:
- none in the tree; outputs go under build/

Step 1: Configure windows-debug and windows-release with the main checkout's package cache, so nothing downloads again.

Run: `cmake.exe --preset windows-debug -DCPM_SOURCE_CACHE=C:/Users/EvanUhhh/source/repos/ThemeParkJones/.cpm-cache > /dev/null && cmake.exe --preset windows-release -DCPM_SOURCE_CACHE=C:/Users/EvanUhhh/source/repos/ThemeParkJones/.cpm-cache > /dev/null; echo $?`
Expected: `0`

Step 2: Build tpj_scenarios on windows-debug and write the baseline over every park.

Run: `mkdir -p build/keyed-storage && cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/keyed-storage/before-scenarios.txt 2>/dev/null; echo $?`
Expected: the build's last line, then `0`.

Step 3: Write the runtime baseline. It takes a while.

Run: `scripts/runtime-report.sh build/keyed-storage/before.report`
Expected: the report printed against the budget, with an `over-budget <n> of <m>` line, then `runtime-report: wrote build/keyed-storage/before.report in <m>m <s>s`.

### Task 2: Spec

Files:
- Modify: `src/sim/SPEC.md` ("Entity keys")

Step 1: After the paragraph that starts "Resolvers create entities with createDerivedEntity", add the two paragraphs given in FEATURE.md's Spec changes, word for word.

### Task 3: World's keyed functions

Files:
- Modify: `src/sim/world.h`
- Modify: `src/sim/world.cpp`

Step 1: In World's public section, after keys(), declare these. Define the templates in world.h below the class, each over Registry and findEntity:

```cpp
  // True when an entity holds this key.
  [[nodiscard]] bool isLive(EntityKey key) const;
  // The entity must be live and hold T.
  template <typename T> T &get(EntityKey key);
  template <typename T> const T &get(EntityKey key) const;
  // Nothing when the key isn't live or its entity doesn't hold T.
  template <typename T> T *tryGet(EntityKey key);
  template <typename T> const T *tryGet(EntityKey key) const;
  // False when the key isn't live.
  template <typename T> [[nodiscard]] bool has(EntityKey key) const;
  // The entity must be live and not hold T.
  template <typename T, typename... Args> void emplace(EntityKey key, Args &&...args);
  // The entity must be live.
  template <typename T> void emplaceOrReplace(EntityKey key, T value);
  // The entity must be live. T must not be an empty type.
  template <typename T> T &getOrEmplace(EntityKey key);
  // Removes T if the entity holds it. Nothing happens when the key isn't live.
  template <typename T> void remove(EntityKey key);
  // Keys of the live entities that hold T, ascending.
  template <typename T> [[nodiscard]] std::vector<EntityKey> keysWith() const;
  // True when the entity is live and holds the registered type.
  [[nodiscard]] bool holds(const ComponentType &type, EntityKey key) const;
```

tryGet, has, and remove check findEntity against entt::null before touching the registry. emplace forwards its arguments to Registry.emplace<T>, so an empty type takes none. emplace and emplaceOrReplace return nothing, because EnTT returns nothing for an empty type and no call site reads the result. getOrEmplace starts with a static_assert that T isn't empty; every call site uses it on a non-empty type. holds calls type.Has on the registry and the key's entity, after the same null check.

keysWith reads the registry's storage for T through the const `Registry.storage<T>()`, which gives nullptr when no T was ever added. It maps each entity in that storage to its key through KeyByEntity, using NULL_KEY for an entity that has no key, and sorts the keys ascending. Reading the storage, not ByKey, means a component left behind on an entity that lost its key still shows up.

Step 2: Define isLive and holds in world.cpp, after keys().

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error, such as `[n/n] Linking CXX static library src/sim/libtpj_sim.a`.

### Task 4: Move the medium to keyed access

Files:
- Modify: `src/sim/medium/field.h`, `src/sim/medium/flow.h`, `src/sim/medium/flow.cpp`, `src/sim/medium/kept_field.h`, `src/sim/medium/ground_field.h`

Step 1: Rewrite every use of Registry, findEntity, and entt in these files by this table. The same table serves Tasks 5 through 9.

- `world.Registry.get<T>(world.findEntity(k))` becomes `world.get<T>(k)`. try_get becomes tryGet, all_of becomes has, emplace stays emplace, emplace_or_replace becomes emplaceOrReplace, get_or_emplace becomes getOrEmplace, and remove stays remove.
- `world.Registry.replace<T>(world.findEntity(k), v)` becomes `world.get<T>(k) = v`.
- `world.findEntity(k) != entt::null` becomes `world.isLive(k)`, and `== entt::null` becomes `!world.isLive(k)`.
- Where code stores `const entt::entity entity = world.findEntity(k)`, then returns nullptr when it's null, then calls try_get on it, drop the variable and the null check and call tryGet with k. If the null branch returns something other than what a missing component gives, keep that branch as `if (!world.isLive(k))`.
- A view over T becomes a loop over `world.keysWith<T>()` that reads `world.get<T>(key)` for each key. Where the view's callback took the entity and called keyOf, use the loop's key.
- A helper that takes an entt::entity takes the EntityKey instead. In field.h, layeredSlots takes `EntityKey fieldKey`, and its callers pass `fieldKey(F::Name)`.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 5: Move park to keyed access

Files:
- Modify: `src/sim/park/intent.cpp`, `src/sim/park/layout.cpp`, `src/sim/park/surface.cpp`, `src/sim/park/edits.cpp`

Step 1: Apply Task 4's table. In surface.cpp, taperCandidates loops over `world.keysWith<PathSegmentIntent>()`. In edits.cpp, the edit target's helpers use `Target->get<T>(key)` and `Target->emplace<T>(key, value)`. edits.cpp's local lambda named keyOf is not World::keyOf, so leave it.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 6: Move routes and operations to keyed access

Files:
- Modify: `src/sim/routes/networks.cpp`, `src/sim/operations/operations.cpp`

Step 1: Apply Task 4's table. In networks.cpp, the network swap keeps its order: it moves the held Network into PreviousNetwork, then puts the new Network.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 7: Move guests to keyed access

Files:
- Modify: `src/sim/guests/ground.cpp`, `src/sim/guests/guests.cpp`, `src/sim/guests/footfall.cpp`

Step 1: Apply Task 4's table. parkGuests keeps its loop over world.keys() with `world.has<Guest>(key)`.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 8: Move the scenarios to keyed access

Files:
- Modify: `src/scenarios/beacons.cpp`, `src/scenarios/food_shop.cpp`, `src/scenarios/walkers.cpp`, `src/scenarios/stalls.cpp`

Step 1: Apply Task 4's table.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 9: Move the tests to keyed access

Files:
- Modify: `tests/sim/support/synthetic_types.h`, `tests/sim/cycle_test.cpp`, `tests/sim/save_test.cpp`, `tests/sim/walk_test.cpp`, `tests/sim/world_checks_test.cpp`, `tests/sim/entity_key_test.cpp`, `tests/sim/park_world_checks_test.cpp`

Step 1: Apply Task 4's table. Change only how each case reaches components, never what it asserts. In park_world_checks_test.cpp, `type.Has(world.Registry, world.findEntity(key))` becomes `world.holds(type, key)`.

Step 2: In entity_key_test.cpp, rewrite these cases as follows:

- "createDerivedEntity is idempotent and never moves the counter": drop the entity variable and the `findEntity(again) == entity` line. Emplace and read Cached by key.
- "destroyEntity removes the entity and its components": replace `!valid(entity)` with `!world.isLive(doomed)`. Replace the two `storage<...>().empty()` lines with `world.keysWith<Probe>().empty()` and `world.keysWith<Tag>().empty()`.
- Delete "findEntity and keyOf are inverses for live entities", "lookups of anything not live give entt::null and NULL_KEY", and the NULL_ENTITY constant.

Step 3: Build and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -1 && build/windows-debug/tpj_sim_tests.exe 2>&1 | tail -2`
Expected: the build's last line with no warning or error, then `All tests passed` with the assertion and case counts.

### Task 10: Make the registry private

Files:
- Modify: `src/sim/world.h`

Step 1: Move Registry, findEntity, and keyOf into World's private section. Replace the comment above Registry with: "Held by the walk, save, and cycle functions. Everything else reaches components by key."

Step 2: Build everything.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1`
Expected: a last line with no warning or error.

### Task 11: Confirm the acceptance criteria

Step 1: Show the simulation unchanged.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/keyed-storage/keyed-access-scenarios.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/keyed-storage/before-scenarios.txt build/keyed-storage/keyed-access-scenarios.txt; echo $?`
Expected: `0`

Step 2: Run the full suite and tidy.

Run: `ctest.exe --preset windows-debug 2>&1 | tail -3 && scripts/tidy.sh 2>&1 | tail -1`
Expected: `100% tests passed, 0 tests failed out of <n>` in ctest's summary, then `tidy: clean.`

Step 3: Write the after report and compare it with the baseline.

Run: `scripts/runtime-report.sh build/keyed-storage/keyed-access.report && scripts/runtime-report.sh --compare build/keyed-storage/before.report build/keyed-storage/keyed-access.report`
Expected: the comparison, ending with `clear <n> of <m>`. Give it to the user. If any stage is marked slower, say which.

### Task 12: Commit

Step 1: Dispatch the reviewer on the diff, as implementing-features says. Then commit the feature once with commit-hygiene, staging only the files listed in FEATURE.md.
