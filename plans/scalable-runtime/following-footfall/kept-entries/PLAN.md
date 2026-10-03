# Implementation Plan: Kept Entries

## Goal

Give the shared medium kept fields, whose entries a system changes at one place and which stay readable until changed, read through their owner's rule at the reading world's tick, with no change to any existing field or to the simulation's output.

## Approach

A new header, src/sim/medium/kept_field.h, holds the kept field's types, its registration, keepEntry and the swap that applies a tick's changes, the reads, and replaceKeptEntries. field.h's position helpers and sampling helpers are generalized from field slots to any slot with placed entries and an order by place, and from reading an entry's value to a reader the caller gives, so kept slots reuse them with readKept as the reader; sampleField dispatches a kept field to sampleKeptField, declared in field.h and defined in kept_field.h. Network gains edgeEnds, built in its constructor beside the stop places, for kept fields' node rule, and World gains isFinishing so replaceKeptEntries can refuse callers other than finishers.

## Placement

Decision 0027 places each behavior this feature adds:

- Kept fields, their entries, registration, changes, swap, reads, node rule, and replacement: shared medium, src/sim/medium/kept_field.h. The medium owns fields and how they are held and sampled; a header of its own keeps kept fields apart from the layered fields in field.h, which every field's user includes.
- sampleField's dispatch to kept fields, and the position and sampling helpers taking any slot and a value reader: shared medium, src/sim/medium/field.h, which owns sampleField and the helpers, and is their only definer.
- Each node's edge ends: shared medium, Network in src/sim/medium/network.h and network.cpp. The network owns its topology and already builds each node's stop places in its constructor.
- Whether the finishers are running: tpj_sim's World in src/sim/world.h, which holds the cycle's other flags, isResolving and isStepping.

## Tasks

### Task 1: Record tpj_scenarios's output before the change

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && mkdir -p build/kept-entries && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/kept-entries/before.txt && wc -l build/kept-entries/before.txt`
Expected: the link or no-work line, then a line count greater than 0.

### Task 2: Spec

Files:
- Modify: `src/sim/medium/SPEC.md`, `src/sim/SPEC.md`

Step 1: Make each change under Spec changes in plans/scalable-runtime/following-footfall/kept-entries/FEATURE.md, verbatim.

### Task 3: Interface: helpers that take any slot and a value reader

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace orderByPlace with:

```cpp
// The positions of the items whose place's distance is not NaN, each once, ordered by carrier key,
// then distance, then position. Items are field entries or kept entries.
template <typename Item> std::vector<uint32_t> orderItemsByPlace(std::span<const Item> items) {
  std::vector<uint32_t> order;
  order.reserve(items.size());
  for (uint32_t i = 0; i < items.size(); ++i) {
    // A NaN distance equals no place and lies inside no edge, and would break the ordering.
    if (!std::isnan(items[i].At.Distance)) {
      order.push_back(i);
    }
  }
  std::ranges::stable_sort(
      order, [items](uint32_t a, uint32_t b) { return placeBefore(items[a].At, items[b].At); });
  return order;
}

// The positions of the entries whose distance is not NaN, each once, ordered by carrier key, then
// distance, then position.
template <typename Entry>
std::vector<uint32_t> orderByPlace(std::span<const PlacedEntry<Entry>> entries) {
  return orderItemsByPlace(entries);
}
```

Step 2: Replace entriesByPlace with:

```cpp
// The slot's entries ordered by place, as orderItemsByPlace gives them, made on the first call and
// kept in the slot. A slot is a field slot or a kept slot.
template <typename Slot> std::span<const uint32_t> entriesByPlace(const Slot &slot) {
  if (!slot.ByPlace) {
    slot.ByPlace = orderItemsByPlace(std::span(slot.Entries));
  }
  return *slot.ByPlace;
}
```

Step 3: In positionsAt, positionsInside, and positionsAtAny, replace `template <typename Entry>` with `template <typename Slot>` and the parameter `const FieldSlot<Entry> &slot` with `const Slot &slot`. Their bodies are unchanged.

Step 4: Replace sampleSlotAtNode with:

```cpp
// Appends the source's entries whose places resolve to the node: exactly those at its stop
// places, found through the slot's order by place, each with the value read gives its position.
template <typename Entry, typename Slot, typename Read>
void sampleSlotAtNode(const Network &network, const Slot &slot, uint32_t node,
                      SampleScratch<Entry> &scratch, std::vector<SampledEntry<Entry>> &sampled,
                      Read read) {
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sampled.push_back({slot.Source, read(i)});
  }
}
```

Step 5: In sampleSlotInEdge, replace `template <FieldDefinition F>` with `template <FieldDefinition F, typename Slot, typename Read>`, the parameter `const FieldSlot<typename F::Entry> &slot` with `const Slot &slot`, and add a last parameter `Read read`. In its body, replace the loop over `inside` with:

```cpp
    for (const uint32_t i : inside) {
      const Place &at = slot.Entries[i].At;
      sample.Along.push_back({at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, read(i)});
    }
