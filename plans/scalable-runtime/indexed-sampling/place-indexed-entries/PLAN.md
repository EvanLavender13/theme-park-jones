# Implementation Plan: Place-Indexed Entries

## Goal

Make a field sample find each source's entries by binary search in an index of the slot's entries ordered by place, with every result and order unchanged.

## Approach

FieldSlot gains a mutable, unvisited member holding its entries' positions ordered by carrier, distance, and position, made by entriesByPlace on the slot's first sample. The node and edge sampling helpers in field.h gather matching positions with equal ranges and a carrier range, then sort and deduplicate them so entries come out in the source's order. tpj_scenarios' windows-debug output before and after shows nothing changed, and the runtime report against the milestone's before report shows the speedup.

## Placement

Decision 0027 places each behavior this feature adds:

- The order of a slot's entries by place, and the slot keeping it: shared medium, FieldSlot, orderByPlace, and entriesByPlace in src/sim/medium/field.h. The slot owns its entries, so it is the one owner of their derived order, and every way a slot is made, moved, or copied carries the order with it.
- Finding a slot's entries at a place and inside an edge through the order: shared medium, the sampling helpers in src/sim/medium/field.h, which already own the sampling rules and are their only callers.

## Tasks

### Task 1: Record tpj_scenarios' output before the change

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && mkdir -p build/place-index && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/place-index/before.txt && wc -l build/place-index/before.txt`
Expected: the link or no-work line, then a line count greater than 0.

### Task 2: Spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: Replace the paragraph beginning "A sample resolves only its own place." with the paragraph under Spec changes in plans/scalable-runtime/indexed-sampling/place-indexed-entries/FEATURE.md, verbatim.

### Task 3: Interface

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Add `#include <cmath>` after `#include <algorithm>`.

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

In publishResolved and publishStepped, replace each `slots.insert(at, Slot{source, std::move(entries)});` by `slots.insert(at, Slot{.Source = source, .Entries = std::move(entries)});`. GCC's -Wmissing-field-initializers, an error under -Werror, fires on a positional initializer that leaves ByPlace out, but not on a designated one, and clang skips members with a default initializer, as tests' designated FieldSlot initializers rely on.

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
// Appends the positions of the slot's entries at exactly the place.
template <typename Entry>
void positionsAt(const FieldSlot<Entry> &slot, const Place &place,
                 std::vector<uint32_t> &positions) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto found = std::ranges::equal_range(order, place, placeBefore, placeOf);
  positions.insert(positions.end(), found.begin(), found.end());
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

// Sorts positions and drops repeats, so entries come out in the source's order, once each.
inline void inSourceOrder(std::vector<uint32_t> &positions) {
  std::ranges::sort(positions);
  const auto repeats = std::ranges::unique(positions);
  positions.erase(repeats.begin(), repeats.end());
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec"`
Expected: the link line, no warnings.

### Task 7: Sampling at a node through the order

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Replace sampleSlotAtNode's body with:

```cpp
  std::vector<uint32_t> positions;
  for (const Place &stop : network.stopPlaces(node)) {
    positionsAt(slot, stop, positions);
  }
  inSourceOrder(positions);
  for (const uint32_t i : positions) {
    sampled.push_back({slot.Source, slot.Entries[i].Value});
  }
```

and its comment by `// Appends the source's entries whose places resolve to the node: exactly those at its stop places, found through the slot's order by place.` (wrapped at 100 columns).

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe -# "[#field_test],[#field_sampling_test],[#field_index_test],[#stepped_field_test]"`
Expected: all pass.

### Task 8: Sampling inside an edge through the order

Files:
- Modify: `src/sim/medium/field.h`

Step 1: In sampleSlotInEdge's HasEdgeRule branch, replace the loop over slot.Entries and its comment with:

```cpp
    // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
    // between its stops, and to one of its nodes exactly when it is at one of that node's stop
    // places, so no entry is resolved. The offsets are resolve's own subtractions.
    std::vector<uint32_t> along;
    positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, along);
    inSourceOrder(along);
    const auto atEnd = [&slot, &along](std::span<const Place> stops) {
      std::vector<uint32_t> positions;
      for (const Place &stop : stops) {
        positionsAt(slot, stop, positions);
      }
      inSourceOrder(positions);
      // An entry inside the edge is not also at an end, as the scan skipped it.
      std::vector<uint32_t> outside;
      std::ranges::set_difference(positions, along, std::back_inserter(outside));
      return outside;
    };
    for (const uint32_t i : along) {
      const PlacedEntry<Entry> &entry = slot.Entries[i];
      sample.Along.push_back({entry.At.Distance - edge.FromDistance,
                              edge.ToDistance - entry.At.Distance, entry.Value});
    }
    for (const uint32_t i : atEnd(fromStops)) {
      sample.AtFrom.push_back(slot.Entries[i].Value);
    }
    for (const uint32_t i : atEnd(toStops)) {
      sample.AtTo.push_back(slot.Entries[i].Value);
    }
```

Add `#include <iterator>` to the includes.

Step 2: In the branch without an edge rule, replace the loop with:

```cpp
    std::vector<uint32_t> positions;
    positionsAt(slot, place, positions);
    inSourceOrder(positions);
    for (const uint32_t i : positions) {
      sampled.push_back({slot.Source, slot.Entries[i].Value});
    }
```

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | grep -E "warning|error|Linking CXX exec" && build/windows-debug/tpj_sim_tests.exe`
Expected: all tpj_sim_tests pass.

### Task 9: Confirm the criteria

Step 1 (criteria 1 to 5): Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: no diagnostics, `100% tests passed`, `tidy: clean.`

Step 2 (criterion 5): Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>/dev/null | tr -d '\r' > build/place-index/after.txt; build/windows-debug/tpj_scenarios.exe --compare "$(wslpath -w build/place-index/before.txt)" "$(wslpath -w build/place-index/after.txt)"; echo status $?`
Expected: `status 0`.

Step 3 (criterion 6): Run: `scripts/runtime-report.sh build/runtime-report/indexed-sampling-after.txt` and then `scripts/runtime-report.sh --compare build/runtime-report/indexed-sampling-before.txt build/runtime-report/indexed-sampling-after.txt`.
Expected: the show output's windows-release full.park ticks line has a median below 33333.3 and no over-budget mark; the comparison's windows-release full.park ticks line says faster. Put both outputs in the feature's report, with the full park's app frames and food overlay before and after.

### Task 10: Commit

Stage the feature's paths only, `git add src/sim/medium/field.h src/sim/medium/SPEC.md tests/sim/medium/field_index_test.cpp tests/sim/CMakeLists.txt`, since the tree holds unrelated untracked files; review per implementing-features, and commit once via commit-hygiene with the subject `Medium: Find sampled entries through an index by place`.
