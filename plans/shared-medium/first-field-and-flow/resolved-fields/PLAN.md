# Implementation Plan: Resolved Fields

## Goal

Add the medium's fields in src/sim/medium/field.h: field definitions, addField, publishResolved, sampleField with the default and owner rules, and fieldValue, with World::isResolving to guard publication.

## Approach

Everything is templates over the field's definition type, so field.h is header-only. addField registers ResolvedEntries<F> as a derived component and a resolver, emptyResolvedEntries<F>, that creates the field's derived-key entity and replaces its component with an empty one. Because resolvers run in registration order, it runs before every producer registered after the field. Slots are kept sorted by source key with a lower_bound insert. sampleField resolves the place once and each entry's place as it scans, and calls the owner's rule through `if constexpr` when the definition has one, so no function pointer is stored anywhere the walk would see.

## Tasks

### Task 1: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: In the first paragraph, replace "the networks every field is sampled on" with "the networks, and the fields sampled on them". At the end of the file, add the Fields section with the exact text in FEATURE.md's Spec changes, headed `## Fields`.

### Task 2: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: After the sentence "Resolvers never draw from the key counter: in debug builds, createEntity during resolution throws WorldInvariantError.", add, in the same paragraph:

"isResolving is true only while the resolvers run, so functions meant only for resolvers can refuse other callers."

### Task 3: Expose whether the world is resolving

Files:
- Modify: `src/sim/world.h`

Step 1: After isResolvePending's declaration, add:

```cpp
  // True only while the resolvers run.
  [[nodiscard]] bool isResolving() const { return Resolving; }
```

### Task 4: Declare fields with stubs

Files:
- Create: `src/sim/medium/field.h`

Step 1: Write the header. The five function templates at the end have stub bodies, replaced in Tasks 6 to 9.

