# Implementation Plan: Footfall on Kept Entries

## Goal

Hungry footfall held as kept entries, changed each tick only where guests stand, with the parks that hold it remade and the runtime report compared before and after.

## Approach

HungryFootfall gains readKept and sampleNode and is registered with addKeptField, and the private Footfall state goes. stepFootfall pairs each guest's stretch with its hunger, stable-sorts the pairs by stretch, and keeps one new value per stretch it finds. carryFootfall carries each kept entry by carryOver, groups the carried entries by stretch, and replaces the source's entries with one per stretch at its midpoint. The parks are remade by the tools that made them. winding-path.park is converted by a one-off script run from the scratchpad, never committed.

## Placement

Decision 0027 places each behavior this feature adds:

- Footfall's decay rule and node rule: guests, HungryFootfall in src/sim/guests/footfall.h. The medium makes a kept field's owner state its rules for reading (src/sim/medium/SPEC.md, Kept fields), and guests own hungry footfall.
- Changing only the stretches guests stand in: guests, stepFootfall in src/sim/guests/footfall.cpp, the module's system that already averages footfall.
- Carrying and combining kept entries across a resolution: guests, the finisher carryFootfall in src/sim/guests/footfall.cpp. The medium makes a kept field's owner carry its entries in its own finisher.
- Dropping the private Footfall state: guests, src/sim/guests/internal/footfall.h, which keeps only addFootfall's declaration.
- The parks' new saved form: data under tests/parks, remade by the existing tpj_scenarios and tpj_bench. The one-off conversion of winding-path.park is a command, not a tool, and adds no code.

No change touches src/app/main.cpp or src/scenarios/main.cpp.

## Tasks

### Task 1: Record the runtime report before the change

Files: none in the repository.

Step 1: Skip this task when build/runtime-report/following-footfall-before.txt exists and build/following-footfall/report-before.log's last line reads `runtime-report: wrote ... following-footfall-before.txt ...`. Both are recorded on main at 6429bff. Otherwise run, from main before any change:

Run: `mkdir -p build/following-footfall && scripts/runtime-report.sh build/runtime-report/following-footfall-before.txt > build/following-footfall/report-before.log 2>&1; tail -1 build/following-footfall/report-before.log`
Expected: `runtime-report: wrote <path>/following-footfall-before.txt in <m>m <ss>s`

### Task 2: Create the branch

Run: `git switch -c footfall-on-kept-entries`
Expected: `Switched to a new branch 'footfall-on-kept-entries'`

### Task 3: Update the guests and legible specs

Files:
- Modify: `src/sim/guests/SPEC.md:3`, `:7`, `:65-73`
- Modify: `src/legible/SPEC.md:27`

Step 1: Make the five changes FEATURE.md's Spec changes section gives, word for word: the opening paragraph's sentence, the Registration sentences, the Hungry footfall section's first sentence, the midpoint sentence appended to that paragraph, and the three paragraphs that replace the paragraphs beginning "Each stretch's value is an exponential moving average", "HungryFootfall's sampleEdge gives", and "A resolution re-derives N under the held entries".

Step 2: In src/legible/SPEC.md, make the change FEATURE.md's Spec changes section gives.

Step 3: Run: `grep -c "Footfall, private state\|addField, the state component type Footfall\|module's footfall state" src/sim/guests/SPEC.md`
Expected: `0`

### Task 4: Make HungryFootfall a kept field, with stubs

Files:
- Modify: `src/sim/guests/footfall.h`
- Modify: `src/sim/guests/internal/footfall.h`
- Modify: `src/sim/guests/footfall.cpp`

Step 1: In src/sim/guests/footfall.h, add `#include "sim/medium/kept_field.h"` after `#include "sim/medium/field.h"`, and replace the struct and its comment with:

```cpp
// The hungry footfall field: each guest-network stretch's moving average of the summed hunger of
// the guests on it, kept at the midpoints of the stretches guests have stood in, and at each node
// the mean of the stretches meeting it, worked out when read.
struct HungryFootfall {
  using Entry = double;
  static constexpr std::string_view Name = "hungry-footfall";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  // A value the ticks after it was kept, decayed as an empty stretch's:
  // value * simExp(ticks * simLog(1 - 1 / FOOTFALL_TIME)), so the value itself at 0 ticks.
  static double readKept(double value, uint64_t ticks);

  // The values of the source's entries strictly inside the sampled edge, in order, ignoring those
  // at its nodes.
  static std::vector<double> sampleEdge(const EdgeSample<double> &sample);

  // The mean, over the node's edge ends, of each end's stretch value: the sum of the values of
  // the entries strictly inside its edge.
  static std::vector<double> sampleNode(const NodeSample &sample);
};
```