```

and replace each other `slot.Entries[i].Value` with `read(i)`. Update its comment's first line to `// Appends the source's entries at a place strictly inside an edge, each with the value read gives` and its second to `// its position: those at the same place, or what the field's own rule gives when it has one.`

Step 6: In sampleSlots, at the top of the loop over slots, add:

```cpp
    const auto read = [slot](uint32_t i) { return slot->Entries[i].Value; };
```

and pass `read` as the last argument of its calls to sampleSlotAtNode and sampleSlotInEdge.

Step 7: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#field_index_test],[#field_sampling_test],[#field_test],[#source_sample_test],[#stepped_field_test]"`
Expected: all of those tests pass, as before the task.

### Task 4: Interface: kept fields

Files:
- Modify: `src/sim/world.h`, `src/sim/medium/network.h`, `src/sim/medium/network.cpp`, `src/sim/medium/field.h`
- Create: `src/sim/medium/kept_field.h`

Step 1: In src/sim/world.h, after isStepping, add:

```cpp
  // True only while the finishers run.
  [[nodiscard]] bool isFinishing() const { return Finishing; }
```

Step 2: In src/sim/medium/network.h, before `using NetworkPosition`, add:

```cpp
// An end of an edge at a node: the edge's index in edges(), and whether the end is its From.
struct EdgeEnd {
  uint32_t Edge = 0;
  bool AtFrom = true;
};
```

After stopPlaces' declaration, add:

```cpp
  // The ends at the node of the edges meeting it, in ascending edge index with an edge's From end
  // before its To end, so an edge whose two ends are the node appears twice. Throws
  // std::out_of_range as nodePlace does.
  [[nodiscard]] std::span<const EdgeEnd> edgeEnds(uint32_t node) const;
```

In the comment above visitFields, replace `The edges and stop places are rebuilt` with `The edges, stop places, and edge ends are rebuilt`. After StopStarts, add:

```cpp
  // Each node's edge ends, node by node: node n's run from EdgeEndStarts[n] up to
  // EdgeEndStarts[n + 1].
  std::vector<EdgeEnd> EdgeEnds;
  std::vector<size_t> EdgeEndStarts;
```

Step 3: In src/sim/medium/network.cpp, after Network::stopPlaces, add the stub:

```cpp
std::span<const EdgeEnd> Network::edgeEnds(uint32_t node) const {
  static_cast<void>(node);
  return {};
}
```

Step 4: In src/sim/medium/field.h, after the HasEdgeRule concept, add:

```cpp
// A kept field: a scalar field whose owner reads a kept entry by its own rule, given the entry's
// value and the ticks since it last changed:
//   static double readKept(double value, uint64_t ticks);
// It registers with addKeptField, in sim/medium/kept_field.h, which holds the rest of it.
template <typename F>
concept KeptFieldDefinition =
    FieldDefinition<F> && (F::Kind == FieldKind::Scalar) && requires(double value, uint64_t ticks) {
      { F::readKept(value, ticks) } -> std::same_as<double>;
    };
```

In addField, after its static_assert, add:

```cpp
  static_assert(!KeptFieldDefinition<F>, "a kept field registers with addKeptField");
```

Before sampleField, add:

```cpp
// A kept field's entries at the place on the network, as sampleField gives them. Defined in
// sim/medium/kept_field.h, which a kept field's definition includes.
template <KeptFieldDefinition F>
std::vector<SampledEntry<double>> sampleKeptField(const World &world, const Network &network,
                                                  const Place &place);
```

Replace sampleField's body with:

```cpp
  if constexpr (KeptFieldDefinition<F>) {
    return sampleKeptField<F>(world, network, place);
  } else {
    const entt::entity entity = world.findEntity(fieldKey(F::Name));
    if (entity == entt::null) {
      return {};
    }
    return sampleSlots<F>(network, place, layeredSlots<F>(world, entity));
  }
```

