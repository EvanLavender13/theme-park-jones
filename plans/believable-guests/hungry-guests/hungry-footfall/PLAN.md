# Implementation Plan: Hungry Footfall

## Goal

Guests publish hungry footfall, each guest-network stretch's moving average of the summed hunger of the guests on it, and carry its values across edits.

## Approach

A private state component, Footfall, on the field's own entity holds one value per stretch at the stretch's midpoint. A system registered after stepGuests updates each stretch's value toward its guests' summed hunger, holds the results, and publishes the same entries into the scalar field HungryFootfall, with one entry per node holding the mean of the stretches meeting it. The field's sampleEdge spreads a stretch's entry over the whole stretch. A finisher registered after carryGuests carries the held midpoints with carryOver, dropping retired ones.

## Tasks

### Task 1: Sim spec

Files:
- Modify: `src/sim/SPEC.md` (the addPark sentence in the schema paragraph)

Step 1: Replace

```
then the guests module's state, system, and finisher, which admit, walk, feed, and carry its guests (sim/guests/SPEC.md),
```

with

```
then the guests module's state, hungry-footfall field, systems, and finishers, which admit, walk, feed, and carry its guests, and average and carry their hungry footfall (sim/guests/SPEC.md),
```

### Task 2: Guests spec

Files:
- Modify: `src/sim/guests/SPEC.md` (the module paragraph, Registration, and a new section before Inspection record)

Step 1: In the module paragraph, replace

```
Believable guests (plans/believable-guests): the park's visitors.
```

with

```
Believable guests (plans/believable-guests): the park's visitors, and the hungry footfall they publish on the guest network.
```

and replace

```
A guest never reads another entity's state, and no other module reads a guest's, so a guest's hunger never leaves it (principles 3 and 6).
```

with

```
A guest never reads another entity's state, and no other module reads a guest's or the module's footfall state, so a guest's hunger leaves the module only summed into the hungry-footfall field it publishes (principles 3 and 6).
```

Step 2: In Registration, replace

```
addGuests registers the state component type Guest, named guest, then the system stepGuests, and then the finisher carryGuests (Carrying).
```

with

```
addGuests registers the state component type Guest, named guest, then the system stepGuests, and then the finisher carryGuests (Carrying). It then registers hungry footfall (Hungry footfall): the field HungryFootfall by addField, the state component type Footfall, named footfall, the system stepFootfall, and the finisher carryFootfall. So guests step before their footfall is averaged, and are carried before it. Footfall is private to the module, declared in guests/internal/footfall.h.
```

Step 3: Insert this section before `## Inspection record`:

```
## Hungry footfall

Guests publish hungry footfall, a scalar field named hungry-footfall on N, defined by HungryFootfall in guests/footfall.h. A stretch is an edge of N. A place lies in the stretch of its own carrier whose FromDistance it is at or above and whose ToDistance it is below, or, at exactly its carrier's length, in its carrier's last stretch. A place whose carrier N lacks, or whose distance is NaN, below 0, or above its carrier's length, lies in none. So a place at a junction lies in one stretch, of its own carrier, and a guest waiting at a shop's guest anchor, the end of the shop's connector, lies in the connector's last stretch.

Each stretch's value is an exponential moving average, with time constant FOOTFALL_TIME, 300 ticks, of the summed hunger of the guests on it. Footfall, private state on the field's entity, fieldKey("hungry-footfall"), holds the values as entries, each a place and a value. Each cycle, after stepGuests, stepFootfall takes each stretch E of N in edges() order. Its held value V is 0.0 with the value of each held entry whose place lies in E added in held order, and its hunger S is 0.0 with the Hunger of each guest whose At lies in E added in ascending key order. Its new value is V + (S - V) / FOOTFALL_TIME, computed in that order with FOOTFALL_TIME converted to double. Footfall then holds one entry per stretch, in edges() order, at the stretch's midpoint, the place of its carrier at (FromDistance + ToDistance) / 2, with its new value. stepFootfall publishes those entries, followed by one entry per node of N in ascending order, at its nodePlace, as the field's stepped entries, with the field's entity as their source. A node's entry is the mean of the new values of the stretches meeting it: 0.0 with the new value added for each end of each stretch at the node, in edges() order and From before To, divided by the number of those ends as a double. Every node is the end of some stretch, so the mean is defined. Node entries are only published, never held. Every guest's hunger counts, with no threshold (principle 5), so a stretch's value approaches the summed hunger of guests who stay on it, and ten hungry guests passing weigh ten times one.

HungryFootfall's sampleEdge gives the values of the source's entries strictly inside the sampled edge, in order, and ignores those at its nodes. A midpoint is strictly inside its stretch and never at a node, so after a step fieldValue anywhere strictly inside a stretch is its value, and at a node, by the medium's node rule, is the node's mean. So a path's end reads its one stretch, and a junction the mean of those meeting it.

A resolution re-derives N under the held entries. The finisher carryFootfall replaces each held entry's place, in held order, with carryOver of it from previousNetwork(world, PathKind::Guest) to N, and drops the entry when that gives none. So an untouched stretch keeps its value, a split stretch's value goes to the half holding the old midpoint while the other half starts at 0, joined stretches' values add, and a deleted path's values are dropped (principle 2). Carrying uses carryOver alone (principle 4), and a candidate's footfall is carried as committing its edit carries it (principle 8). A world's first resolution has no network before, so carrying changes nothing in it. The field's readable entries are those the last step published, so between an edit and the next step they sit where that step put them.
```

