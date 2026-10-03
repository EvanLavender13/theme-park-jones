# Implementation Plan: Place-Indexed Entries

## Goal

Make a field sample, and sourceEntryAtNode, find each source's entries by binary search in an index of the slot's entries ordered by place, reusing the sample's working lists across sources, with every result and order unchanged.

## Approach

FieldSlot gains a mutable, unvisited member holding its entries' positions ordered by carrier, distance, and position, made by entriesByPlace on the slot's first sample. The node and edge sampling helpers in field.h gather matching positions with equal ranges and a carrier range into a scratch list the sample owns, then sort and deduplicate them so entries come out in the source's order; sourceEntryAtNode takes the least matching position directly. tpj_scenarios' windows-debug output before and after shows nothing changed, and the runtime report before and after shows the speedup.

## Placement

Decision 0027 places each behavior this feature adds:

- The order of a slot's entries by place, and the slot keeping it: shared medium, FieldSlot, orderByPlace, and entriesByPlace in src/sim/medium/field.h. The slot owns its entries, so it is the one owner of their derived order, and every way a slot is made, moved, or copied carries the order with it.
- Finding a slot's entries at a place and inside an edge through the order, and the scratch a sample reuses: shared medium, the sampling helpers and sourceEntryAtNode in src/sim/medium/field.h, which already own the sampling rules and are their only callers.

## Tasks

### Task 1: Record the report and tpj_scenarios' output before the change

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && mkdir -p build/place-index && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/place-index/before.txt && wc -l build/place-index/before.txt`
Expected: the link or no-work line, then a line count greater than 0.

Step 2: Run: `scripts/runtime-report.sh build/runtime-report/place-index-before.txt > build/place-index/report-before.log 2>&1; grep "full.park" build/place-index/report-before.log | tail -8`
Expected: the full park's windows-release and app lines, food-overlay near 103,000 µs.

### Task 2: Spec

Files:
- Modify: `src/sim/medium/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/medium/field.h`, `src/sim/routes/route_distance.h`

Step 1: Make each change under Spec changes in plans/scalable-runtime/indexed-sampling/place-indexed-entries/FEATURE.md, verbatim.

Step 2: In src/sim/medium/field.h, end sourceEntryAtNode's comment with `Allocates nothing but the slot's order by place, when it is the slot's first reader.` in place of `Allocates nothing.`, and in src/sim/routes/route_distance.h end routeEntryAt's comment with `allocates nothing but a slot's order by place, when it is the slot's first reader.` in place of `allocates nothing.` (wrapped at 100 columns).

### Task 3: Interface

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Add `#include <cmath>` after `#include <algorithm>`, and `#include <iterator>` after `#include <concepts>`.

Step 2: Replace FieldSlot with:

```cpp
// One source's entries in a field, in the order it published them.
template <typename Entry> struct FieldSlot {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<Entry>> Entries;
  // The positions of Entries ordered by place, made on the slot's first sample. Derived from
  // Entries, which nothing changes in place, and never visited, so saves, hashes, and comparisons
  // never see it.
  mutable std::optional<std::vector<uint32_t>> ByPlace = std::nullopt;
};
```

In publishResolved and publishStepped, replace each `slots.insert(at, Slot{source, std::move(entries)});` by `slots.insert(at, Slot{.Source = source, .Entries = std::move(entries)});`. GCC's -Wmissing-field-initializers, an error under -Werror, fires on a positional initializer that leaves ByPlace out, but not on a designated one.

Step 3: After the visitFields for FieldSlot, add:

```cpp
// Whether a's place orders before b's: by carrier key, then distance.
inline bool placeBefore(const Place &a, const Place &b) {
  return a.Carrier != b.Carrier ? a.Carrier < b.Carrier : a.Distance < b.Distance;
}

// The positions of the entries whose distance is not NaN, each once, ordered by carrier key, then
// distance, then position.
template <typename Entry>
std::vector<uint32_t> orderByPlace(std::span<const PlacedEntry<Entry>> /*entries*/) {
  return {};
}

// The slot's entries ordered by place, as orderByPlace gives them, made on the first call and kept
// in the slot.
template <typename Entry> std::span<const uint32_t> entriesByPlace(const FieldSlot<Entry> &slot) {
  static_cast<void>(slot);
  return {};
}
```

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec"`
Expected: the link line of tpj_sim_tests.exe, no warnings.