Step 5: Create src/sim/medium/kept_field.h:

```cpp
#ifndef TPJ_SIM_MEDIUM_KEPT_FIELD_H
#define TPJ_SIM_MEDIUM_KEPT_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

// A kept field's entry: a value at a place, and the World::Tick at which the value became
// readable.
struct KeptEntry {
  Place At;
  double Value = 0.0;
  uint64_t Tick = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptEntry &entry) {
  visitor.field("at", entry.At);
  visitor.field("value", entry.Value);
  visitor.field("tick", entry.Tick);
}

// One source's kept entries, in the order their places were first kept.
struct KeptSlot {
  EntityKey Source = NULL_KEY;
  std::vector<KeptEntry> Entries;
  // The positions of Entries ordered by place, made the first time it is needed and kept as the
  // swap adds entries. Derived from Entries' places, which nothing changes, and never visited, so
  // saves, hashes, and comparisons never see it.
  mutable std::optional<std::vector<uint32_t>> ByPlace = std::nullopt;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptSlot &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
}

// A change a system made during the tick, applied by the swap that ends it.
struct KeptChange {
  EntityKey Source = NULL_KEY;
  Place At;
  double Value = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptChange &change) {
  visitor.field("source", change.Source);
  visitor.field("at", change.At);
  visitor.field("value", change.Value);
}

// A kept field's entries. State. Slots holds the readable entries, sources in ascending key order,
// and Pending the changes made during the current tick, in the order made.
template <KeptFieldDefinition F> struct KeptEntries {
  std::vector<KeptSlot> Slots;
  std::vector<KeptChange> Pending;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, KeptEntries<F> &kept) {
  visitor.field("slots", kept.Slots);
  visitor.field("pending", kept.Pending);
}

// One end, at a sampled node, of an edge meeting it, with the source's entries strictly inside the
// edge.
struct NodeEnd {
  NetworkEdge Edge;
  // Whether the node is the edge's From end; false for its To end.
  bool AtFrom = true;
  std::vector<EdgeEntry<double>> Along;
};

// What a kept field's node rule is given for one source at a node.
struct NodeSample {
  uint32_t Node = 0;
  std::vector<double> AtNode;
  std::vector<NodeEnd> Ends;
};

// A kept field whose owner reads nodes by its own rule:
//   static std::vector<double> sampleNode(const NodeSample &sample);
template <typename F>
concept HasNodeRule = KeptFieldDefinition<F> && requires(const NodeSample &sample) {
  { F::sampleNode(sample) } -> std::same_as<std::vector<double>>;
};

// The field's kept entries, or null when the world holds none.
template <KeptFieldDefinition F> const KeptEntries<F> *findKeptEntries(const World &world) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  return entity == entt::null ? nullptr : world.Registry.try_get<KeptEntries<F>>(entity);
}

template <KeptFieldDefinition F> KeptEntries<F> *findKeptEntries(World &world) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  return entity == entt::null ? nullptr : world.Registry.try_get<KeptEntries<F>>(entity);
}

// The source's slot among slots in ascending source order, or null.
inline const KeptSlot *findKeptSlot(const std::vector<KeptSlot> &slots, EntityKey source) {
  const auto at = std::ranges::lower_bound(slots, source, {}, &KeptSlot::Source);
  return at != slots.end() && at->Source == source ? &*at : nullptr;
}

// The entry's value read at the tick by the field's rule, with 0 ticks when the entry's tick is
// later.
template <KeptFieldDefinition F> double readKeptEntry(const KeptEntry &entry, uint64_t tick) {
  return F::readKept(entry.Value, tick > entry.Tick ? tick - entry.Tick : 0);
}

// The field's resolver: creates the field's entity holding empty kept entries when it holds none.
template <KeptFieldDefinition F> void emptyKeptEntries(World &world) {
  static_cast<void>(world);
}

// The field's swap: applies the tick's changes in the order made, then empties them.
template <KeptFieldDefinition F> void applyKeptChanges(World &world) {
  static_cast<void>(world);
}

// Registers the kept field's state component, <name>-kept, its resolver, <name>-field, and its
// swap. Throws std::invalid_argument when a name is malformed or already registered.
template <KeptFieldDefinition F> void addKeptField(WorldSchema &schema) {
  static_cast<void>(schema);
}

// Sets the source's entry at the place to the value when the swap ending this tick runs. Systems
// only. Throws std::logic_error when the world is not stepping or holds no kept entries for the
// field, and std::invalid_argument for the null key or a NaN distance.
template <KeptFieldDefinition F>
void keepEntry(World &world, EntityKey source, const Place &place, double value) {
  static_cast<void>(world);
  static_cast<void>(source);
  static_cast<void>(place);
  static_cast<void>(value);
}

// The read value of the source's first entry at exactly the place, or none, and none at a NaN
// distance.
template <KeptFieldDefinition F>
std::optional<double> keptValue(const World &world, EntityKey source, const Place &place) {
  static_cast<void>(world);
  static_cast<void>(source);
  static_cast<void>(place);
  return std::nullopt;
}

// The source's kept entries as held, unread, in its order, or an empty list.
template <KeptFieldDefinition F>
std::span<const KeptEntry> keptEntries(const World &world, EntityKey source) {
  static_cast<void>(world);
  static_cast<void>(source);
  return {};
}

// Makes the source's kept entries the given ones, in the given order; an empty list removes the
// source. Finishers only. Throws std::logic_error when the finishers are not running or the world
// holds no kept entries for the field, and std::invalid_argument for the null key, an entry whose
// distance is NaN or whose tick is later than the world's, or two entries at one place.
template <KeptFieldDefinition F>
void replaceKeptEntries(World &world, EntityKey source, std::vector<KeptEntry> entries) {
  static_cast<void>(world);
  static_cast<void>(source);
  static_cast<void>(entries);
}

template <KeptFieldDefinition F>
std::vector<SampledEntry<double>> sampleKeptField(const World &world, const Network &network,
                                                  const Place &place) {
  static_cast<void>(world);
  static_cast<void>(network);
  static_cast<void>(place);
  return {};
}

} // namespace tpj

#endif
```

