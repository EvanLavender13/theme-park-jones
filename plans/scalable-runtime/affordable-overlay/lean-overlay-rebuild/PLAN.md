# Implementation Plan: Lean Overlay Rebuild

## Goal

Remove the allocation and copying from the overlay rebuild's loops, without changing any value, pixel, or simulation result.

## Approach

Edge rules append to a reused list. The batch sampler calls its visitor once per entry, so nothing is collected per place. availabilityValues sums terms per place directly, sharing one term function with availabilityAt so the values stay bit-identical. The band keeps one row list and one distance list for all carriers and reserves its mesh.

## Placement

Decision 0027 places each change:

- Edge rules append to a given list: medium, sim/medium/field.h (the HasEdgeRule contract), with the two rules in their owners, sim/routes/route_distance and sim/guests/footfall. The medium owns the contract, and each owner its rule.
- One entry per visitor call: medium, sim/medium/field.h and sim/medium/kept_field.h, which own sampling.
- Values summed per place without contribution lists: legible, legible/availability.cpp, which owns availability.
- Reused band lists and a reserved mesh: render, render/band_overlay.cpp, which owns the band's geometry.

## Tasks

### Task 1: Check the before artifacts

No code in src, scripts, or tests has changed since batched-field-samples, only SPEC.md prose, so its after artifacts are this feature's before: build/batched-field-samples-after-scenarios.txt, build/batched-field-samples-after.bmp, and build/runtime-report/batched-field-samples-after.txt.

Run: `git diff --stat bf2aabd HEAD -- src scripts tests ':!*.md' | wc -l; ls build/batched-field-samples-after-scenarios.txt build/batched-field-samples-after.bmp build/runtime-report/batched-field-samples-after.txt`
Expected: `0`, then the three paths.

### Task 2: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: Make the change FEATURE.md's Spec changes gives, word for word.

### Task 3: Edge rules append to a given list

Files:
- Modify: `src/sim/medium/field.h`, `src/sim/routes/route_distance.h`, `src/sim/routes/route_distance.cpp`, `src/sim/guests/footfall.h`, `src/sim/guests/footfall.cpp`

Step 1: In field.h, replace the HasEdgeRule comment and concept with:

```cpp
// A field whose owner reads places inside an edge by its own rule, which appends the entries at
// the sample's place to out:
//   static void sampleEdge(const EdgeSample<Entry> &sample, std::vector<Entry> &out);
template <typename F>
concept HasEdgeRule =
    FieldDefinition<F> &&
    requires(const EdgeSample<typename F::Entry> &sample, std::vector<typename F::Entry> &out) {
      { F::sampleEdge(sample, out) } -> std::same_as<void>;
    };
```

Step 2: In field.h, add a member to SampleScratch after `EdgeSample<Entry> Edge;`:

```cpp
  // The entries the field's rule makes at one place.
  std::vector<Entry> Made;
```

Step 3: In finishRun, replace

```cpp
        for (const Entry &value : F::sampleEdge(scratch.Edge)) {
```

with

```cpp
        scratch.Made.clear();
        F::sampleEdge(scratch.Edge, scratch.Made);
        for (const Entry &value : scratch.Made) {
```

Step 4: In route_distance.h, change the sampleRouteEdge declaration and its comment to:

```cpp
// At a place inside an edge, appends one entry for the source to out: the better of the edge's two
// ends, the place's offset to each added to that end's distance, ties to the From end. Entries
// inside the edge are ignored.
void sampleRouteEdge(const EdgeSample<RouteEntry> &sample, std::vector<RouteEntry> &out);
```

and RouteDistance's rule to:

```cpp
  static void sampleEdge(const EdgeSample<RouteEntry> &sample, std::vector<RouteEntry> &out) {
    sampleRouteEdge(sample, out);
  }
```

Step 5: In route_distance.cpp, change sampleRouteEdge's signature to match, and replace its ending