### Task 3: Hungry footfall interface

Files:
- Create: `src/sim/guests/footfall.h`
- Create: `src/sim/guests/footfall.cpp`
- Modify: `src/sim/CMakeLists.txt` (tpj_sim's sources)

Step 1: Create src/sim/guests/footfall.h:

```cpp
#ifndef TPJ_SIM_GUESTS_FOOTFALL_H
#define TPJ_SIM_GUESTS_FOOTFALL_H

#include "sim/medium/field.h"

#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

// The hungry footfall field: each guest-network stretch's moving average of the summed hunger of
// the guests on it, one entry per stretch at its midpoint, and at each node the mean of the
// stretches meeting it.
struct HungryFootfall {
  using Entry = double;
  static constexpr std::string_view Name = "hungry-footfall";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  // The values of the source's entries strictly inside the sampled edge, in order, ignoring those
  // at its nodes.
  static std::vector<double> sampleEdge(const EdgeSample<double> &sample);
};

// The moving average's time constant, in ticks: each tick a stretch's value moves 1/FOOTFALL_TIME
// of the way toward its guests' summed hunger.
inline constexpr uint64_t FOOTFALL_TIME = 300;

} // namespace tpj

#endif
```

Step 2: Create src/sim/guests/footfall.cpp with the stub:

```cpp
#include "sim/guests/footfall.h"

#include "sim/medium/field.h"

#include <vector>

namespace tpj {

std::vector<double> HungryFootfall::sampleEdge(const EdgeSample<double> & /*sample*/) { return {}; }

} // namespace tpj
```

Step 3: In src/sim/CMakeLists.txt, add `guests/footfall.cpp` to tpj_sim's sources, before `guests/guests.cpp`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`
Expected: the build finishes with no warnings or errors.

### Task 4: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/SPEC.md, src/sim/guests/SPEC.md, src/sim/medium/SPEC.md, and src/sim/routes/SPEC.md, and the headers src/sim/guests/footfall.h, src/sim/guests/guests.h, src/sim/medium/field.h, src/sim/medium/network.h, src/sim/routes/networks.h, and src/sim/park_schema.h. FEATURE.md's Superseded tests section names the existing test the pass rewrites.

### Task 5: Average and publish the footfall

Files:
- Create: `src/sim/guests/internal/footfall.h`
- Modify: `src/sim/guests/footfall.cpp`
- Modify: `src/sim/guests/guests.cpp` (includes, and addGuests at the end of the file)
- Modify: `src/sim/guests/guests.h` (addGuests's comment)

Step 1: Create src/sim/guests/internal/footfall.h:

```cpp
#ifndef TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H
#define TPJ_SIM_GUESTS_INTERNAL_FOOTFALL_H

#include "sim/medium/field.h"

#include <vector>

namespace tpj {

class WorldSchema;

// State, on hungry footfall's field entity: each stretch's value at the place it is held at, in
// the order the stretches were held.
struct Footfall {
  std::vector<PlacedEntry<double>> Stretches;
};

template <typename Visitor> void visitFields(Visitor &visitor, Footfall &footfall) {
  visitor.field("stretches", footfall.Stretches);
}

// Registers hungry footfall: its field, its state, the system that averages it, and the finisher
// that carries it across each resolution. addGuests calls it after its own registrations.
void addFootfall(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 2: Replace src/sim/guests/footfall.cpp with:

```cpp
#include "sim/guests/footfall.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/guest.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The field's entity, which holds the footfall and is the source of its entries.
constexpr EntityKey FOOTFALL_SOURCE = fieldKey(HungryFootfall::Name);

// The index in edges() of the stretch the place lies in: the one of its own carrier whose
// FromDistance it is at or above and whose ToDistance it is below, or the carrier's last at its
// length. None when the network lacks the carrier or the distance is off it.
std::optional<size_t> stretchOf(const Network &network, const Place &place) {
  const std::vector<NetworkEdge> &edges = network.edges();
  const auto first = std::ranges::lower_bound(edges, place.Carrier, {}, &NetworkEdge::Carrier);
  const auto last =
      std::ranges::upper_bound(first, edges.end(), place.Carrier, {}, &NetworkEdge::Carrier);
  if (first == last || !(place.Distance >= first->FromDistance) ||
      place.Distance > std::prev(last)->ToDistance) {
    return std::nullopt;
  }
  const auto after =
      std::ranges::upper_bound(first, last, place.Distance, {}, &NetworkEdge::FromDistance);
  return static_cast<size_t>(std::prev(after) - edges.begin());
}

// Moves each stretch's value toward the summed hunger of the guests on it, holds the new values
// at the stretches' midpoints, and publishes them with each node's mean of its stretches.
void stepFootfall(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<NetworkEdge> &edges = network.edges();
  Footfall &footfall = world.Registry.get_or_emplace<Footfall>(world.findEntity(FOOTFALL_SOURCE));
  std::vector<double> held(edges.size(), 0.0);
  for (const PlacedEntry<double> &entry : footfall.Stretches) {
    if (const std::optional<size_t> stretch = stretchOf(network, entry.At)) {
      held[*stretch] += entry.Value;
    }
  }
  std::vector<double> hunger(edges.size(), 0.0);
  for (const EntityKey key : parkGuests(world)) {
    const Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<size_t> stretch = stretchOf(network, guest.At)) {
      hunger[*stretch] += guest.Hunger;
    }
  }
  std::vector<PlacedEntry<double>> stretches;
  stretches.reserve(edges.size());
  std::vector<double> nodeSums(network.nodeCount(), 0.0);
  std::vector<uint32_t> nodeEnds(network.nodeCount(), 0);
  for (size_t i = 0; i < edges.size(); ++i) {
    const NetworkEdge &edge = edges[i];
    const double value = held[i] + ((hunger[i] - held[i]) / static_cast<double>(FOOTFALL_TIME));
    stretches.push_back({Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0}, value});
    nodeSums[edge.From] += value;
    ++nodeEnds[edge.From];
    nodeSums[edge.To] += value;
    ++nodeEnds[edge.To];
  }
  footfall.Stretches = stretches;
  std::vector<PlacedEntry<double>> entries = std::move(stretches);
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    entries.push_back(
        {network.nodePlace(node), nodeSums[node] / static_cast<double>(nodeEnds[node])});
  }
  publishStepped<HungryFootfall>(world, FOOTFALL_SOURCE, std::move(entries));
}

} // namespace