Step 6: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec"`
Expected: the link line of tpj_sim_tests.exe, no warnings.

Step 7: kept_field.h has no includer until the test pass, so check it alone. Run: `printf '#include "sim/medium/kept_field.h"\n' > build/kept-entries/check.cpp && entt=$(grep -o -m1 'isystem [^ ]*entt[^ ]*' build/windows-debug/compile_commands.json | cut -d' ' -f2) && g++.exe -std=c++20 -fsyntax-only -Wall -Wextra -Werror -I"$(wslpath -w src)" -isystem "$entt" "$(wslpath -w build/kept-entries/check.cpp)"; echo status $?`
Expected: `status 0`.

### Task 5: Test pass

Dispatch the test-writer for criteria 1 to 9 against src/sim/medium/kept_field.h, src/sim/medium/field.h, src/sim/medium/network.h, src/sim/world.h, src/sim/medium/SPEC.md, src/sim/SPEC.md, and docs/principles.md: criterion 6 in tests/sim/medium/network_test.cpp, and the rest in a new tests/sim/medium/kept_field_test.cpp registered in tests/sim/CMakeLists.txt. Criterion 10 is a check.

### Task 6: Edge ends

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: At the end of Network's constructor, after the loop that fills StopPlaces, add:

```cpp
  // Edges in index order leave each node's run in ascending edge index, From before To.
  EdgeEndStarts.assign(static_cast<size_t>(NodeCount) + 1, 0);
  for (const NetworkEdge &edge : Edges) {
    ++EdgeEndStarts[static_cast<size_t>(edge.From) + 1];
    ++EdgeEndStarts[static_cast<size_t>(edge.To) + 1];
  }
  for (size_t node = 0; node < NodeCount; ++node) {
    EdgeEndStarts[node + 1] += EdgeEndStarts[node];
  }
  EdgeEnds.resize(EdgeEndStarts.back());
  std::vector<size_t> endsFilled(EdgeEndStarts.begin(), EdgeEndStarts.end() - 1);
  for (size_t i = 0; i < Edges.size(); ++i) {
    const auto index = static_cast<uint32_t>(i);
    EdgeEnds[endsFilled[Edges[i].From]++] = EdgeEnd{.Edge = index, .AtFrom = true};
    EdgeEnds[endsFilled[Edges[i].To]++] = EdgeEnd{.Edge = index, .AtFrom = false};
  }
```

Step 2: Replace edgeEnds' stub body with:

```cpp
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  return std::span<const EdgeEnd>(EdgeEnds).subspan(EdgeEndStarts[node],
                                                    EdgeEndStarts[node + 1] - EdgeEndStarts[node]);
```

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#network_test]"`
Expected: all network tests pass, the test pass's edgeEnds tests among them.

