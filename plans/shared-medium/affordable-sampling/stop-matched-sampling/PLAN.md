# Implementation Plan: Stop-Matched Sampling

## Goal

Field sampling finds each entry's position by comparing places with the network's stop places and the sampled edge's bounds, so it no longer resolves every entry, and gives exactly what it gave before.

## Approach

The Network constructor fills each node's stop places in its existing pass over carriers and stops, held flat with per-node starts, and stopPlaces returns a std::span over a node's run; nodePlace becomes its first element. sampleSlotAtNode and sampleSlotInEdge in field.h replace their per-entry resolve with those comparisons. Before any code changes, the baseline times, perf share, final hashes, and cross-build output are recorded under build/perf/ so the after figures compare against the same machine.

## Tasks

### Task 1: Record the baseline

Files: none in the repository. Outputs go to build/perf/, which is ignored.

Step 1: Build the app on both presets.

Run: `cmake.exe --build --preset windows-debug --target tpj_app 2>&1 | tail -1 && cmake --build --preset linux-debug --target tpj_app 2>&1 | tail -1`
Expected: both builds finish without errors.

Step 2: Time 3000 ticks of supply.park on each build.

Run: `mkdir -p build/perf && time build/windows-debug/ThemeParkJones.exe --park tests/parks/supply.park --ticks 3000 --hash && time build/linux-debug/ThemeParkJones --park tests/parks/supply.park --ticks 3000 --hash`
Expected: each prints `tick 3000 hash <16 hex digits>`, the same hash on both, and a real time near 2.6 s on Windows and 8.4 s on Linux. Write the two hashes and real times into build/perf/before.txt.

Step 3: Profile the Linux run.

Run: `P=/usr/lib/linux-tools/6.8.0-142-generic/perf; $P record -q -F 999 -g -o build/perf/before.data build/linux-debug/ThemeParkJones --park tests/parks/supply.park --ticks 3000 --hash && $P report -i build/perf/before.data --children --sort symbol --stdio -g none 2>/dev/null | grep -E "Network::resolve|sampleSlotInEdge|stepGuests" | head -5`
Expected: Network::resolve's Children share near 50%. Append the lines to build/perf/before.txt.

Step 4: Record the cross-build output.

Run: `scripts/cross-build-check.sh 2>&1 | tail -1 && cp build/cross-build-check/linux-debug.txt build/perf/cross-build-before.txt`
Expected: `cross-build-check: both builds wrote the same 27340 lines; passed.`

### Task 2: Medium spec

Files:
- Modify: `src/sim/medium/SPEC.md` (Networks, after the nodePlace paragraph; Fields, after the sampleField paragraph)

Step 1: After the paragraph that begins `nodePlace gives a node's place:`, add this paragraph:

```
stopPlaces gives a node's stop places: for each stop at the node, its carrier's key and the stop's distance, in ascending carrier key and then distance. A place resolves to a node only at exactly the distance of one of its stops, so a node's stop places are exactly the places resolve gives it for, and nodePlace is the first of them. The network builds them once, in its constructor. stopPlaces throws std::out_of_range for a node not below the node count.
```

Step 2: After the paragraph that begins `sampleField gives a field's entries at a place on a network,` and before the paragraph that begins `fieldValue gives`, add this paragraph:

```
A sample resolves only its own place. It finds a source's entries at a node by comparing their places with the node's stop places, and, for a field with sampleEdge, its entries strictly inside an edge by comparing their carrier with the edge's and their distance with the edge's two stop distances, strictly. Those comparisons give the same entries, offsets, and order as resolving each entry, so a sample costs a scan of its sources' entries and no resolve per entry.
```

### Task 3: Stop places interface