std::vector<double> HungryFootfall::sampleEdge(const EdgeSample<double> &sample) {
  std::vector<double> values;
  values.reserve(sample.Along.size());
  for (const EdgeEntry<double> &entry : sample.Along) {
    values.push_back(entry.Value);
  }
  return values;
}

void addFootfall(WorldSchema &schema) {
  addField<HungryFootfall>(schema);
  schema.addComponent<Footfall>("footfall", DataKind::State);
  schema.addSystem(&stepFootfall);
}

} // namespace tpj
```

The field's resolver creates the field's entity in every resolution, and a world is resolved before its first step, so findEntity(FOOTFALL_SOURCE) always finds it while systems step.

Step 3: In src/sim/guests/guests.cpp, add `#include "sim/guests/internal/footfall.h"` after `#include "sim/draw.h"`, and replace addGuests with:

```cpp
void addGuests(WorldSchema &schema) {
  schema.addComponent<Guest>("guest", DataKind::State);
  schema.addSystem(&stepGuests);
  schema.addFinisher(&carryGuests);
  addFootfall(schema);
}
```

Step 4: In src/sim/guests/guests.h, replace

```cpp
// Registers the guest state, the system that steps guests and admits new ones, and the finisher
// that carries their places across each resolution. The routes and operations modules'
// registrations come first, and addDropPreviousNetworks after.
```

