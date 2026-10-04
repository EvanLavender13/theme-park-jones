# Implementation Plan: Batched Field Samples

## Goal

Give the shared medium a sample of a field at a list of places that shares each source's lookups among consecutive places inside one edge, route every existing sample through it, and have the availability overlay read route distance with it, so the full park's overlay rebuilds cheaply.

## Approach

sampleSlotInEdge in src/sim/medium/field.h is split into gatherEdgeSample, which finds a source's entries at an edge's ends and along it into an EdgeSample, and the field's own rule, which a new core, sampleSlotsAt, runs at each place of a run of consecutive places inside one edge after gathering once per source. sampleSlots, sampleResolvedField, and the kept field's sample become the core given one place, so the guests, the operations, and the scenarios exercise it at every tick, and the new public sampleFieldAtPlaces hands each place's entries to a visitor. legible's availabilityValues reads guest route distance through sampleFieldAtPlaces, computing each place's value with the same availabilityWith as availabilityAt.

## Placement

Decision 0027 places each behavior this feature adds:

- Sampling a field at a list of places, sharing lookups along an edge: sim/medium, sim/medium/field.h, beside sampleField. The medium owns how fields are read on networks, and every field's owner and reader reaches it there.
- Sampling a kept field at a list of places: sim/medium, sim/medium/kept_field.h, beside sampleKeptField, which holds the kept layer's reading rules.
- The availability overlay's values read through the batch: legible, legible/availability.cpp, which already computes them.

## Tasks

### Task 1: The before output, report, and capture

Files: none in the repository.

The code is what prepared-availability merged (d4f8325), and its outputs were taken on it, so they are this feature's before: copy build/scenarios-probe.txt to build/batched-field-samples-before-scenarios.txt, build/runtime-report/prepared-availability-after.txt to build/runtime-report/batched-field-samples-before.txt, and build/prepared-availability-after.bmp to build/batched-field-samples-before.bmp.

Run: `git diff --stat d4f8325 HEAD -- src scripts tests | wc -l`
Expected: `0`

### Task 2: Medium spec

Files:
- Modify: `src/sim/medium/SPEC.md` (section "## Fields")

Step 1: After the paragraph beginning "sampleField gives a field's entries at a place on a network", add a paragraph with the text FEATURE.md's Spec changes gives, beginning "sampleFieldAtPlaces gives a field's entries at each of a list of places in one call".

### Task 3: Gather an edge's entries once, and the batch core

Files:
- Modify: `src/sim/medium/field.h:385-440` (sampleSlotInEdge), `src/sim/medium/field.h:10-23` (includes)

Step 1: Add `#include <stddef.h>` after `#include <span>` in src/sim/medium/field.h.

Step 2: Replace sampleSlotInEdge, from its comment "// Appends the source's entries at a place strictly inside an edge" to its closing brace, with