```cpp
  if (!best) {
    return {};
  }
  return {*best};
```

with

```cpp
  if (best) {
    out.push_back(*best);
  }
```

Step 6: In footfall.h, change the sampleEdge comment and declaration to:

```cpp
  // Appends the values of the source's entries strictly inside the sampled edge to out, in order,
  // ignoring those at its nodes.
  static void sampleEdge(const EdgeSample<double> &sample, std::vector<double> &out);
```

Step 7: In footfall.cpp, replace HungryFootfall::sampleEdge with:

```cpp
void HungryFootfall::sampleEdge(const EdgeSample<double> &sample, std::vector<double> &out) {
  for (const EdgeEntry<double> &entry : sample.Along) {
    out.push_back(entry.Value);
  }
}
```

Step 8: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 4: The visitor gets one entry at a time

Files:
- Modify: `src/sim/medium/field.h`, `src/sim/medium/kept_field.h`, `src/legible/availability.cpp`

Step 1: In field.h, replace SampleRun with:

```cpp
// A run of consecutive places strictly inside one edge: their indices and positions, kept across
// runs so they allocate only while they grow.
template <typename Entry> struct SampleRun {
  std::vector<size_t> Indices;
  std::vector<EdgePosition> Positions;
};
```

Step 2: Replace finishRun's comment and body with:

```cpp
// Hands visit(index, entry) each entry from the slots at the run's places, one slot at a time and,
// for each slot, the run's places in order, and empties the run: those at exactly the place, or
// what the field's sampleEdge makes of each slot's entries gathered once for the run's edge.
template <FieldDefinition F, typename Slot, typename ReadFor, typename Visit>
void finishRun(const Network &network, std::span<const Place> places,
               std::span<const Slot *const> slots, ReadFor &readFor,
               SampleRun<typename F::Entry> &run, SampleScratch<typename F::Entry> &scratch,
               Visit &visit) {
  using Entry = typename F::Entry;
  if (run.Indices.empty()) {
    return;
  }
  const NetworkEdge &edge = network.edges()[run.Positions.front().Edge];
  for (const Slot *slot : slots) {
    const auto read = readFor(*slot);
    if constexpr (HasEdgeRule<F>) {
      if (!gatherEdgeSample(network, *slot, edge, scratch, read)) {
        continue;
      }
      for (size_t k = 0; k < run.Indices.size(); ++k) {
        scratch.Edge.FromOffset = run.Positions[k].FromOffset;
        scratch.Edge.ToOffset = run.Positions[k].ToOffset;
        scratch.Made.clear();
        F::sampleEdge(scratch.Edge, scratch.Made);
        for (const Entry &value : scratch.Made) {
          visit(run.Indices[k], SampledEntry<Entry>{slot->Source, value});
        }
      }
    } else {
      for (size_t k = 0; k < run.Indices.size(); ++k) {
        for (const uint32_t i : positionsAt(*slot, places[run.Indices[k]])) {
          visit(run.Indices[k], SampledEntry<Entry>{slot->Source, read(i)});
        }
      }
    }
  }
  run.Indices.clear();
  run.Positions.clear();
}
```

Step 3: In sampleSlotsAt, replace its comment's first two sentences with "Hands visit(index, entry) each entry from the slots at each place, with the place's index: at a node what atNode appends for each slot, strictly inside an edge those at exactly the place or what the field's sampleEdge makes of the gathered entries, and none where the place does not resolve. Each place's entries arrive in the slots' order, but along one edge the places' entries interleave, one slot at a time." Keep the rest of the comment. Then replace the code after the inside-an-edge branch, from `finishRun<F>(network, places, slots, readFor, run, scratch, visit);` through `visit(index, single);`, with:

```cpp
    finishRun<F>(network, places, slots, readFor, run, scratch, visit);
    if (position) {
      single.clear();
      const uint32_t node = std::get<NodePosition>(*position).Node;
      for (const Slot *slot : slots) {
        atNode(*slot, node, scratch, single, readFor(*slot));
      }
      for (const SampledEntry<Entry> &entry : single) {
        visit(index, entry);
      }
    }
```