```cpp
#ifndef TPJ_SIM_MEDIUM_FIELD_H
#define TPJ_SIM_MEDIUM_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

// Whether a field's entries are only listed, or also summed into a value.
enum class FieldKind { Entry, Scalar };

// An entry published at a place.
template <typename Entry> struct PlacedEntry {
  Place At;
  Entry Value{};
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, PlacedEntry<Entry> &entry) {
  visitor.field("at", entry.At);
  visitor.field("value", entry.Value);
}

// One source's entries in a field, in the order it published them.
template <typename Entry> struct FieldSlot {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<Entry>> Entries;
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, FieldSlot<Entry> &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
}

// An entry sampled at a place, with the source that published it.
template <typename Entry> struct SampledEntry {
  EntityKey Source = NULL_KEY;
  Entry Value{};
};

// An entry strictly inside the sampled edge, with its distances along the carrier to the edge's
// two ends.
template <typename Entry> struct EdgeEntry {
  double FromOffset = 0.0;
  double ToOffset = 0.0;
  Entry Value{};
};

// What an owner's rule is given for one source at a place strictly inside an edge.
template <typename Entry> struct EdgeSample {
  NetworkEdge Edge;
  double FromOffset = 0.0;
  double ToOffset = 0.0;
  std::vector<Entry> AtFrom;
  std::vector<Entry> AtTo;
  std::vector<EdgeEntry<Entry>> Along;
};

// A field's definition, a type its owning module supplies:
//   struct HungryFootfall {
//     using Entry = double;
//     static constexpr std::string_view Name = "hungry-footfall";
//     static constexpr FieldKind Kind = FieldKind::Scalar;
//   };
template <typename F>
concept FieldDefinition = requires {
  typename F::Entry;
  { F::Name } -> std::convertible_to<std::string_view>;
  { F::Kind } -> std::convertible_to<FieldKind>;
};

// A field whose owner reads places inside an edge by its own rule:
//   static std::vector<Entry> sampleEdge(const EdgeSample<Entry> &sample);
template <typename F>
concept HasEdgeRule = FieldDefinition<F> && requires(const EdgeSample<typename F::Entry> &sample) {
  { F::sampleEdge(sample) } -> std::same_as<std::vector<typename F::Entry>>;
};

inline constexpr uint64_t FIELD_PURPOSE = hashName("field");

// The key of the entity holding the named field's entries.
constexpr EntityKey fieldKey(std::string_view name) {
  return deriveKey(NULL_KEY, FIELD_PURPOSE, hashName(name));
}

// A field's resolved entries, sources in ascending key order. Derived data.
template <FieldDefinition F> struct ResolvedEntries {
  std::vector<FieldSlot<typename F::Entry>> Slots;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, ResolvedEntries<F> &resolved) {
  visitor.field("slots", resolved.Slots);
}

// The field's resolver: empties its resolved entries, creating the entity that holds them.
template <FieldDefinition F> void emptyResolvedEntries(World & /*world*/) {
  // Stub until implemented.
}

// Registers the field's derived component, <name>-resolved, and its resolver, <name>-field.
// Throws std::invalid_argument when either name is malformed or already registered.
template <FieldDefinition F> void addField(WorldSchema & /*schema*/) {
  // Stub until implemented.
}

// Gives a source's entries for this resolution. Resolvers only. Throws std::logic_error when the
// world is not resolving or holds no resolved entries for the field, and std::invalid_argument for
// the null key or a source that has already published into the field in this resolution.
template <FieldDefinition F>
void publishResolved(World & /*world*/, EntityKey /*source*/,
                     std::vector<PlacedEntry<typename F::Entry>> /*entries*/) {
  // Stub until implemented.
}

// The field's entries at the place on the network, each with its source, sources in ascending key
// order, by the default rule or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleField(const World & /*world*/, const Network & /*network*/, const Place & /*place*/) {
  return {}; // Stub until implemented.
}

// A scalar field's value at the place: 0.0 with each sampled entry added in turn.
template <FieldDefinition F>
  requires(F::Kind == FieldKind::Scalar)
double fieldValue(const World & /*world*/, const Network & /*network*/, const Place & /*place*/) {
  return 0.0; // Stub until implemented.
}

} // namespace tpj

#endif
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 147 existing tests pass. No source includes field.h yet, so the header is compiled only once the tests include it.

### Task 5: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, and the public headers src/sim/medium/field.h, src/sim/medium/network.h, and src/sim/world.h. It creates tests/sim/medium/field_test.cpp and the synthetic fields and network producer it needs in tests/sim/support/, and adds the test file to tpj_sim_tests in tests/sim/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 147 existing tests pass. The new tests of registration, publication, replacement, source order, both sampling rules, scalar values, and candidates fail against the stubs. Tests that hold vacuously may pass, such as "no entries" and a value of 0.0 where nothing is published.

### Task 6: Register fields

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace the stubs of emptyResolvedEntries and addField:

```cpp
template <FieldDefinition F> void emptyResolvedEntries(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  world.Registry.emplace_or_replace<ResolvedEntries<F>>(world.findEntity(key));
}