```cpp
// Fills the scratch's edge sample with the source's entries at the edge's two ends and strictly
// inside it, each with the value read gives its position, leaving the sample's offsets as they
// were, and gives whether it found any. They depend on the edge alone, so every place strictly
// inside the edge shares them.
template <typename Entry, typename Slot, typename Read>
bool gatherEdgeSample(const Network &network, const Slot &slot, const NetworkEdge &edge,
                      SampleScratch<Entry> &scratch, Read read) {
  const std::span<const Place> fromStops = network.stopPlaces(edge.From);
  const std::span<const Place> toStops = network.stopPlaces(edge.To);
  EdgeSample<Entry> &sample = scratch.Edge;
  sample.AtFrom.clear();
  sample.AtTo.clear();
  sample.Along.clear();
  sample.Edge = edge;
  // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
  // between its stops, and to one of its nodes exactly when it is at one of that node's stop
  // places, so no entry is resolved. The offsets are resolve's own subtractions.
  std::vector<uint32_t> &inside = scratch.Inside;
  inside.clear();
  positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
  std::ranges::sort(inside);
  for (const uint32_t i : inside) {
    const Place &at = slot.Entries[i].At;
    sample.Along.push_back({at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, read(i)});
  }
  // An entry inside the edge is not also at an end, as resolving it gives the inside.
  positionsAtAny(slot, fromStops, scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    if (!std::ranges::binary_search(inside, i)) {
      sample.AtFrom.push_back(read(i));
    }
  }
  positionsAtAny(slot, toStops, scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    if (!std::ranges::binary_search(inside, i)) {
      sample.AtTo.push_back(read(i));
    }
  }
  return !sample.AtFrom.empty() || !sample.AtTo.empty() || !sample.Along.empty();
}

// A run of consecutive places strictly inside one edge: their indices, their positions, and each
// one's entries, kept across runs so they allocate only while they grow.
template <typename Entry> struct SampleRun {
  std::vector<size_t> Indices;
  std::vector<EdgePosition> Positions;
  std::vector<std::vector<SampledEntry<Entry>>> Entries;
};

// Hands visit(index, entries) the entries from the slots at each place of the run, in the run's
// order, and empties the run: those at exactly the place, or what the field's sampleEdge makes of
// each slot's entries gathered once for the run's edge.
template <FieldDefinition F, typename Slot, typename ReadFor, typename Visit>
void finishRun(const Network &network, std::span<const Place> places,
               std::span<const Slot *const> slots, ReadFor &readFor,
               SampleRun<typename F::Entry> &run, SampleScratch<typename F::Entry> &scratch,
               Visit &visit) {
  using Entry = typename F::Entry;
  const size_t count = run.Indices.size();
  if (count == 0) {
    return;
  }
  if (run.Entries.size() < count) {
    run.Entries.resize(count);
  }
  for (size_t k = 0; k < count; ++k) {
    run.Entries[k].clear();
  }
  const NetworkEdge &edge = network.edges()[run.Positions.front().Edge];
  for (const Slot *slot : slots) {
    const auto read = readFor(*slot);
    if constexpr (HasEdgeRule<F>) {
      if (!gatherEdgeSample(network, *slot, edge, scratch, read)) {
        continue;
      }
      for (size_t k = 0; k < count; ++k) {
        scratch.Edge.FromOffset = run.Positions[k].FromOffset;
        scratch.Edge.ToOffset = run.Positions[k].ToOffset;
        for (const Entry &value : F::sampleEdge(scratch.Edge)) {
          run.Entries[k].push_back({slot->Source, value});
        }
      }
    } else {
      for (size_t k = 0; k < count; ++k) {
        for (const uint32_t i : positionsAt(*slot, places[run.Indices[k]])) {
          run.Entries[k].push_back({slot->Source, read(i)});
        }
      }
    }
  }
  for (size_t k = 0; k < count; ++k) {
    visit(run.Indices[k], run.Entries[k]);
  }
  run.Indices.clear();
  run.Positions.clear();
}

// Hands visit(index, entries) each place's entries from the slots, in the places' order, and each
// place's in the slots' order: at a node what atNode appends for each slot, strictly inside an
// edge those at exactly the place or what the field's sampleEdge makes of the gathered entries,
// and none where the place does not resolve. Consecutive places strictly inside one edge share
// each slot's gathered entries, so only the field's rule runs at each of them. readFor(slot)
// gives the function reading a position of that slot.
template <FieldDefinition F, typename Slot, typename ReadFor, typename AtNode, typename Visit>
void sampleSlotsAt(const Network &network, std::span<const Place> places,
                   std::span<const Slot *const> slots, ReadFor readFor, AtNode atNode,
                   Visit visit) {
  using Entry = typename F::Entry;
  SampleScratch<Entry> scratch;
  SampleRun<Entry> run;
  std::vector<SampledEntry<Entry>> single;
  for (size_t index = 0; index < places.size(); ++index) {
    const std::optional<NetworkPosition> position = network.resolve(places[index]);
    if (position) {
      if (const auto *inside = std::get_if<EdgePosition>(&*position)) {
        if (!run.Indices.empty() && run.Positions.front().Edge != inside->Edge) {
          finishRun<F>(network, places, slots, readFor, run, scratch, visit);
        }
        run.Indices.push_back(index);
        run.Positions.push_back(*inside);
        continue;
      }
    }
    finishRun<F>(network, places, slots, readFor, run, scratch, visit);
    single.clear();
    if (position) {
      const uint32_t node = std::get<NodePosition>(*position).Node;
      for (const Slot *slot : slots) {
        atNode(*slot, node, scratch, single, readFor(*slot));
      }
    }
    visit(index, single);
  }
  finishRun<F>(network, places, slots, readFor, run, scratch, visit);
}
```

The run's work is its own function, finishRun, with the run's lists in a SampleRun, so that sampleSlotsAt stays within tidy's readability-function-size limit of 60 statements.

Step 3: Replace sampleSlots, from its comment "// The entries of the slots at the place on the network" to its closing brace, with