Step 4: Change sampleFieldSlotsAt's comment to "The slots' entries at each of the places on the network, handed to visit(index, entry) as sampleSlotsAt gives them for field slots." Its body is unchanged.

Step 5: Replace sampleSlots's body with:

```cpp
  std::vector<SampledEntry<typename F::Entry>> sampled;
  sampleFieldSlotsAt<F>(network, std::span<const Place>(&place, 1), slots,
                        [&sampled](size_t /*index*/, const SampledEntry<typename F::Entry> &entry) {
                          sampled.push_back(entry);
                        });
  return sampled;
```

Step 6: Change sampleKeptFieldAt's declaration comment in field.h to "A kept field's entries at each of the places on the network, handed to visit(index, entry) as sampleFieldAtPlaces gives them. Defined in sim/medium/kept_field.h."

Step 7: Replace sampleFieldAtPlaces's comment and body with:

```cpp
// The field's entries at each of the places on the network, exactly as sampleField gives them at
// each, handed to visit(index, entry), with entry a const SampledEntry<Entry> &. Each place's
// entries arrive in the order sampleField gives them, but along one edge the places' entries
// interleave, one source at a time, and a place with no entries gets no call. Consecutive places
// strictly inside one edge share each source's lookups of its entries at the edge's ends and along
// it. Keeps nothing after it returns.
template <FieldDefinition F, typename Visit>
void sampleFieldAtPlaces(const World &world, const Network &network, std::span<const Place> places,
                         Visit visit) {
  using Entry = typename F::Entry;
  if constexpr (KeptFieldDefinition<F>) {
    sampleKeptFieldAt<F>(world, network, places, visit);
  } else {
    const entt::entity entity = world.findEntity(fieldKey(F::Name));
    const std::vector<const FieldSlot<Entry> *> slots =
        entity == entt::null ? std::vector<const FieldSlot<Entry> *>{}
                             : layeredSlots<F>(world, entity);
    sampleFieldSlotsAt<F>(network, places, slots, visit);
  }
}
```

Step 8: In kept_field.h, replace sampleKeptField's body with:

```cpp
  std::vector<SampledEntry<double>> sampled;
  sampleKeptFieldAt<F>(world, network, std::span<const Place>(&place, 1),
                       [&sampled](size_t /*index*/, const SampledEntry<double> &entry) {
                         sampled.push_back(entry);
                       });
  return sampled;
```

Step 9: In availability.cpp, replace availabilityWith (template, comment included) with:

```cpp
// The source's contribution through its route entry and its supplied offer. availabilityAt and
// availabilityValues both compute every term here, so a place's value is the same however it is
// asked for.
AvailabilityContribution contributionOf(const AvailabilityDefinition &definition,
                                        EntityKey source, const RouteEntry &route,
                                        const OfferEntry &offer) {
  AvailabilityContribution contribution;
  contribution.Shop = source;
  contribution.Relief = offer.Relief;
  contribution.Distance = route.Distance;
  contribution.Wait = static_cast<double>(offer.Wait) * SIM_TICK_SECONDS;
  contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
  contribution.Term = contribution.Relief * discount(definition.Curve, contribution.Time);
  return contribution;
}
```

Step 10: Replace availabilityAt's body with:

```cpp
  const Network &network = parkNetwork(world, PathKind::Guest);
  Availability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route :
       sampleField<RouteDistance<PathKind::Guest>>(world, network, place)) {
    // Sources come in ascending order, and only a source's first entry counts.
    if (route.Source == previous) {
      continue;
    }
    previous = route.Source;
    const std::optional<OfferEntry> offer =
        suppliedOffer(world, definition, network, route.Source);
    if (!offer) {
      continue;
    }
    const AvailabilityContribution contribution =
        contributionOf(definition, route.Source, route.Value, *offer);
    availability.Value += contribution.Term;
    availability.Contributions.push_back(contribution);
  }
  return availability;
```