with

```cpp
// Registers the guest state, the system that steps guests and admits new ones, and the finisher
// that carries their places across each resolution, and then hungry footfall's field, state,
// system, and finisher. The routes and operations modules' registrations come first, and
// addDropPreviousNetworks after.
```

Step 5: Build and run the guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`, then, for each file under tests/sim/guests, `build/windows-debug/tpj_sim_tests.exe -# "[#<file name without .cpp>]" 2>&1 | tr -d '\r' | tail -3`
Expected: the build has no warnings. The test pass's tests of criteria 2 and 4, and of criterion 3 with no commands, pass. Criterion 1's test still fails for the missing finisher, and criterion 3's tests may fail where an edit moves a guest path, whose held places are not yet carried. Every other guests test passes.

### Task 6: Carry the footfall

Files:
- Modify: `src/sim/guests/footfall.cpp`

Step 1: In footfall.cpp's anonymous namespace, after stepFootfall, add:

```cpp
// Carries each held value's place from the guest network before the resolution to the new one,
// dropping those whose places are retired.
void carryFootfall(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  const entt::entity entity = world.findEntity(FOOTFALL_SOURCE);
  auto *footfall = before == nullptr || entity == entt::null
                       ? nullptr
                       : world.Registry.try_get<Footfall>(entity);
  if (footfall == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  std::vector<PlacedEntry<double>> carried;
  carried.reserve(footfall->Stretches.size());
  for (const PlacedEntry<double> &entry : footfall->Stretches) {
    if (const std::optional<Place> at = carryOver(entry.At, *before, after)) {
      carried.push_back({*at, entry.Value});
    }
  }
  footfall->Stretches = std::move(carried);
}
```

Step 2: In addFootfall, after `schema.addSystem(&stepFootfall);`, add `schema.addFinisher(&carryFootfall);`.

Step 3: Build and run the guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests 2>&1 | tail -3`, then, for each file under tests/sim/guests, `build/windows-debug/tpj_sim_tests.exe -# "[#<file name without .cpp>]" 2>&1 | tr -d '\r' | tail -3`
Expected: the build has no warnings, and every guests test passes, including the test pass's tests of criteria 1 to 4.

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

Step 4: Run the cross-build check (criterion 5).

Run: `scripts/cross-build-check.sh 2>&1 | tail -3`
Expected: the check reports the builds' outputs identical.

Step 5: Check the app still runs.

Run, from build/windows-debug: `timeout 90 ./ThemeParkJones.exe --park ../../tests/parks/supply.park --ticks 1800 --capture supply.bmp`
Expected: exit 0, and the capture shows guests on the paths as before.

### Task 8: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, and present the findings to Evan.

Step 2: Commit once, via the commit-hygiene skill, with the subject `Guests: Publish hungry footfall on the guest network` and a body naming the moving average, the held midpoints, the sampleEdge rule, and the carrying finisher.