```cpp
// The slots' entries at each of the places on the network, handed to visit(index, entries) in the
// places' order, as sampleSlotsAt gives them for field slots.
template <FieldDefinition F, typename Visit>
void sampleFieldSlotsAt(const Network &network, std::span<const Place> places,
                        std::span<const FieldSlot<typename F::Entry> *const> slots, Visit visit) {
  using Entry = typename F::Entry;
  using Slot = FieldSlot<Entry>;
  sampleSlotsAt<F>(
      network, places, slots,
      [](const Slot &slot) { return [&slot](uint32_t i) { return slot.Entries[i].Value; }; },
      [&network](const Slot &slot, uint32_t node, SampleScratch<Entry> &scratch,
                 std::vector<SampledEntry<Entry>> &sampled, auto read) {
        sampleSlotAtNode(network, slot, node, scratch, sampled, read);
      },
      visit);
}

// The entries of the slots at the place on the network, in the slots' order, by the default rule
// or the field's sampleEdge: the batch of one place.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleSlots(const Network &network, const Place &place,
            const std::vector<const FieldSlot<typename F::Entry> *> &slots) {
  std::vector<SampledEntry<typename F::Entry>> sampled;
  sampleFieldSlotsAt<F>(network, std::span<const Place>(&place, 1), slots,
                        [&sampled](size_t /*index*/,
                                   std::vector<SampledEntry<typename F::Entry>> &entries) {
                          sampled = std::move(entries);
                        });
  return sampled;
}
```

Step 4: After the declaration of sampleKeptField (the comment beginning "// A kept field's entries at the place on the network, as sampleField gives them."), add

```cpp
// A kept field's entries at each of the places on the network, as sampleFieldAtPlaces gives them.
// Defined in sim/medium/kept_field.h.
template <KeptFieldDefinition F, typename Visit>
void sampleKeptFieldAt(const World &world, const Network &network, std::span<const Place> places,
                       Visit visit);
```

Step 5: After sampleField's definition, add

```cpp
// The field's entries at each of the places on the network, exactly as sampleField gives them at
// each, handed to visit(index, entries), with entries a std::span<const SampledEntry<Entry>> valid
// during the call, in the places' order. Consecutive places strictly inside one edge share each
// source's lookups of its entries at the edge's ends and along it. Keeps nothing after it returns.
template <FieldDefinition F, typename Visit>
void sampleFieldAtPlaces(const World &world, const Network &network, std::span<const Place> places,
                         Visit visit) {
  using Entry = typename F::Entry;
  const auto handOn = [&visit](size_t index, std::vector<SampledEntry<Entry>> &entries) {
    visit(index, std::span<const SampledEntry<Entry>>(entries));
  };
  if constexpr (KeptFieldDefinition<F>) {
    sampleKeptFieldAt<F>(world, network, places, handOn);
  } else {
    const entt::entity entity = world.findEntity(fieldKey(F::Name));
    const std::vector<const FieldSlot<Entry> *> slots =
        entity == entt::null ? std::vector<const FieldSlot<Entry> *>{}
                             : layeredSlots<F>(world, entity);
    sampleFieldSlotsAt<F>(network, places, slots, handOn);
  }
}
```

Step 6: Do not build yet: kept_field.h still calls sampleSlotInEdge until Task 4.

### Task 4: Kept fields through the batch

Files:
- Modify: `src/sim/medium/kept_field.h:309-338` (sampleKeptField)

Step 1: Replace sampleKeptField's definition, from `template <KeptFieldDefinition F>` above `std::vector<SampledEntry<double>> sampleKeptField(` to its closing brace, with

```cpp
template <KeptFieldDefinition F, typename Visit>
void sampleKeptFieldAt(const World &world, const Network &network, std::span<const Place> places,
                       Visit visit) {
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  std::vector<const KeptSlot *> slots;
  if (kept != nullptr) {
    slots.reserve(kept->Slots.size());
    for (const KeptSlot &slot : kept->Slots) {
      slots.push_back(&slot);
    }
  }
  sampleSlotsAt<F>(
      network, places, std::span<const KeptSlot *const>(slots),
      [&world](const KeptSlot &slot) {
        return [&slot, &world](uint32_t i) {
          return readKeptEntry<F>(slot.Entries[i], world.Tick);
        };
      },
      [&network](const KeptSlot &slot, uint32_t node, SampleScratch<double> &scratch,
                 std::vector<SampledEntry<double>> &sampled, auto read) {
        if constexpr (HasNodeRule<F>) {
          sampleKeptSlotAtNode<F>(network, slot, node, scratch, sampled, read);
        } else {
          sampleSlotAtNode(network, slot, node, scratch, sampled, read);
        }
      },
      visit);
}

template <KeptFieldDefinition F>
std::vector<SampledEntry<double>> sampleKeptField(const World &world, const Network &network,
                                                  const Place &place) {
  std::vector<SampledEntry<double>> sampled;
  sampleKeptFieldAt<F>(world, network, std::span<const Place>(&place, 1),
                       [&sampled](size_t /*index*/, std::vector<SampledEntry<double>> &entries) {
                         sampled = std::move(entries);
                       });
  return sampled;
}
```

