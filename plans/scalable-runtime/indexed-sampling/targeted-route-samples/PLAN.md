# Implementation Plan: Targeted Route Samples

## Goal

Make a walking guest read only the route distance entry it follows, and a shop's offer, without allocating, and sample every source only when it chooses, with the simulation's output unchanged.

## Approach

The medium gains sourceSlot, a binary search for one source's slot by the layer rule, and sourceEntryAtNode, a scan of that slot alone against a node's stop places; the network gains firstAnchoredNode. routes gains routeEntryAt, which uses them at a node and applies route distance's edge rule, moved into helpers sampleRouteEdge shares, inside an edge. guests.cpp's headingOf and suppliedOffer read through them, and choose takes its own full sample, so walk no longer samples on every pass.

## Placement

Decision 0027 places each behavior this feature adds:

- One source's slot by the layer rule, and its first entry at a node: shared medium, sourceSlot and sourceEntryAtNode in src/sim/medium/field.h, beside layeredSlots and sampleSlotAtNode, which own the layer rule and the node rule they restate for one source.
- An entity's lowest anchored node: shared medium, Network::firstAnchoredNode in src/sim/medium/network.h, beside anchoredNodes, over the anchors the network owns.
- One source's route entry at a place, and the edge rule's helpers: navigable-networks, routeEntryAt in src/sim/routes/route_distance.h, beside sampleRouteEdge, since routes owns route distance and its edge rule.
- Reading one entry while walking and the full sample only when choosing: believable-guests, walk, headingOf, choose, and suppliedOffer in src/sim/guests/guests.cpp, which already own the walk.

## Tasks

### Task 1: Record tpj_scenarios' output before the change

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && mkdir -p build/targeted-route && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/targeted-route/before.txt && wc -l build/targeted-route/before.txt`
Expected: the link or no-work line, then a line count greater than 0.

### Task 2: Specs

Files:
- Modify: `src/sim/medium/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/guests/SPEC.md`

Step 1: Make each change under Spec changes in plans/scalable-runtime/indexed-sampling/targeted-route-samples/FEATURE.md, verbatim.

### Task 3: Interfaces

Files:
- Modify: `src/sim/medium/network.h`, `src/sim/medium/network.cpp`, `src/sim/medium/field.h`, `src/sim/routes/route_distance.h`, `src/sim/routes/route_distance.cpp`

Step 1: In src/sim/medium/network.h, after the declaration of anchoredNodes, add:

```cpp
  // The lowest node anchored to the entity, the first anchoredNodes gives, or none. Allocates
  // nothing.
  [[nodiscard]] std::optional<uint32_t> firstAnchoredNode(EntityKey entity) const;
```

and in src/sim/medium/network.cpp, after Network::anchoredNodes:

```cpp
std::optional<uint32_t> Network::firstAnchoredNode(EntityKey /*entity*/) const {
  return std::nullopt;
}
```

Step 2: In src/sim/medium/field.h, after layeredSlots, add:

```cpp
// The source's slot as the layer rule chooses it: its readable stepped slot when it has one, and
// otherwise its resolved one, or none. Allocates nothing.
template <FieldDefinition F>
const FieldSlot<typename F::Entry> *sourceSlot(const World & /*world*/, EntityKey /*source*/) {
  return nullptr;
}
```

and after sampleField:

```cpp
// The first of the source's entries that sampleField gives at the node's nodePlace, or none.
// Throws std::out_of_range for a node not below the node count. Allocates nothing.
template <FieldDefinition F>
std::optional<typename F::Entry> sourceEntryAtNode(const World & /*world*/,
                                                   const Network & /*network*/,
                                                   uint32_t /*node*/, EntityKey /*source*/) {
  return std::nullopt;
}
```

Step 3: In src/sim/routes/route_distance.h, after the RouteDistance template, add:

```cpp
// The source's entry in the kind's route distance field at the place on the network: exactly the
// entry sampleField of that field gives the source there, or none. Reads only the source's
// entries and allocates nothing.
std::optional<RouteEntry> routeEntryAt(const World &world, PathKind kind, const Network &network,
                                       const Place &place, EntityKey source);