Files:
- Modify: `src/sim/medium/network.h` (includes; Network's public queries after nodePlace)
- Modify: `src/sim/medium/network.cpp` (after Network::nodePlace)

Step 1: In network.h, add `#include <span>` between `#include <optional>` and `#include <stddef.h>`.

Step 2: In network.h, after the declaration of nodePlace, add:

```cpp
  // The place of each stop at the node, in ascending carrier key and then distance: exactly the
  // places resolve gives the node for. Throws std::out_of_range as nodePlace does.
  [[nodiscard]] std::span<const Place> stopPlaces(uint32_t node) const;
```

Step 3: In network.cpp, after Network::nodePlace, add the stub:

```cpp
std::span<const Place> Network::stopPlaces(uint32_t /*node*/) const { return {}; }
```

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -1`
Expected: the build finishes without warnings.

### Task 4: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the spec src/sim/medium/SPEC.md, and the headers src/sim/medium/network.h and src/sim/medium/field.h.

### Task 5: Build the stop places

Files:
- Modify: `src/sim/medium/network.h` (the visitFields comment and the private members)
- Modify: `src/sim/medium/network.cpp` (the constructor's last two loops, nodePlace, and the stopPlaces stub)

Step 1: In network.h, in the comment above visitFields, replace `The edges and node places are rebuilt` with `The edges and stop places are rebuilt`.

Step 2: In network.h, replace

```cpp
  std::vector<Place> NodePlaces;
```

with

```cpp
  // Each node's stop places, node by node: node n's run from StopStarts[n] up to StopStarts[n + 1].
  std::vector<Place> StopPlaces;
  std::vector<size_t> StopStarts;
```

Step 3: In network.cpp, replace the constructor's code from `std::vector<std::optional<Place>> places(NodeCount);` to the end of the constructor with:

```cpp
  // Counts each node's stops into the start after it, while building the edges.
  StopStarts.assign(static_cast<size_t>(NodeCount) + 1, 0);
  for (const Carrier &carrier : Carriers) {
    FirstEdges.push_back(static_cast<uint32_t>(Edges.size()));
    for (size_t i = 0; i < carrier.Stops.size(); ++i) {
      const CarrierStop &stop = carrier.Stops[i];
      ++StopStarts[static_cast<size_t>(stop.Node) + 1];
      if (i + 1 < carrier.Stops.size()) {
        const CarrierStop &next = carrier.Stops[i + 1];
        Edges.push_back({carrier.Key, stop.Node, next.Node, stop.Distance, next.Distance});
      }
    }
  }
  for (size_t node = 0; node < NodeCount; ++node) {
    if (StopStarts[node + 1] == 0) {
      throw std::invalid_argument("node " + std::to_string(node) +
                                  " has no carrier stopping at it");
    }
    StopStarts[node + 1] += StopStarts[node];
  }
  // Carriers in key order and stops in distance order leave each node's run in that order.
  StopPlaces.resize(StopStarts.back());
  std::vector<size_t> filled(StopStarts.begin(), StopStarts.end() - 1);
  for (const Carrier &carrier : Carriers) {
    for (const CarrierStop &stop : carrier.Stops) {
      StopPlaces[filled[stop.Node]] = Place{carrier.Key, stop.Distance};
      ++filled[stop.Node];
    }
  }
}
```

Step 4: In network.cpp, replace

```cpp
Place Network::nodePlace(uint32_t node) const { return NodePlaces.at(node); }
```

with

```cpp
Place Network::nodePlace(uint32_t node) const { return stopPlaces(node).front(); }
```

and replace the stopPlaces stub with:

```cpp
std::span<const Place> Network::stopPlaces(uint32_t node) const {
  if (node >= NodeCount) {
    throw std::out_of_range("node " + std::to_string(node) + " is not below the node count");
  }
  return std::span<const Place>(StopPlaces)
      .subspan(StopStarts[node], StopStarts[node + 1] - StopStarts[node]);
}
```

Step 5: Build and run the network tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -1 && build/windows-debug/tpj_sim_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: the build has no warnings. The test pass's tests of criteria 1 and 2 pass, and every existing test passes, since sampling still resolves entries.

### Task 6: Sample by stop places

Files:
- Modify: `src/sim/medium/field.h` (includes; sampleSlotAtNode; the sampleEdge branch of sampleSlotInEdge)

Step 1: Add `#include <span>` between `#include <optional>` and `#include <stdexcept>`.

Step 2: Replace sampleSlotAtNode, with its comment, by:

```cpp
// Appends the source's entries whose places resolve to the node: exactly those among its stop
// places.
template <typename Entry>
void sampleSlotAtNode(const Network &network, const FieldSlot<Entry> &slot, uint32_t node,
                      std::vector<SampledEntry<Entry>> &sampled) {
  const std::span<const Place> stops = network.stopPlaces(node);
  for (const PlacedEntry<Entry> &entry : slot.Entries) {
    if (std::ranges::find(stops, entry.At) != stops.end()) {
      sampled.push_back({slot.Source, entry.Value});
    }
  }
}
```

Step 3: In sampleSlotInEdge, replace the body of `if constexpr (HasEdgeRule<F>) {`, from `EdgeSample<Entry> sample;` up to and including the loop over slot.Entries, by:

```cpp
    const NetworkEdge &edge = network.edges()[position.Edge];
    const std::span<const Place> fromStops = network.stopPlaces(edge.From);
    const std::span<const Place> toStops = network.stopPlaces(edge.To);
    EdgeSample<Entry> sample;
    sample.Edge = edge;
    sample.FromOffset = position.FromOffset;
    sample.ToOffset = position.ToOffset;
    // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
    // between its stops, and to one of its nodes exactly when it is among that node's stop places,
    // so no entry is resolved. The offsets are resolve's own subtractions.
    for (const PlacedEntry<Entry> &entry : slot.Entries) {
      const Place &at = entry.At;
      if (at.Carrier == edge.Carrier && at.Distance > edge.FromDistance &&
          at.Distance < edge.ToDistance) {
        sample.Along.push_back(
            {at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, entry.Value});
        continue;
      }
      if (std::ranges::find(fromStops, at) != fromStops.end()) {
        sample.AtFrom.push_back(entry.Value);
      }
      if (std::ranges::find(toStops, at) != toStops.end()) {
        sample.AtTo.push_back(entry.Value);
      }
    }
```

Leave the rest of the branch, from `if (sample.AtFrom.empty() && ...` on, and the branch for fields without sampleEdge, unchanged.

Step 4: Build and run the medium, routes, operations, and guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -1 && build/windows-debug/tpj_sim_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: no warnings, and every test passes, including the test pass's test of criterion 3.

### Task 7: Confirm the criteria

Step 1: Format the changed files.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
Expected: no output.

Step 2: Build and test Windows.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -3 && ctest.exe --preset windows-debug 2>&1 | tr -d '\r' | tail -4`
Expected: no warnings, and 100% tests passed.

Step 3: Build and test Linux, and run clang-tidy.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -4; scripts/tidy.sh 2>&1 | tail -3`
Expected: no warning lines, 100% tests passed, and `tidy: clean.`

Step 4: Run the cross-build check and compare with the baseline (criterion 4).

Run: `scripts/cross-build-check.sh 2>&1 | tail -1 && cmp build/cross-build-check/linux-debug.txt build/perf/cross-build-before.txt && echo identical`
Expected: `cross-build-check: both builds wrote the same 27340 lines; passed.` and `identical`.

Step 5: Time and profile the after run (criterion 5).

Run: `time build/windows-debug/ThemeParkJones.exe --park tests/parks/supply.park --ticks 3000 --hash && time build/linux-debug/ThemeParkJones --park tests/parks/supply.park --ticks 3000 --hash && P=/usr/lib/linux-tools/6.8.0-142-generic/perf; $P record -q -F 999 -g -o build/perf/after.data build/linux-debug/ThemeParkJones --park tests/parks/supply.park --ticks 3000 --hash && $P report -i build/perf/after.data --children --sort symbol --stdio -g none 2>/dev/null | grep -E "Network::resolve|sampleSlotInEdge|stepGuests" | head -5`
Expected: both hashes equal those in build/perf/before.txt; the Windows real time is at most two thirds of the before time; Network::resolve's Children share is under 5%. If a criterion misses, stop and report it as a deviation.

### Task 8: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, and present the findings to Evan.

Step 2: Commit once, via the commit-hygiene skill, with the subject `Medium: Sample fields by matching stop places` and a body naming the stop places, the comparisons that replace resolving each entry, and the before and after times and resolve shares.