template <FieldDefinition F> void addField(WorldSchema &schema) {
  static_assert(F::Kind != FieldKind::Scalar || std::is_same_v<typename F::Entry, double>,
                "a scalar field's entries are doubles");
  const std::string name(F::Name);
  schema.addComponent<ResolvedEntries<F>>(name + "-resolved", DataKind::Derived);
  schema.addResolver(name + "-field", &emptyResolvedEntries<F>);
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the registration tests pass.

### Task 7: Publish resolved entries

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace publishResolved's stub. The checks run in the order FEATURE.md lists them, and the lower_bound insert keeps slots in ascending source order.

```cpp
template <FieldDefinition F>
void publishResolved(World &world, EntityKey source,
                     std::vector<PlacedEntry<typename F::Entry>> entries) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::string name(F::Name);
  if (!world.isResolving()) {
    throw std::logic_error("field " + name + " takes resolved entries only from resolvers");
  }
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr) {
    throw std::logic_error("field " + name +
                           " holds no resolved entries; register it before its publishers");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  std::vector<Slot> &slots = resolved->Slots;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + name);
  }
  slots.insert(at, Slot{source, std::move(entries)});
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and the publication tests that do not sample pass: the refusals, and entries never appearing in a save.

### Task 8: Sample at nodes and inside edges

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Before sampleField, add two helpers:

```cpp
// Appends the source's entries whose places resolve to the node.
template <typename Entry>
void sampleSlotAtNode(const Network &network, const FieldSlot<Entry> &slot, uint32_t node,
                      std::vector<SampledEntry<Entry>> &sampled) {
  for (const PlacedEntry<Entry> &entry : slot.Entries) {
    const std::optional<NetworkPosition> at = network.resolve(entry.At);
    const auto *atNode = at ? std::get_if<NodePosition>(&*at) : nullptr;
    if (atNode != nullptr && atNode->Node == node) {
      sampled.push_back({slot.Source, entry.Value});
    }
  }
}

// Appends the source's entries at a place strictly inside an edge: those at the same place, or
// what the field's own rule gives when it has one.
template <FieldDefinition F>
void sampleSlotInEdge(const Network &network, const FieldSlot<typename F::Entry> &slot,
                      [[maybe_unused]] const Place &place,
                      [[maybe_unused]] const EdgePosition &position,
                      std::vector<SampledEntry<typename F::Entry>> &sampled) {
  using Entry = typename F::Entry;
  if constexpr (HasEdgeRule<F>) {
    EdgeSample<Entry> sample;
    sample.Edge = network.edges()[position.Edge];
    sample.FromOffset = position.FromOffset;
    sample.ToOffset = position.ToOffset;
    for (const PlacedEntry<Entry> &entry : slot.Entries) {
      const std::optional<NetworkPosition> at = network.resolve(entry.At);
      if (!at) {
        continue;
      }
      if (const auto *atNode = std::get_if<NodePosition>(&*at)) {
        if (atNode->Node == sample.Edge.From) {
          sample.AtFrom.push_back(entry.Value);
        }
        if (atNode->Node == sample.Edge.To) {
          sample.AtTo.push_back(entry.Value);
        }
      } else {
        const EdgePosition &inEdge = std::get<EdgePosition>(*at);
        if (inEdge.Edge == position.Edge) {
          sample.Along.push_back({inEdge.FromOffset, inEdge.ToOffset, entry.Value});
        }
      }
    }
    if (sample.AtFrom.empty() && sample.AtTo.empty() && sample.Along.empty()) {
      return;
    }
    for (Entry &value : F::sampleEdge(sample)) {
      sampled.push_back({slot.Source, std::move(value)});
    }
  } else {
    for (const PlacedEntry<Entry> &entry : slot.Entries) {
      if (entry.At == place) {
        sampled.push_back({slot.Source, entry.Value});
      }
    }
  }
}
```

Step 2: Replace sampleField's stub:

```cpp
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>> sampleField(const World &world,
                                                         const Network &network,
                                                         const Place &place) {
  std::vector<SampledEntry<typename F::Entry>> sampled;
  const std::optional<NetworkPosition> position = network.resolve(place);
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  if (!position || entity == entt::null) {
    return sampled;
  }
  const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr) {
    return sampled;
  }
  for (const FieldSlot<typename F::Entry> &slot : resolved->Slots) {
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      sampleSlotAtNode(network, slot, node->Node, sampled);
    } else {
      sampleSlotInEdge<F>(network, slot, place, std::get<EdgePosition>(*position), sampled);
    }
  }
  return sampled;
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and every test passes except those of scalar values.

### Task 9: Scalar values

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace fieldValue's stub:

```cpp
template <FieldDefinition F>
  requires(F::Kind == FieldKind::Scalar)
double fieldValue(const World &world, const Network &network, const Place &place) {
  double value = 0.0;
  for (const SampledEntry<double> &entry : sampleField<F>(world, network, place)) {
    value += entry.Value;
  }
  return value;
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and every test passes.

### Task 10: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: the script prints its stages and passes, with the scenarios' output unchanged, since no scenario has a field yet.

### Task 11: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Medium: Add resolved fields with per-source sampling`.