### Task 4: Test pass

Dispatch the test-writer for criteria 1 to 4 against src/sim/medium/field.h, src/sim/medium/SPEC.md, and docs/principles.md, in a new tests/sim/medium/field_index_test.cpp registered in tests/sim/CMakeLists.txt. Criteria 5 and 6 are checks.

### Task 5: orderByPlace and entriesByPlace

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace the two stubs with:

```cpp
template <typename Entry>
std::vector<uint32_t> orderByPlace(std::span<const PlacedEntry<Entry>> entries) {
  std::vector<uint32_t> order;
  order.reserve(entries.size());
  for (uint32_t i = 0; i < entries.size(); ++i) {
    // A NaN distance equals no place and lies inside no edge, and would break the ordering.
    if (!std::isnan(entries[i].At.Distance)) {
      order.push_back(i);
    }
  }
  std::ranges::stable_sort(order, [entries](uint32_t a, uint32_t b) {
    return placeBefore(entries[a].At, entries[b].At);
  });
  return order;
}

template <typename Entry> std::span<const uint32_t> entriesByPlace(const FieldSlot<Entry> &slot) {
  if (!slot.ByPlace) {
    slot.ByPlace = orderByPlace(std::span<const PlacedEntry<Entry>>(slot.Entries));
  }
  return *slot.ByPlace;
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#field_index_test]"`
Expected: the test pass's orderByPlace and entriesByPlace tests pass; the sampling ones pass already, since sampling still scans.

### Task 6: Finding positions through the order

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Before sampleSlotAtNode, add:

```cpp
// The positions of the slot's entries at exactly the place, ascending.
template <typename Entry>
std::span<const uint32_t> positionsAt(const FieldSlot<Entry> &slot, const Place &place) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto found = std::ranges::equal_range(order, place, placeBefore, placeOf);
  return {found.begin(), found.end()};
}

// Appends the positions of the slot's entries on the carrier strictly between the two distances.
template <typename Entry>
void positionsInside(const FieldSlot<Entry> &slot, EntityKey carrier, double from, double to,
                     std::vector<uint32_t> &positions) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto first = std::ranges::upper_bound(order, Place{carrier, from}, placeBefore, placeOf);
  const auto last = std::ranges::lower_bound(order, Place{carrier, to}, placeBefore, placeOf);
  if (first < last) {
    positions.insert(positions.end(), first, last);
  }
}

// Clears positions and fills it with those of the slot's entries at any of the places, ascending
// and once each, so entries come out in the source's order.
template <typename Entry>
void positionsAtAny(const FieldSlot<Entry> &slot, std::span<const Place> places,
                    std::vector<uint32_t> &positions) {
  positions.clear();
  for (const Place &place : places) {
    const std::span<const uint32_t> found = positionsAt(slot, place);
    positions.insert(positions.end(), found.begin(), found.end());
  }
  std::ranges::sort(positions);
  const auto repeats = std::ranges::unique(positions);
  positions.erase(repeats.begin(), repeats.end());
}

// The working lists a sample reuses for each of its sources, so its sources' lookups allocate only
// while the lists grow.
template <typename Entry> struct SampleScratch {
  std::vector<uint32_t> Positions;
  std::vector<uint32_t> Inside;
  EdgeSample<Entry> Edge;
};
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec"`
Expected: the link line, no warnings.

### Task 7: Sampling at a node through the order

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace sampleSlotAtNode with:

```cpp
// Appends the source's entries whose places resolve to the node: exactly those at its stop
// places, found through the slot's order by place.
template <typename Entry>
void sampleSlotAtNode(const Network &network, const FieldSlot<Entry> &slot, uint32_t node,
                      SampleScratch<Entry> &scratch, std::vector<SampledEntry<Entry>> &sampled) {
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sampled.push_back({slot.Source, slot.Entries[i].Value});
  }
}
```

Step 2: In sampleSlots, declare `SampleScratch<typename F::Entry> scratch;` before the loop over slots, and pass `scratch` to sampleSlotAtNode after its node argument.