Step 2: Build the simulation

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

Step 3: Confirm the simulation is unchanged (criterion 1)

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | tail -1 && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/batched-field-samples-after-scenarios.txt 2>/dev/null; build/windows-debug/tpj_scenarios.exe --compare build/batched-field-samples-before-scenarios.txt build/batched-field-samples-after-scenarios.txt; echo $?`
Expected: the build's last line, then `0` with nothing printed between.

### Task 5: The availability overlay reads route distance in one batch

Files:
- Modify: `src/legible/availability.cpp:50-105` (availabilityWith, availabilityAt, availabilityValues)

Step 1: Replace availabilityWith with

```cpp
// The availability given each reachable source's route entries, sources ascending, with each
// source's supplied offer from offerOf. Every value an availability gives is computed here, so a
// place's value is the same however it is asked for.
template <typename OfferOf>
Availability availabilityWith(const AvailabilityDefinition &definition,
                              std::span<const SampledEntry<RouteEntry>> routes,
                              const OfferOf &offerOf) {
  Availability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route : routes) {
```

keeping the rest of its body, from the comment "// Sources come in ascending order, and only a source's first entry counts." to its end, unchanged.

Step 2: Replace availabilityAt's body with

```cpp
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<SampledEntry<RouteEntry>> routes =
      sampleField<RouteDistance<PathKind::Guest>>(world, network, place);
  return availabilityWith(definition, routes, [&](EntityKey source) {
    return suppliedOffer(world, definition, network, source);
  });
```

Step 3: In availabilityValues, replace

```cpp
  std::vector<double> values;
  values.reserve(places.size());
  for (const Place &place : places) {
    values.push_back(availabilityWith(world, definition, network, place, offerOf).Value);
  }
  return values;
```

with

```cpp
  std::vector<double> values(places.size(), 0.0);
  sampleFieldAtPlaces<RouteDistance<PathKind::Guest>>(
      world, network, places,
      [&](size_t index, std::span<const SampledEntry<RouteEntry>> routes) {
        values[index] = availabilityWith(definition, routes, offerOf).Value;
      });
  return values;
```

Step 4: Add `#include <span>` after `#include <optional>` in src/legible/availability.cpp.

Step 5: Build

Run: `cmake.exe --build --preset windows-debug --target tpj_legible 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 6: Confirm the build, tests, and tidy

Step 1: Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

### Task 7: The after report and capture

Step 1: Run (in the background): `scripts/runtime-report.sh build/runtime-report/batched-field-samples-after.txt`
Expected: `runtime-report: wrote ...` after about 7 minutes. Compile nothing while it runs.

Step 2: Compare (criterion 3)

Run: `scripts/runtime-report.sh --compare build/runtime-report/batched-field-samples-before.txt build/runtime-report/batched-field-samples-after.txt`
Expected: the line for windows-release tests/parks/stress/full.park food-overlay reads `faster`, and no line for a ticks stage reads `slower`.

Then, over the whole milestone, run: `scripts/runtime-report.sh --compare build/runtime-report/prepared-availability-before.txt build/runtime-report/batched-field-samples-after.txt | grep 'windows-release  *tests/parks/stress/full.park  *food-overlay'`
Expected: one line reading `faster`.

Step 3: The frames with the overlay shown (criterion 4)

Run: `scripts/runtime-report.sh --compare build/runtime-report/batched-field-samples-before.txt build/runtime-report/batched-field-samples-after.txt | grep 'windows-release-app-overlay  *tests/parks/stress/full.park'`
Expected: one line reading `faster`.

Step 4: Capture and compare (criterion 2)

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/warm.park --ticks 3000 --overlay food --capture build/batched-field-samples-after.bmp; python3 -c "from PIL import Image, ImageChops; a = Image.open('build/batched-field-samples-before.bmp').convert('RGB'); b = Image.open('build/batched-field-samples-after.bmp').convert('RGB'); print('differing pixels', sum(1 for p in ImageChops.difference(a, b).getdata() if max(p)))"`
Expected: `differing pixels 0`.

Put both comparisons, the frames lines of both reports, and the capture in the feature's report, and ask the user to try criterion 5's hungry footfall by play.

### Task 8: Commit

Commit the feature once through the commit-hygiene skill, with `git commit -o` naming only this feature's files: src/sim/medium/field.h, src/sim/medium/kept_field.h, src/sim/medium/SPEC.md, src/legible/availability.cpp, and this feature's FEATURE.md, PLAN.md, and RESEARCH.md, after `git add` of the three plan files just before it. Subject: `Medium: Sample a field at many places sharing edge lookups`.