### Task 7: Registration, changes, the swap, and replacement

Files:
- Modify: `src/sim/medium/kept_field.h`

Step 1: Replace the stub bodies of emptyKeptEntries, applyKeptChanges, addKeptField, keepEntry, keptValue, keptEntries, and replaceKeptEntries with:

```cpp
template <KeptFieldDefinition F> void emptyKeptEntries(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  static_cast<void>(world.Registry.get_or_emplace<KeptEntries<F>>(world.findEntity(key)));
}

template <KeptFieldDefinition F> void applyKeptChanges(World &world) {
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    return;
  }
  for (const KeptChange &change : kept->Pending) {
    auto at = std::ranges::lower_bound(kept->Slots, change.Source, {}, &KeptSlot::Source);
    if (at == kept->Slots.end() || at->Source != change.Source) {
      at = kept->Slots.insert(at, KeptSlot{.Source = change.Source, .Entries = {}});
    }
    KeptSlot &slot = *at;
    // Makes the slot's order when it has none, so the order exists below.
    const std::span<const uint32_t> found = positionsAt(slot, change.At);
    if (!found.empty()) {
      KeptEntry &entry = slot.Entries[found.front()];
      entry.Value = change.Value;
      entry.Tick = world.Tick;
      continue;
    }
    const auto position = static_cast<uint32_t>(slot.Entries.size());
    slot.Entries.push_back({.At = change.At, .Value = change.Value, .Tick = world.Tick});
    std::vector<uint32_t> &order = *slot.ByPlace;
    const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
    order.insert(std::ranges::upper_bound(order, change.At, placeBefore, placeOf), position);
  }
  kept->Pending.clear();
}

template <KeptFieldDefinition F> void addKeptField(WorldSchema &schema) {
  const std::string name(F::Name);
  schema.addComponent<KeptEntries<F>>(name + "-kept", DataKind::State);
  schema.addResolver(name + "-field", &emptyKeptEntries<F>);
  schema.addSwap(&applyKeptChanges<F>);
}

template <KeptFieldDefinition F>
void keepEntry(World &world, EntityKey source, const Place &place, double value) {
  // Messages are built only when throwing, since systems call this for each mover every tick.
  if (!world.isStepping()) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " takes kept changes only from systems");
  }
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no kept entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entries from the null key");
  }
  if (std::isnan(place.Distance)) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entry at a NaN distance");
  }
  kept->Pending.push_back({.Source = source, .At = place, .Value = value});
}

template <KeptFieldDefinition F>
std::optional<double> keptValue(const World &world, EntityKey source, const Place &place) {
  // A NaN distance is at no place, and would match a whole carrier's run in the order.
  if (std::isnan(place.Distance)) {
    return std::nullopt;
  }
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  const KeptSlot *slot = kept == nullptr ? nullptr : findKeptSlot(kept->Slots, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  // Positions at a place ascend, so the first is the source's first entry there.
  const std::span<const uint32_t> found = positionsAt(*slot, place);
  if (found.empty()) {
    return std::nullopt;
  }
  return readKeptEntry<F>(slot->Entries[found.front()], world.Tick);
}

template <KeptFieldDefinition F>
std::span<const KeptEntry> keptEntries(const World &world, EntityKey source) {
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  const KeptSlot *slot = kept == nullptr ? nullptr : findKeptSlot(kept->Slots, source);
  return slot == nullptr ? std::span<const KeptEntry>() : std::span<const KeptEntry>(slot->Entries);
}

template <KeptFieldDefinition F>
void replaceKeptEntries(World &world, EntityKey source, std::vector<KeptEntry> entries) {
  const std::string name(F::Name);
  if (!world.isFinishing()) {
    throw std::logic_error("field " + name + " takes kept replacements only from finishers");
  }
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    throw std::logic_error("field " + name + " holds no kept entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  for (const KeptEntry &entry : entries) {
    if (std::isnan(entry.At.Distance)) {
      throw std::invalid_argument("field " + name + " takes no entry at a NaN distance");
    }
    if (entry.Tick > world.Tick) {
      throw std::invalid_argument("field " + name + " takes no entry stamped after tick " +
                                  std::to_string(world.Tick));
    }
  }
  std::vector<uint32_t> order = orderItemsByPlace(std::span<const KeptEntry>(entries));
  // The order ascends, so two entries at one place are neighbours in it.
  for (size_t i = 1; i < order.size(); ++i) {
    if (!placeBefore(entries[order[i - 1]].At, entries[order[i]].At)) {
      throw std::invalid_argument("field " + name + " takes one entry at a place");
    }
  }
  auto at = std::ranges::lower_bound(kept->Slots, source, {}, &KeptSlot::Source);
  const bool held = at != kept->Slots.end() && at->Source == source;
  if (entries.empty()) {
    if (held) {
      kept->Slots.erase(at);
    }
    return;
  }
  if (!held) {
    at = kept->Slots.insert(at, KeptSlot{.Source = source, .Entries = {}});
  }
  at->Entries = std::move(entries);
  at->ByPlace = std::move(order);
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#kept_field_test]"`
Expected: the test pass's registration, change, refusal, replacement, and save tests pass; those that sample through sampleField or fieldValue still fail, since sampleKeptField is a stub.