```

and in src/sim/routes/route_distance.cpp, after sampleRouteEdge:

```cpp
std::optional<RouteEntry> routeEntryAt(const World & /*world*/, PathKind /*kind*/,
                                       const Network & /*network*/, const Place & /*place*/,
                                       EntityKey /*source*/) {
  return std::nullopt;
}
```

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec"`
Expected: the link line of tpj_sim_tests.exe, no warnings.

### Task 4: Test pass

Dispatch the test-writer for criteria 1 to 4 against src/sim/medium/field.h, src/sim/medium/network.h, src/sim/routes/route_distance.h, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, and docs/principles.md, in new tests/sim/medium/source_sample_test.cpp and tests/sim/routes/route_entry_test.cpp registered in tests/sim/CMakeLists.txt. Criteria 5 to 7 are checks.

### Task 5: firstAnchoredNode

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Replace the stub with:

```cpp
std::optional<uint32_t> Network::firstAnchoredNode(EntityKey entity) const {
  std::optional<uint32_t> first;
  for (const NodeAnchor &anchor : Anchors) {
    if (anchor.Entity == entity && (!first || anchor.Node < *first)) {
      first = anchor.Node;
    }
  }
  return first;
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#source_sample_test]"`
Expected: the firstAnchoredNode tests pass.

### Task 6: sourceSlot and sourceEntryAtNode

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace sourceSlot's stub body, naming its parameters world and source, with:

```cpp
  using Slot = FieldSlot<typename F::Entry>;
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  if (entity == entt::null) {
    return nullptr;
  }
  // Both lists keep their slots in ascending source order.
  const auto find = [source](const std::vector<Slot> &slots) -> const Slot * {
    const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
    return at != slots.end() && at->Source == source ? &*at : nullptr;
  };
  if (const auto *stepped = world.Registry.try_get<SteppedEntries<F>>(entity)) {
    if (const Slot *slot = find(stepped->Readable)) {
      return slot;
    }
  }
  const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);
  return resolved != nullptr ? find(resolved->Slots) : nullptr;
```

Step 2: Replace sourceEntryAtNode's stub body, naming its parameters world, network, node, and source, with:

```cpp
  const std::span<const Place> stops = network.stopPlaces(node);
  const FieldSlot<typename F::Entry> *slot = sourceSlot<F>(world, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  for (const PlacedEntry<typename F::Entry> &entry : slot->Entries) {
    if (std::ranges::find(stops, entry.At) != stops.end()) {
      return entry.Value;
    }
  }
  return std::nullopt;
```

stopPlaces comes first so a node out of range throws whether or not the source has a slot.

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#source_sample_test]"`
Expected: all source_sample_test tests pass.

### Task 7: The edge rule's helpers

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Inside the anonymous namespace, before its closing `} // namespace`, add:

```cpp
// The entry a place inside the edge takes from an entry at the edge's From end, the place's offset
// from that end added.
RouteEntry viaFrom(const NetworkEdge &edge, const RouteEntry &entry, double offset) {
  return {entry.Distance + offset, {edge.Carrier, edge.ToDistance, edge.FromDistance}};
}

// The entry a place inside the edge takes from an entry at the edge's To end.
RouteEntry viaTo(const NetworkEdge &edge, const RouteEntry &entry, double offset) {
  return {entry.Distance + offset, {edge.Carrier, edge.FromDistance, edge.ToDistance}};
}

// Keeps the candidate when it is the first or strictly less, so the earlier of equals stays.
void keepLeast(std::optional<RouteEntry> &best, const RouteEntry &candidate) {
  if (!best || candidate.Distance < best->Distance) {
    best = candidate;
  }
}
```

Step 2: Replace sampleRouteEdge's body with:

```cpp
  std::optional<RouteEntry> best;
  // The From end's entries come first, so it wins ties.
  for (const RouteEntry &entry : sample.AtFrom) {
    keepLeast(best, viaFrom(sample.Edge, entry, sample.FromOffset));
  }
  for (const RouteEntry &entry : sample.AtTo) {
    keepLeast(best, viaTo(sample.Edge, entry, sample.ToOffset));
  }
  if (!best) {
    return {};
  }
  return {*best};
```

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#route_distance_test]"`
Expected: all route_distance_test tests pass.