Step 2: In src/sim/guests/internal/footfall.h, delete the Footfall struct, its comment, and its visitFields, and the includes of sim/medium/field.h and <vector>. Change addFootfall's comment to:

```cpp
// Registers hungry footfall: its kept field, the system that averages it, and the finisher that
// carries it across each resolution. addGuests calls it after its own registrations.
```

Step 3: In src/sim/guests/footfall.cpp, replace stepFootfall's and carryFootfall's bodies with nothing, naming their parameters `World & /*world*/`. Mark stretchOf `[[maybe_unused]]` until Task 8. Add these stubs after sampleEdge:

```cpp
double HungryFootfall::readKept(double value, uint64_t /*ticks*/) { return value; }

std::vector<double> HungryFootfall::sampleNode(const NodeSample & /*sample*/) { return {}; }
```

In addFootfall, replace `addField<HungryFootfall>(schema);` and `schema.addComponent<Footfall>("footfall", DataKind::State);` with `addKeptField<HungryFootfall>(schema);`. Keep the file's includes: Tasks 8 and 9 use them again.

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_legible tpj_app tpj_scenarios tpj_bench`
Expected: builds with no warnings. tpj_sim_tests does not build yet: tests/sim/guests/arrivals_test.cpp calls addField<HungryFootfall>, which addField now refuses at compile time. The test pass rewrites that test.

### Task 5: Edit fed.park to the new saved form

Files:
- Modify: `tests/parks/fed.park:41-42`

Step 1: Replace the two lines

```
[hungry-footfall-stepped]
11741940842860801778 readable=[] pending=[]
```

with

```
[hungry-footfall-kept]
11741940842860801778 slots=[] pending=[]
```

Step 2: Run: `build/windows-debug/tpj_bench.exe --ticks 1 tests/parks/fed.park | head -1`
Expected: a line beginning `park tests/parks/fed.park ticks 1`, with no load error.

### Task 6: Test pass

Dispatch the test-writer as implementing-features' Test pass dispatch gives, with:

- Feature: plans/scalable-runtime/following-footfall/footfall-on-kept-entries/FEATURE.md
- Specs: src/sim/guests/SPEC.md, src/sim/medium/SPEC.md, src/sim/SPEC.md
- Public headers: src/sim/guests/footfall.h, src/sim/guests/guests.h, src/sim/medium/kept_field.h, src/sim/medium/field.h, src/sim/medium/network.h, src/sim/routes/networks.h

Expected: tpj_sim_tests builds. The tests of criteria 1 to 7 fail against the stubs. Tests that open warm.park or cut.park, in tests/legible and tests/integration, fail with a LoadError naming `[hungry-footfall-stepped]` until Task 10 remakes those parks.

### Task 7: Footfall's read rules

Files:
- Modify: `src/sim/guests/footfall.cpp` (readKept and sampleNode)

Step 1: Add `#include "sim/sim_math.h"` and replace the two stubs with:

```cpp
double HungryFootfall::readKept(double value, uint64_t ticks) {
  const double perTick = 1.0 - (1.0 / static_cast<double>(FOOTFALL_TIME));
  return value * simExp(static_cast<double>(ticks) * simLog(perTick));
}

std::vector<double> HungryFootfall::sampleNode(const NodeSample &sample) {
  double sum = 0.0;
  for (const NodeEnd &end : sample.Ends) {
    double stretch = 0.0;
    for (const EdgeEntry<double> &entry : end.Along) {
      stretch += entry.Value;
    }
    sum += stretch;
  }
  return {sum / static_cast<double>(sample.Ends.size())};
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#footfall_test]"`
Expected: builds with no warnings. The test pass's tests of criteria 4 and 5 pass, unless they need a stepped world to read.

### Task 8: Step only the stretches guests stand in

Files:
- Modify: `src/sim/guests/footfall.cpp` (stepFootfall, stretchOf)

Step 1: Remove `[[maybe_unused]]` from stretchOf. After stretchOf, add:

```cpp
// The place of the stretch's carrier halfway between its stops, where its value is kept.
Place midpointOf(const NetworkEdge &edge) {
  return Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0};
}

// A guest's stretch, as its index in edges(), and its hunger.
struct GuestOnStretch {
  size_t Stretch = 0;
  double Hunger = 0.0;
};
```

Step 2: Replace stepFootfall and its comment with:

```cpp
// Moves the value of each stretch some guest stands in toward the summed hunger of the guests
// there, and visits no other stretch.
void stepFootfall(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  // In ascending key order, then grouped by stretch with that order kept within each.
  std::vector<GuestOnStretch> onStretches;
  for (const EntityKey key : parkGuests(world)) {
    const Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<size_t> stretch = stretchOf(network, guest.At)) {
      onStretches.push_back({.Stretch = *stretch, .Hunger = guest.Hunger});
    }
  }
  std::ranges::stable_sort(onStretches, {}, &GuestOnStretch::Stretch);
  for (auto group = onStretches.begin(); group != onStretches.end();) {
    const size_t stretch = group->Stretch;
    double hunger = 0.0;
    for (; group != onStretches.end() && group->Stretch == stretch; ++group) {
      hunger += group->Hunger;
    }
    const Place midpoint = midpointOf(network.edges()[stretch]);
    const double held =
        keptValue<HungryFootfall>(world, FOOTFALL_SOURCE, midpoint).value_or(0.0);
    keepEntry<HungryFootfall>(world, FOOTFALL_SOURCE, midpoint,
                              held + ((hunger - held) / static_cast<double>(FOOTFALL_TIME)));
  }
}
```

Change FOOTFALL_SOURCE's comment to "The field's entity, the source of its kept entries."

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#footfall_test]"`
Expected: builds with no warnings. The tests of criteria 1 to 5 pass. Criterion 2's may still fail on worlds that resolve an edit.

### Task 9: Carry kept entries across a resolution

Files:
- Modify: `src/sim/guests/footfall.cpp` (carryFootfall)

Step 1: Add `#include <span>` to the file's standard includes. After GuestOnStretch, add:

```cpp
// A kept entry carried to a stretch of the new network, as its index in edges().
struct CarriedEntry {
  size_t Stretch = 0;
  const KeptEntry *Entry = nullptr;
};

// One entry at the stretch's midpoint for the entries carried into it: a lone entry keeps its
// value and tick, and several hold their values read at the tick, added in held order, with the
// tick.
KeptEntry combineCarried(std::span<const CarriedEntry> group, const Place &midpoint,
                         uint64_t tick) {
  if (group.size() == 1) {
    return {.At = midpoint, .Value = group.front().Entry->Value, .Tick = group.front().Entry->Tick};
  }
  double sum = 0.0;
  for (const CarriedEntry &carried : group) {
    sum += readKeptEntry<HungryFootfall>(*carried.Entry, tick);
  }
  return {.At = midpoint, .Value = sum, .Tick = tick};
}
```

Step 2: Replace carryFootfall and its comment with:

```cpp
// Carries each kept value from the guest network before the resolution to the new one, holding
// one entry per stretch at its midpoint and dropping values whose places are retired.
void carryFootfall(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  if (before == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  std::vector<CarriedEntry> carried;
  for (const KeptEntry &entry : keptEntries<HungryFootfall>(world, FOOTFALL_SOURCE)) {
    const std::optional<Place> at = carryOver(entry.At, *before, after);
    if (const std::optional<size_t> stretch = at ? stretchOf(after, *at) : std::nullopt) {
      carried.push_back({.Stretch = *stretch, .Entry = &entry});
    }
  }
  std::ranges::stable_sort(carried, {}, &CarriedEntry::Stretch);
  std::vector<KeptEntry> entries;
  for (auto group = carried.begin(); group != carried.end();) {
    const size_t stretch = group->Stretch;
    const auto next = std::ranges::find_if(
        group, carried.end(), [stretch](const CarriedEntry &c) { return c.Stretch != stretch; });
    entries.push_back(combineCarried(std::span<const CarriedEntry>(group, next),
                                     midpointOf(after.edges()[stretch]), world.Tick));
    group = next;
  }
  replaceKeptEntries<HungryFootfall>(world, FOOTFALL_SOURCE, std::move(entries));
}
```

entries is built in full before replaceKeptEntries runs, so the pointers into the held entries are never read after they change.

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#footfall_test],[#guest_edits_test],[#arrivals_test]"`
Expected: builds with no warnings, and every test in the three files passes.

### Task 10: Remake fed.park, warm.park, and cut.park