### Task 8: Reading kept fields

Files:
- Modify: `src/sim/medium/kept_field.h`

Step 1: Before sampleKeptField, add:

```cpp
// Appends what the field's node rule gives for the source at the node, when the source has an
// entry at the node's stop places or strictly inside an edge meeting it, each value read by read.
template <KeptFieldDefinition F, typename Read>
  requires HasNodeRule<F>
void sampleKeptSlotAtNode(const Network &network, const KeptSlot &slot, uint32_t node,
                          SampleScratch<double> &scratch,
                          std::vector<SampledEntry<double>> &sampled, Read read) {
  NodeSample sample{.Node = node, .AtNode = {}, .Ends = {}};
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sample.AtNode.push_back(read(i));
  }
  bool any = !sample.AtNode.empty();
  for (const EdgeEnd &end : network.edgeEnds(node)) {
    const NetworkEdge &edge = network.edges()[end.Edge];
    NodeEnd &nodeEnd = sample.Ends.emplace_back(NodeEnd{.Edge = edge, .AtFrom = end.AtFrom, .Along = {}});
    std::vector<uint32_t> &inside = scratch.Inside;
    inside.clear();
    positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
    std::ranges::sort(inside);
    for (const uint32_t i : inside) {
      const Place &at = slot.Entries[i].At;
      nodeEnd.Along.push_back(
          {at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, read(i)});
    }
    any = any || !nodeEnd.Along.empty();
  }
  if (!any) {
    return;
  }
  for (const double value : F::sampleNode(sample)) {
    sampled.push_back({slot.Source, value});
  }
}
```

Step 2: Replace sampleKeptField's stub body with:

```cpp
  std::vector<SampledEntry<double>> sampled;
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    return sampled;
  }
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return sampled;
  }
  SampleScratch<double> scratch;
  for (const KeptSlot &slot : kept->Slots) {
    const auto read = [&slot, &world](uint32_t i) {
      return readKeptEntry<F>(slot.Entries[i], world.Tick);
    };
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      if constexpr (HasNodeRule<F>) {
        sampleKeptSlotAtNode<F>(network, slot, node->Node, scratch, sampled, read);
      } else {
        sampleSlotAtNode(network, slot, node->Node, scratch, sampled, read);
      }
    } else {
      sampleSlotInEdge<F>(network, slot, place, std::get<EdgePosition>(*position), scratch,
                          sampled, read);
    }
  }
  return sampled;
```

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe`
Expected: all tpj_sim_tests pass.

### Task 9: Confirm the criteria

Step 1 (criteria 1 to 10): Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: no diagnostics, `100% tests passed`, `tidy: clean.` When tidy names an include to add or remove in a file this feature touched, make that change and run it again.

Step 2 (criterion 10): Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/kept-entries/after.txt; build/windows-debug/tpj_scenarios.exe --compare "$(wslpath -w build/kept-entries/before.txt)" "$(wslpath -w build/kept-entries/after.txt)"; echo status $?`
Expected: `status 0`.

### Task 10: Commit

Stage the feature's paths only: `git add src/sim/medium/kept_field.h src/sim/medium/field.h src/sim/medium/network.h src/sim/medium/network.cpp src/sim/world.h src/sim/medium/SPEC.md src/sim/SPEC.md tests/sim/medium/kept_field_test.cpp tests/sim/medium/network_test.cpp tests/sim/CMakeLists.txt`. Review per implementing-features, and commit once via commit-hygiene with the subject `Medium: Keep field entries that only movers change`, using `git commit -- <those paths>` so no other session's staged files are swept in.
