# Implementation Plan: Carried Guests

## Goal

Every resolution carries each guest's place from the previous guest network to the new one, moving guests on deleted paths to the nearest point of the network, with the previous networks kept by path-networks and dropped by a routes finisher registered after the guests module.

## Approach

path-networks moves the network already on each network entity into a derived PreviousNetwork component on the same entity before replacing it, and routes exposes it through previousNetwork and registers the finisher that removes it through addDropPreviousNetworks. The guests module registers a finisher, carryGuests, that applies carryOver to each guest's place and falls back to nearestPlace of the old ground point, and addPark registers the dropping finisher after addGuests. The step's existing rule, that a guest whose place does not resolve leaves, now covers only a guest network with no carrier.

## Tasks

### Task 1: Sim spec

Files:
- Modify: `src/sim/SPEC.md` (the addPark sentence in the schema paragraph, and the finisher sentence in the resolvers paragraph)

Step 1: Replace

```
and then the guests module's state and system, which admit, walk, and feed its guests (sim/guests/SPEC.md).
```

with

```
then the guests module's state, system, and finisher, which admit, walk, feed, and carry its guests (sim/guests/SPEC.md), and last the routes module's finisher that drops the previous networks, after every finisher that reads them (sim/routes/SPEC.md).
```

Step 2: Replace

```
Like a resolver, it never draws from the key counter and derives nothing from state, but it may clear state that its owning module's spec says a resolution invalidates.
```

with

```
Like a resolver, it never draws from the key counter, but it may change state that its owning module's spec says a resolution invalidates: clearing stepped entries the new resolution no longer matches, or carrying places held on a network the resolution re-derived.
```

The sentence drops "derives nothing from state", since carrying a place reads the place.

### Task 2: Routes spec

Files:
- Modify: `src/sim/routes/SPEC.md` (the module paragraph and the first paragraph of Networks)

Step 1: In the module paragraph, replace

```
so other modules read them through parkNetwork and the Network type's queries,
```

with

```
so other modules read them through parkNetwork, previousNetwork, and the Network type's queries,
```

Step 2: Replace the first paragraph of the Networks section with:

```
addRoutes registers the derived component type PreviousNetwork, named previous-network, then the resolver path-networks, and then route distance's fields and resolver (Route distance), and makeParkSchema calls it after addParkEdits. In each resolution, path-networks derives one network for each PathKind and puts it on the entity keyed networkKey(kind), deriveKey(NULL_KEY, hashName("network"), the kind's value), which it creates with createDerivedEntity. When the entity already holds a network, path-networks first moves it into the entity's PreviousNetwork, replacing any there, and then replaces it. parkNetwork gives a kind's network, or an empty network when the world holds none, as before its first resolution. Networks are derived from intent alone, so saves never hold them (principle 1).

previousNetwork gives the kind's network as the resolution before the current one derived it, so that finishers can carry places held on it to the new network with carryOver (sim/medium/SPEC.md, Carry-over), or none when the world holds none. addDropPreviousNetworks registers a finisher that removes both kinds' PreviousNetwork, and addPark calls it after every module whose finisher reads them, so no resolution ends holding a previous network: a world between cycles, its copies and saves, and every candidate hold none. A world's first resolution finds no network on the entities, so it has no previous network, and finishers carry nothing in it.
```

### Task 3: Guests spec

Files:
- Modify: `src/sim/guests/SPEC.md` (the module paragraph, Registration, Stepping, and a new Carrying section after Visits and meals)

Step 1: In the module paragraph, replace

```
the guest network through parkNetwork and the Network type's queries,
```

with

```
the guest network through parkNetwork, previousNetwork, and the Network type's queries,
```

Step 2: In Registration, replace

```
addGuests registers the state component type Guest, named guest, and then the system stepGuests.
```

with

```
addGuests registers the state component type Guest, named guest, then the system stepGuests, and then the finisher carryGuests (Carrying).
```

Step 3: In Stepping, replace

```
A guest whose place does not resolve on N, as after an edit deletes its path, leaves the park.
```

with

```
A guest whose place does not resolve on N leaves the park. Carrying leaves such a place only when N has no carrier, as after an edit deletes every guest path, or when a loaded save held one.
```

Step 4: After the Visits and meals section and before Inspection record, add:

```
## Carrying

A resolution re-derives N under the guests. Its finisher carryGuests moves each guest's place, in ascending key order, from the network before, previousNetwork(world, PathKind::Guest), to N, so every world a cycle leaves and every candidate holds carried places, and a candidate's guests stand where committing its edit puts them (principle 8). The place becomes carryOver of it from the network before to N when that gives a place, so a guest keeps its ground position while its carrier stays. A retired place, whose path or connector is gone, becomes N's nearestPlace of the place's groundPoint on the network before, so a guest whose path was deleted stands at the nearest point of the guest network. That is the one straight-line measure guests use (principle 4). With no such place, because N has no carrier, the place is unchanged, and the guest leaves the park when it next steps (Stepping).

Carrying changes nothing but the place. A waiting guest keeps waiting where it is carried to, and a guest heading to a shop keeps its Target, dropping it at its next walk if its Target's offer is no longer reachable there (Walking). LastChoice keeps the place the guest chose at. A world's first resolution has no network before, so carrying changes nothing in it, and loading the save of a resolved world and resolving gives the world saved.
```

### Task 4: Scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md` (the scenarios paragraph)

Step 1: Replace

```
and the walks, choices, and meals of the guests its entrance admits, are compared across builds.
```

with

```
and the walks, choices, and meals of the guests its entrance admits, and how its edits carry them, are compared across builds.
```

### Task 5: Routes interface

Files:
- Modify: `src/sim/routes/networks.h` (after parkNetwork's declaration, and addRoutes's comment)
- Modify: `src/sim/routes/networks.cpp:418-432`

Step 1: In networks.h, after the declaration of parkNetwork, add:

```cpp
// The kind's network as the resolution before the current one derived it, or none when the world
// holds none: always outside a resolution, and in a world's first. Meant for finishers that carry
// places held on it to parkNetwork with carryOver.
const Network *previousNetwork(const World &world, PathKind kind);
```

Step 2: Replace addRoutes's comment and declaration with:

```cpp
// Registers the derived type previous-network, the resolver path-networks, and then route
// distance's fields and resolver.
void addRoutes(WorldSchema &schema);

// Registers the finisher that drops the previous networks. Register it after every finisher that
// reads them.
void addDropPreviousNetworks(WorldSchema &schema);
```

Step 3: In networks.cpp, after `} // namespace` at line 418, add stubs:

```cpp
const Network *previousNetwork(const World & /*world*/, PathKind /*kind*/) { return nullptr; }

void addDropPreviousNetworks(WorldSchema & /*schema*/) {}
```

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`
Expected: the build finishes with no warnings or errors.

### Task 6: Guests and park schema interface

Files:
- Modify: `src/sim/guests/guests.h` (addGuests's comment)
- Modify: `src/sim/park_schema.cpp:12-23`

Step 1: In guests.h, replace

```cpp
// Registers the guest state and the system that steps guests and admits new ones. The routes and
// operations modules' registrations come first.
```

with

```cpp
// Registers the guest state, the system that steps guests and admits new ones, and the finisher
// that carries their places across each resolution. The routes and operations modules'
// registrations come first, and addDropPreviousNetworks after.
```

Step 2: In park_schema.cpp, replace addPark's body with:

```cpp
  // The medium's types come first, since every capability builds on them. Capabilities register
  // their park types after them, in dependency order, starting with park intent and its commands,
  // then the routes that derive the park's networks, then the operations that run its shops and
  // depots, then the guests who visit them. The previous networks are dropped last, once every
  // finisher that carries places from them has run.
  addNetworkComponent(schema);
  addParkIntent(schema);
  addParkEdits(schema);
  addRoutes(schema);
  addOperations(schema);
  addGuests(schema);
  addDropPreviousNetworks(schema);
```

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`
Expected: the build finishes with no warnings or errors.

### Task 7: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/SPEC.md, src/sim/routes/SPEC.md, src/sim/guests/SPEC.md, and src/sim/medium/SPEC.md, and the headers src/sim/routes/networks.h, src/sim/guests/guests.h, src/sim/medium/network.h, and src/sim/park_schema.h. FEATURE.md's Superseded tests section names the two existing tests the pass rewrites.

### Task 8: Keep and drop the previous networks

Files:
- Modify: `src/sim/routes/networks.cpp` (the anonymous namespace, resolvePathNetworks at lines 406-416, the stubs from Task 5, and addRoutes)

Step 1: In the anonymous namespace, before resolvePathNetworks, add:

```cpp
// Derived, on a network's entity from path-networks to the finisher that drops it: the network the
// previous resolution derived.
struct PreviousNetwork {
  Network Held;
};

template <typename Visitor> void visitFields(Visitor &visitor, PreviousNetwork &previous) {
  visitor.field("network", previous.Held);
}
```

Step 2: Replace the last two statements of resolvePathNetworks's loop body with:

```cpp
    const EntityKey key =
        world.createDerivedEntity(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
    const entt::entity entity = world.findEntity(key);
    if (Network *held = world.Registry.try_get<Network>(entity)) {
      world.Registry.emplace_or_replace<PreviousNetwork>(entity, PreviousNetwork{std::move(*held)});
    }
    world.Registry.emplace_or_replace<Network>(entity, std::move(network));
```

Step 3: After resolvePathNetworks, still in the anonymous namespace, add:

```cpp
// Removes both kinds' previous networks, so no resolution ends holding one.
void dropPreviousNetworks(World &world) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const entt::entity entity = world.findEntity(networkKey(kind));
    if (entity != entt::null) {
      world.Registry.remove<PreviousNetwork>(entity);
    }
  }
}
```

Step 4: Replace the two stubs from Task 5 with:

```cpp
const Network *previousNetwork(const World &world, PathKind kind) {
  const entt::entity entity = world.findEntity(networkKey(kind));
  if (entity == entt::null) {
    return nullptr;
  }
  const PreviousNetwork *previous = world.Registry.try_get<PreviousNetwork>(entity);
  return previous != nullptr ? &previous->Held : nullptr;
}
```

and, after addRoutes:

```cpp
void addDropPreviousNetworks(WorldSchema &schema) { schema.addFinisher(&dropPreviousNetworks); }
```

Step 5: Make addRoutes register the component before the resolver:

```cpp
void addRoutes(WorldSchema &schema) {
  schema.addComponent<PreviousNetwork>("previous-network", DataKind::Derived);
  schema.addResolver("path-networks", &resolvePathNetworks);
  addRouteDistance(schema);
}
```

Step 6: Build and run the routes tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`, then, for each file under tests/sim/routes the test pass wrote or changed, `build/windows-debug/tpj_sim_tests.exe -# "[#<file name without .cpp>]" 2>&1 | tr -d '\r' | tail -3`
Expected: the build has no warnings, and the test pass's tests of criteria 1 and 2 pass, with that file's other tests.

### Task 9: Carry guests

Files:
- Modify: `src/sim/guests/guests.cpp` (the anonymous namespace before `} // namespace` at line 374, stepGuests's comment at lines 347-349, and addGuests at line 452)

Step 1: Before the anonymous namespace's closing `} // namespace`, add:

```cpp
// Carries each guest's place from the guest network before the resolution to the one it derived:
// by carryOver, and for a retired place, to the new network's nearest place to where the guest
// stood. A place with neither is left as it is, and its guest leaves when it next steps. Nothing
// changes in a world's first resolution, which has no network before.
void carryGuests(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  if (before == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  for (const EntityKey key : parkGuests(world)) {
    Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<Place> carried = carryOver(guest.At, *before, after)) {
      guest.At = *carried;
      continue;
    }
    const std::optional<GroundPoint> stood = before->groundPoint(guest.At);
    if (!stood) {
      continue;
    }
    if (const std::optional<Place> nearest = after.nearestPlace(*stood)) {
      guest.At = *nearest;
    }
  }
}
```

parkGuests is declared in guests.h and defined after the anonymous namespace, so it is visible here.

Step 2: Replace stepGuests's comment with:

```cpp
// Each guest, in ascending key order, leaves when its place does not resolve, which carrying
// leaves only when the guest network has no carrier, and otherwise gets hungrier, and unless it is
// still waiting for its visit to come back, walks. Guests that leave go after all have stepped,
// and then the entrances admit new ones.
```

Step 3: Register the finisher:

```cpp
void addGuests(WorldSchema &schema) {
  schema.addComponent<Guest>("guest", DataKind::State);
  schema.addSystem(&stepGuests);
  schema.addFinisher(&carryGuests);
}
```

Step 4: Build and run the guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`, then, for each file under tests/sim/guests, `build/windows-debug/tpj_sim_tests.exe -# "[#<file name without .cpp>]" 2>&1 | tr -d '\r' | tail -3`
Expected: the build has no warnings, and every guests test passes, including the test pass's tests of criteria 3 to 6.

### Task 10: Confirm the criteria

Step 1: Format the changed files.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
Expected: no output.

Step 2: Build and test Windows.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -3 && ctest.exe --preset windows-debug 2>&1 | tr -d '\r' | tail -4`
Expected: no warnings, and 100% tests passed.

Step 3: Build and test Linux, and run clang-tidy.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -4; scripts/tidy.sh 2>&1 | tail -3`
Expected: no warning lines, 100% tests passed, and `tidy: clean.`

Step 4: Run the cross-build check (criterion 7).

Run: `scripts/cross-build-check.sh 2>&1 | tail -3`
Expected: the check reports the builds' outputs identical.

Step 5: Check the app still runs.

Run, from build/windows-debug: `timeout 90 ./ThemeParkJones.exe --park ../../tests/parks/supply.park --ticks 1800 --capture supply.bmp`
Expected: exit 0, and the capture shows guests on the paths as before.

### Task 11: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, and present the findings to Evan.

Step 2: Commit once, via the commit-hygiene skill, with the subject `Guests: Carry guests' places across park edits` and a body naming the previous networks, the carrying finisher, and the nearest-point rule for retired places.