Files:
- Modify: `tests/parks/fed.park`, `tests/parks/warm.park`, `tests/parks/cut.park`

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park`
Expected: exits with status 0.

Step 2 (criterion 8): Run: `git diff -U0 -- tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park | grep -E '^[-+]([^-+]|$)' | grep -v -E '^[-+](\[hungry-footfall-(stepped|kept)\]|\[footfall\]|11741940842860801778 |$)' | wc -l`
Expected: `0`. Put the command and its output in the feature's report. `git diff --stat -- tests/parks/fed.park` shows the two lines Task 5 changed and no others, so the tool rewrote fed.park unchanged.

### Task 11: Remake full.park

Files:
- Modify: `tests/parks/stress/full.park`

Step 1: Run: `cmake.exe --build --preset windows-release --target tpj_bench && build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park`
Expected: exits with status 0. windows-release steps the park's 1,800 warm-up ticks quickly, and decision 0022 makes its save the same text as any build's.

Step 2 (criterion 8): Run: `git diff -U0 -- tests/parks/stress/full.park | grep -E '^[-+]([^-+]|$)' | grep -v -E '^[-+](\[hungry-footfall-(stepped|kept)\]|\[footfall\]|11741940842860801778 |$)' | wc -l`
Expected: `0`. Put the command and its output in the feature's report.

### Task 12: Convert winding-path.park

Files:
- Modify: `tests/parks/stress/winding-path.park`

Step 1: Write this script to the scratchpad as convert_winding_path.py, never to the repository, and run it with `python3 <scratchpad>/convert_winding_path.py` from the repository root. It holds each stretch value [footfall] holds as a kept entry at the same place, with the save's tick. It drops the [footfall] section and the stepped section, and asserts that no value is lost.

```python
import re

PATH = 'tests/parks/stress/winding-path.park'
KEY = '11741940842860801778'
with open(PATH, newline='') as f:
    text = f.read()
tick = re.search(r'^tick (\d+)$', text, re.M).group(1)
held = re.search(r'\n\[footfall\]\n' + KEY + r' stretches=\[(.*)\]\n', text)
stretches = held.group(1)
entries = re.sub(r'value=([^ }]+)\}', r'value=\1 tick=' + tick + '}', stretches)
assert entries.count(' tick=') == stretches.count('value=') > 0
text = text[:held.start()] + text[held.end():]
text = re.sub(r'\[hungry-footfall-stepped\]\n' + KEY + r' readable=.*\n',
              '[hungry-footfall-kept]\n' + KEY + ' slots=[{source=' + KEY + ' entries=['
              + entries + ']}] pending=[]\n', text)
with open(PATH, 'w', newline='') as f:
    f.write(text)
```

Expected: exits with status 0 and prints nothing.

Step 2: Run: `build/windows-debug/tpj_bench.exe --ticks 1 tests/parks/stress/winding-path.park | head -1`
Expected: a line beginning `park tests/parks/stress/winding-path.park ticks 1`, with no load error.

Step 3 (criterion 8): Run: `git diff -U0 -- tests/parks/stress/winding-path.park | grep -E '^[-+]([^-+]|$)' | grep -v -E '^[-+](\[hungry-footfall-(stepped|kept)\]|\[footfall\]|11741940842860801778 |$)' | wc -l; git show HEAD:tests/parks/stress/winding-path.park | grep -A1 '^\[footfall\]' | grep -o 'value=[^ }]*' | md5sum; grep -A1 '^\[hungry-footfall-kept\]' tests/parks/stress/winding-path.park | grep -o 'value=[^ }]*' | md5sum`
Expected: `0`, then two identical checksums. Put the commands and their output in the feature's report.

### Task 13: Confirm the acceptance criteria

Step 1 (criterion 10): Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i "warning"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -2`
Expected: `0` warnings, `100% tests passed`, and tidy reports no warnings.

Step 2: Run: `git diff --stat main -- tests | grep -v "tests/parks"`
Expected: only tests/sim/guests/footfall_test.cpp and tests/sim/guests/arrivals_test.cpp, from the test pass.

### Task 14: Compare the runtime report

Step 1 (criterion 9): Run: `scripts/runtime-report.sh build/runtime-report/following-footfall-after.txt > build/following-footfall/report-after.log 2>&1; scripts/runtime-report.sh --compare build/runtime-report/following-footfall-before.txt build/runtime-report/following-footfall-after.txt`
Expected: no stress park's ticks line says slower. Put every stress park's ticks lines in the feature's report.

### Task 15: Review and commit

Step 1: Review and handle findings as implementing-features steps 7 and 8 give.

Step 2: Commit the feature once, via the commit-hygiene skill, with `git commit -o` naming only this feature's files: src/sim/guests/SPEC.md, src/legible/SPEC.md, src/sim/guests/footfall.h, src/sim/guests/footfall.cpp, src/sim/guests/internal/footfall.h, the five park files, and the test pass's two test files. Subject: `Guests: Keep footfall only where guests stand`.