Step 3: In sourceEntryAtNode, replace the loop over slot->Entries and the `return std::nullopt;` after it with:

```cpp
  // Each place's positions ascend, so the least first position among the stops is the source's
  // first entry at the node.
  std::optional<uint32_t> first;
  for (const Place &stop : stops) {
    const std::span<const uint32_t> found = positionsAt(*slot, stop);
    if (!found.empty() && (!first || found.front() < *first)) {
      first = found.front();
    }
  }
  if (!first) {
    return std::nullopt;
  }
  return slot->Entries[*first].Value;
```

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#field_test],[#field_sampling_test],[#field_index_test],[#stepped_field_test],[#source_sample_test]"`
Expected: all pass.

### Task 8: Sampling inside an edge through the order

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Give sampleSlotInEdge a parameter `SampleScratch<typename F::Entry> &scratch` before `sampled`, and pass `scratch` to it in sampleSlots.

Step 2: In its HasEdgeRule branch, replace `EdgeSample<Entry> sample;` by `EdgeSample<Entry> &sample = scratch.Edge;` followed by `sample.AtFrom.clear(); sample.AtTo.clear(); sample.Along.clear();` on separate lines, and replace the loop over slot.Entries and its comment with:

```cpp
    // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
    // between its stops, and to one of its nodes exactly when it is at one of that node's stop
    // places, so no entry is resolved. The offsets are resolve's own subtractions.
    std::vector<uint32_t> &inside = scratch.Inside;
    inside.clear();
    positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
    std::ranges::sort(inside);
    for (const uint32_t i : inside) {
      const PlacedEntry<Entry> &entry = slot.Entries[i];
      sample.Along.push_back({entry.At.Distance - edge.FromDistance,
                              edge.ToDistance - entry.At.Distance, entry.Value});
    }
    // An entry inside the edge is not also at an end, as resolving it gives the inside.
    positionsAtAny(slot, fromStops, scratch.Positions);
    for (const uint32_t i : scratch.Positions) {
      if (!std::ranges::binary_search(inside, i)) {
        sample.AtFrom.push_back(slot.Entries[i].Value);
      }
    }
    positionsAtAny(slot, toStops, scratch.Positions);
    for (const uint32_t i : scratch.Positions) {
      if (!std::ranges::binary_search(inside, i)) {
        sample.AtTo.push_back(slot.Entries[i].Value);
      }
    }
```

Positions inside the edge are distinct, so sorting them is enough to put them in the source's order.

Step 3: In the branch without an edge rule, replace the loop with:

```cpp
    for (const uint32_t i : positionsAt(slot, place)) {
      sampled.push_back({slot.Source, slot.Entries[i].Value});
    }
```

and mark the scratch parameter `[[maybe_unused]]`.

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe`
Expected: all tpj_sim_tests pass.

### Task 9: Confirm the criteria

Step 1 (criteria 1 to 5): Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: no diagnostics, `100% tests passed`, `tidy: clean.`

Step 2 (criterion 5): Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/place-index/after.txt; build/windows-debug/tpj_scenarios.exe --compare "$(wslpath -w build/place-index/before.txt)" "$(wslpath -w build/place-index/after.txt)"; echo status $?`
Expected: `status 0`.

Step 3 (criterion 6): Run: `scripts/runtime-report.sh build/runtime-report/place-index-after.txt > build/place-index/report-after.log 2>&1; scripts/runtime-report.sh --compare build/runtime-report/place-index-before.txt build/runtime-report/place-index-after.txt | grep full.park`
Expected: the windows-release full.park food-overlay line says faster, and its ticks line does not say slower. Put the full park's lines in the feature's report.

### Task 10: Commit

Stage the feature's paths only: `git add src/sim/medium/field.h src/sim/medium/SPEC.md src/sim/routes/SPEC.md src/sim/routes/route_distance.h tests/sim/medium/field_index_test.cpp tests/sim/CMakeLists.txt`. Review per implementing-features, and commit once via commit-hygiene with the subject `Medium: Find sampled entries through an index by place`, using `git commit -- <those paths>` so no other session's staged work is included.