### Task 8: routeEntryAt

Files:
- Modify: `src/sim/routes/route_distance.cpp`

Step 1: Add `#include <span>` and `#include <variant>` to the standard includes, in order.

Step 2: Inside the anonymous namespace, after keepLeast, add:

```cpp
template <PathKind Kind>
std::optional<RouteEntry> routeEntryOn(const World &world, const Network &network,
                                       const Place &place, EntityKey source) {
  using Field = RouteDistance<Kind>;
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return std::nullopt;
  }
  if (const auto *node = std::get_if<NodePosition>(&*position)) {
    return sourceEntryAtNode<Field>(world, network, node->Node, source);
  }
  const FieldSlot<RouteEntry> *slot = sourceSlot<Field>(world, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  const EdgePosition &inside = std::get<EdgePosition>(*position);
  const NetworkEdge &edge = network.edges()[inside.Edge];
  std::optional<RouteEntry> best;
  // The source's entries at one end, in its order, as the medium's edge sample lists them: an
  // entry strictly inside the edge is at neither end.
  const auto atEnd = [&edge, slot](std::span<const Place> stops, const auto &take) {
    for (const PlacedEntry<RouteEntry> &entry : slot->Entries) {
      const Place &at = entry.At;
      const bool within = at.Carrier == edge.Carrier && at.Distance > edge.FromDistance &&
                          at.Distance < edge.ToDistance;
      if (!within && std::ranges::find(stops, at) != stops.end()) {
        take(entry.Value);
      }
    }
  };
  atEnd(network.stopPlaces(edge.From), [&](const RouteEntry &entry) {
    keepLeast(best, viaFrom(edge, entry, inside.FromOffset));
  });
  atEnd(network.stopPlaces(edge.To), [&](const RouteEntry &entry) {
    keepLeast(best, viaTo(edge, entry, inside.ToOffset));
  });
  return best;
}
```

Step 3: Replace routeEntryAt's stub, naming its parameters, with:

```cpp
std::optional<RouteEntry> routeEntryAt(const World &world, PathKind kind, const Network &network,
                                       const Place &place, EntityKey source) {
  return kind == PathKind::Guest ? routeEntryOn<PathKind::Guest>(world, network, place, source)
                                 : routeEntryOn<PathKind::Backstage>(world, network, place, source);
}
```

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#route_entry_test],[#route_distance_test]"`
Expected: all pass.

### Task 9: Guests read one entry while walking

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Delete entryOf and its comment. After homeEntry, add:

```cpp
// The least entrance entry at the place, ties to the lower key, or none, as homeEntry finds it in a
// sample there.
std::optional<RouteEntry> homeEntryAt(const World &world, const Network &network,
                                      const Place &place, const std::vector<EntityKey> &entrances) {
  std::optional<RouteEntry> nearest;
  // Entrances come in ascending key order, so keeping only a strictly less distance breaks ties
  // to the lower key.
  for (const EntityKey entrance : entrances) {
    const std::optional<RouteEntry> entry =
        routeEntryAt(world, PathKind::Guest, network, place, entrance);
    if (entry && (!nearest || entry->Distance < nearest->Distance)) {
      nearest = entry;
    }
  }
  return nearest;
}
```

Step 2: Replace suppliedOffer's body with:

```cpp
  const std::optional<uint32_t> anchor = network.firstAnchoredNode(source);
  if (!anchor) {
    return std::nullopt;
  }
  const std::optional<OfferEntry> offer =
      sourceEntryAtNode<FoodOffer>(world, network, *anchor, source);
  if (!offer || !offer->Supplied) {
    return std::nullopt;
  }
  return offer;
```

Step 3: Change choose to take its own sample: remove its `const RouteSample &routes` parameter, and make its first statement `const RouteSample routes = sampleField<GuestRouteDistance>(world, network, guest.At);`, before scoring. Its comment becomes `// Samples route distance where the guest stands, scores its options there, picks one by softmax, and takes it up.` (wrapped at 100 columns).