Step 11: In availabilityValues, keep `network`, `Found`, `found`, and `offerOf` as they are, and replace everything from `std::vector<double> values(places.size(), 0.0);` to the end of the function with:

```cpp
  std::vector<double> values(places.size(), 0.0);
  // Each place's last counted source, since only a source's first entry at a place counts.
  std::vector<EntityKey> counted(places.size(), NULL_KEY);
  // Along an edge, entries come one source at a time, so the next entry's offer is usually the
  // last one's.
  EntityKey lastSource = NULL_KEY;
  std::optional<OfferEntry> lastOffer;
  sampleFieldAtPlaces<RouteDistance<PathKind::Guest>>(
      world, network, places, [&](size_t index, const SampledEntry<RouteEntry> &route) {
        if (route.Source == counted[index]) {
          return;
        }
        counted[index] = route.Source;
        if (route.Source != lastSource) {
          lastSource = route.Source;
          lastOffer = offerOf(route.Source);
        }
        if (lastOffer) {
          values[index] += contributionOf(definition, route.Source, route.Value, *lastOffer).Term;
        }
      });
  return values;
```

Each place still adds its terms in ascending source order starting from 0.0, as availabilityAt does, so the values are bit-identical.

Step 12: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible tpj_scenarios 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

Step 13: Compare the simulation.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/lean-overlay-rebuild-after-scenarios.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/batched-field-samples-after-scenarios.txt build/lean-overlay-rebuild-after-scenarios.txt; echo $?`
Expected: `0` with nothing printed before it.

### Task 5: The band reuses its lists

Files:
- Modify: `src/render/band_overlay.cpp`

Step 1: Change sampleDistances to fill a list it is given:

```cpp
// Fills distances with those the band samples along a carrier: its points', its stops', and each
// positive multiple of OVERLAY_SPACING below its length, ascending, each once.
void sampleDistances(const Carrier &carrier, std::vector<double> &distances) {
  const double length = carrier.Points.back().Distance;
  distances.clear();
  for (const CarrierPoint &point : carrier.Points) {
    distances.push_back(point.Distance);
  }
  for (const CarrierStop &stop : carrier.Stops) {
    distances.push_back(stop.Distance);
  }
  for (uint64_t k = 1; static_cast<double>(k) * OVERLAY_SPACING < length; ++k) {
    distances.push_back(static_cast<double>(k) * OVERLAY_SPACING);
  }
  std::ranges::sort(distances);
  const auto [first, last] = std::ranges::unique(distances);
  distances.erase(first, last);
}
```

Step 2: Change appendBand to take `std::span<const BandRow> rows` instead of `const std::vector<BandRow> &rows`. Its body is unchanged. Add `#include <span>` if missing (it is already included).

Step 3: Replace buildBandOverlay from `// Each band line's rows, colored once every row's value is known.` to the end of the function with:

```cpp
  // Each band line's carrier and its rows' range in rows, colored once every row's value is known.
  struct BandLine {
    const Carrier *Line = nullptr;
    size_t First = 0;
    size_t Count = 0;
  };
  std::vector<BandLine> lines;
  std::vector<BandRow> rows;
  std::vector<Place> places;
  std::vector<double> distances;
  // Carriers come in key order, and a connector's key is no path's.
  for (const Carrier &carrier : network.carriers()) {
    if (!std::ranges::binary_search(guestPaths, carrier.Key)) {
      continue;
    }
    sampleDistances(carrier, distances);
    const size_t first = rows.size();
    for (const double distance : distances) {
      const Place place{carrier.Key, distance};
      const std::optional<GroundPoint> at = network.groundPoint(place);
      if (!at) {
        continue;
      }
      rows.push_back({*at, directionAt(carrier.Points, distance), OVERLAY_ZERO_COLOR});
      places.push_back(place);
    }
    if (rows.size() > first) {
      lines.push_back({&carrier, first, rows.size() - first});
    }
  }
  const std::vector<double> shades = values(places);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].Color = rampColor(shades.at(i), full);
  }
  ParkMesh mesh;
  // Three vertices and twelve indices a row, and two cones a line.
  mesh.Vertices.reserve(3 * rows.size() + 2 * lines.size() * (JOINT_SEGMENTS + 2));
  mesh.Indices.reserve(12 * rows.size() + 2 * lines.size() * 3 * JOINT_SEGMENTS);
  for (const BandLine &line : lines) {
    const std::span<const BandRow> lineRows(rows.data() + line.First, line.Count);
    const std::vector<CarrierPoint> &points = line.Line->Points;
    appendBand(mesh, lineRows);
    appendCone(mesh, lineRows.front(), unitStep(points[1], points[0]));
    appendCone(mesh, lineRows.back(), unitStep(points[points.size() - 2], points.back()));
  }
  return mesh;
```

The rows, their order, and their colors are what they were, so the mesh is identical.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 6: Confirm the overlay looks the same

Run: `cmake.exe --build --preset windows-release 2>&1 | tail -1 && build/windows-release/ThemeParkJones.exe --park tests/parks/warm.park --ticks 3000 --overlay food --capture build/lean-overlay-rebuild-after.bmp; python3 -c "from PIL import Image, ImageChops; a = Image.open('build/batched-field-samples-after.bmp').convert('RGB'); b = Image.open('build/lean-overlay-rebuild-after.bmp').convert('RGB'); print('differing pixels', sum(1 for p in ImageChops.difference(a, b).getdata() if max(p)))"`
Expected: `differing pixels 0`.

### Task 7: Build, tests, tidy

Step 1: Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

### Task 8: Profile

Step 1: Run: `W=$(wslpath -w .) && for i in 1 2 3 4 5; do "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-release\tpj_bench.exe --ticks 1 $W\tests\parks\stress\full.park" "/o:$W\build\perf\lean-after-$i.sleepy"; echo run $i status $?; done`
Expected: five lines, each `status 0`.

Step 2: Sum the time under buildBandOverlay whose leaf is RtlAllocateHeap or RtlFreeHeap, across the five files, as RESEARCH.md did.
Expected: under 5% of the time under buildBandOverlay.

### Task 9: Runtime report

Step 1: Run: `scripts/runtime-report.sh build/runtime-report/lean-overlay-rebuild-after.txt`
Expected: `runtime-report: wrote ...` after about 7 minutes. Compile nothing while it runs.

Step 2: Run: `scripts/runtime-report.sh --compare build/runtime-report/batched-field-samples-after.txt build/runtime-report/lean-overlay-rebuild-after.txt`
Expected: windows-release tests/parks/stress/full.park food-overlay reads `faster` without `result-changed`, and no ticks line reads `slower`.

Step 3: Run: `grep -A1 "windows-release-app.* tests/parks/stress/full.park" build/runtime-report/lean-overlay-rebuild-after.txt`
Expected: the overlay's median frame is no slower than the plain app's, and its worst frame is closer to the plain app's worst frame than 31.5 ms against 13.3 ms.

### Task 10: Update the milestone's frame target

Files:
- Modify: `plans/scalable-runtime/affordable-overlay/MILESTONE.md`

Step 1: Replace the milestone's 3 ms build and 1 ms frame targets with the target FEATURE.md's Spec decisions gives: the app's frame rate on the full park with the overlay shown matches it with the overlay off.

### Task 11: Review and commit

Step 1: Dispatch the reviewer via the `reviewing` skill on this feature's files, with FEATURE.md, src/sim/medium/SPEC.md, and docs/principles.md as context.

Step 2: Commit only this feature's files with `git commit -o`, via the `commit-hygiene` skill. Never stage parks/routes.park or parks/sketch.park.