Step 4: Replace Heading and headingOf with:

```cpp
struct Heading {
  std::optional<RouteEntry> Target;
  std::optional<RouteEntry> Home;
  bool Dropped = false;
};

Heading headingOf(const World &world, Guest &guest, const Network &network,
                  const std::vector<EntityKey> &entrances) {
  Heading heading;
  if (guest.Activity == GuestActivity::HeadingToShop) {
    heading.Target = routeEntryAt(world, PathKind::Guest, network, guest.At, guest.Target);
    if (!heading.Target || !suppliedOffer(world, network, guest.Target)) {
      guest.Activity = GuestActivity::Wandering;
      guest.Target = NULL_KEY;
      heading.Target.reset();
      heading.Dropped = true;
    }
  }
  if (guest.Activity == GuestActivity::HeadingHome) {
    heading.Home = homeEntryAt(world, network, guest.At, entrances);
    if (!heading.Home) {
      guest.Activity = GuestActivity::Wandering;
      heading.Dropped = true;
    }
  }
  return heading;
}
```

keeping the comment above Heading.

Step 5: In walk, delete the line `const RouteSample routes = sampleField<GuestRouteDistance>(world, network, guest.At);`, call `headingOf(world, guest, network, entrances)` and `choose(world, key, guest, network, entrances, choices)`, and replace each `heading.Target != nullptr` with `heading.Target` and each `heading.Home != nullptr` with `heading.Home`.

Step 6: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe`
Expected: all tpj_sim_tests pass.

### Task 10: Confirm the criteria

Step 1 (criteria 1 to 5): Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: no diagnostics, `100% tests passed`, `tidy: clean.`

Step 2 (criterion 5): Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/targeted-route/after.txt; build/windows-debug/tpj_scenarios.exe --compare "$(wslpath -w build/targeted-route/before.txt)" "$(wslpath -w build/targeted-route/after.txt)"; echo status $?`
Expected: `status 0`.

Step 3 (criterion 6): Build the profile build and profile, by the recipe: `cmake.exe --preset windows-release -B build/windows-profile -DCMAKE_CXX_FLAGS="-g -fno-omit-frame-pointer" -DCMAKE_C_FLAGS="-g" && cmake.exe --build build/windows-profile --target tpj_bench`, then `W=$(wslpath -w .); "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-profile\tpj_bench.exe --ticks 600 $W\tests\parks\stress\full.park" "/o:$W\build\windows-profile\full.sleepy"`. Unzip Callstacks.txt and Symbols.txt from build/windows-profile/full.sleepy and sum, per stack, the seconds of stacks through stepWorld and of those through a sampleField symbol naming RouteDistance.
Expected: sampleField of guest route distance under 5% of stepWorld's seconds, and stepWorld's seconds per tick, divided by 600, reported beside the 23.5 ms before.

Step 4 (criterion 7): Run: `scripts/runtime-report.sh build/runtime-report/indexed-sampling-after.txt` and then `scripts/runtime-report.sh --compare build/runtime-report/indexed-sampling-before.txt build/runtime-report/indexed-sampling-after.txt`.
Expected: the show output's windows-release full.park ticks line has a median below 33333.3 and no over-budget mark; the windows-release full.park app frames line has a median below 33333.3 and no over-budget mark; the comparison's windows-release full.park ticks line says faster. Put both outputs in the feature's report, with the full park's app frames and food overlay before and after.

### Task 11: Commit

Stage the feature's paths only, since the tree holds another session's staged and untracked files: `git add src/sim/medium/field.h src/sim/medium/network.h src/sim/medium/network.cpp src/sim/medium/SPEC.md src/sim/routes/route_distance.h src/sim/routes/route_distance.cpp src/sim/routes/SPEC.md src/sim/guests/guests.cpp src/sim/guests/SPEC.md tests/sim/medium/source_sample_test.cpp tests/sim/routes/route_entry_test.cpp tests/sim/CMakeLists.txt`, and commit them alone with `git commit -- <those paths>` so plans/slices/paths-and-plazas stays out. Review per implementing-features, and commit once via commit-hygiene with the subject `Guests: Read only the followed route entry while walking`.
