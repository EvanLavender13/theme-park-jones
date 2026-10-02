# Implementation Plan: Full Park

## Goal

Add makeFullPark to tpj_scenarios_lib and `tpj_bench --full-park PATH` to write it, commit tests/parks/stress/full.park, and make the first runtime report on it.

## Approach

makeFullPark applies a fixed list of park commands to the new park's template, each checked with isAccepted first, so a layout mistake stops it with the command named. It then resolves the park, checks every shop has a depot, adds 2,000 guests through addGuest at keyed draws along the guest network's edges, and steps 1,800 ticks. tpj_bench gains one option that saves the result. Everything is a test utility, so there is no test pass, and each criterion is a command run with its output kept for the report.

## Placement

Decision 0027 places each behavior this feature adds:

- The full park's generator: scenarios, src/scenarios/full_park.h and full_park.cpp. The milestone puts it in tpj_scenarios_lib so it builds with the simulation's floating-point flags and gives the same park on every build. Like makeSliceParks, it makes park files from park commands and stepping.
- Writing the full park to a file: bench, src/bench/main.cpp, composition only: parse, makeFullPark, saveWorld, writeTextFile, and a message for each failure. src/scenarios/main.cpp takes no new concern until sound-architecture restructures it, and tpj_bench already reads park files for the stress parks.
- Reading --full-park: bench, src/bench/options.h, beside the rest of tpj_bench's command line.
- Writing a text file in binary mode: bench, src/bench/text_file.h, beside readTextFile.
- bench linking the scenarios library: src/bench/CMakeLists.txt. bench is in a layer above scenarios, so the include is allowed (cmake/layers.txt).
- full.park: tests/parks/stress/, the stress parks' directory, which the cross-build check and the integration tests never read.

## Tasks

### Task 1: Add the full park to the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md`

Step 1: After the paragraph that ends "and running it again takes up a change to the simulation's rules.", insert the text under "src/scenarios/SPEC.md, a new paragraph after makeSliceParks's two:" in plans/scalable-runtime/measured-runtime/full-park/FEATURE.md, without the leading "> " of each line.

### Task 2: Update the bench spec

Files:
- Modify: `src/bench/SPEC.md`

Step 1: Make the four changes listed under "src/bench/SPEC.md:" in FEATURE.md: the link list, the doubled comma, the parseBenchOptions paragraph's fields, refusals, and usage line, and the new Command line paragraph after the section's first, without the leading "> ".

### Task 3: Write the generator

Files:
- Create: `src/scenarios/full_park.h`
- Create: `src/scenarios/full_park.cpp`
- Modify: `src/scenarios/CMakeLists.txt`

Step 1: Write full_park.h with the header guard TPJ_SCENARIOS_FULL_PARK_H, includes `sim/world.h` and `<stdint.h>`, and, in namespace tpj, the declarations under "src/scenarios/full_park.h, new:" in FEATURE.md.

Step 2: Write full_park.cpp:

```cpp
#include "scenarios/full_park.h"

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"

#include <array>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

// The template's guest path, which the full park replaces with its own.
constexpr EntityKey TEMPLATE_PATH{2};
// The backstage rows' z, each serving the shops in the cells beside it.
constexpr std::array<double, 5> ROWS{-80.0, -40.0, 0.0, 40.0, 80.0};
// The paths of each grid direction, 20 m apart from -90 m.
constexpr int GRID_LINES = 10;
// A row's cells, 20 m apart from -80 m; those whose index mod 3 is 2 stay empty.
constexpr int ROW_CELLS = 9;

// Applies the command, or throws naming it when the world refuses it.
template <typename Command>
void applyAccepted(World &world, const Command &command, const std::string &what) {
  if (!isAccepted(world, command)) {
    throw std::logic_error("full park: the world refuses " + what);
  }
  applyCommand(world, command);
}

void addPath(World &world, PathKind kind, ParkPoint from, ParkPoint to) {
  applyAccepted(world, AddPath{kind, {from, to}},
                std::format("the {} path from ({}, {}) to ({}, {})",
                            kind == PathKind::Guest ? "guest" : "backstage", from.X, from.Z, to.X,
                            to.Z));
}

void addBox(World &world, BoxKind kind, Pose at) {
  applyAccepted(world, AddBox{kind, at},
                std::format("the {} at ({}, {})", kind == BoxKind::Shop ? "shop" : "depot", at.X,
                            at.Z));
}

// The coordinate of a grid line or cell: the first plus 20 m per step.
double gridAt(double first, int step) { return first + 20.0 * static_cast<double>(step); }

// The entrance's path, the guest grid, the backstage spine and rows, the shops, and the depots.
void layOut(World &world) {
  applyAccepted(world, DeletePath{TEMPLATE_PATH}, "deleting the template's path");
  addPath(world, PathKind::Guest, {0.0, 123.0}, {0.0, 90.0});
  for (int i = 0; i < GRID_LINES; ++i) {
    const double x = gridAt(-90.0, i);
    addPath(world, PathKind::Guest, {x, -100.0}, {x, 100.0});
  }
  for (int j = 0; j < GRID_LINES; ++j) {
    const double z = gridAt(-90.0, j);
    addPath(world, PathKind::Guest, {-100.0, z}, {100.0, z});
  }
  addPath(world, PathKind::Backstage, {-106.0, -100.0}, {-106.0, 100.0});
  for (const double row : ROWS) {
    addPath(world, PathKind::Backstage, {-106.0, row}, {100.0, row});
  }
  for (const double row : ROWS) {
    for (int k = 0; k < ROW_CELLS; ++k) {
      if (k % 3 != 2) {
        addBox(world, BoxKind::Shop,
               Pose{.X = gridAt(-80.0, k), .Z = row - 4.5, .FacingX = 0.0, .FacingZ = -1.0});
      }
    }
  }
  for (const double z : {-60.0, 0.0, 60.0}) {
    addBox(world, BoxKind::Depot, Pose{.X = -111.5, .Z = z, .FacingX = 1.0, .FacingZ = 0.0});
  }
}

// Throws naming the first shop, in key order, with no depot to supply it.
void refuseStarvedShops(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop && !nearestDepot(world, box.Key)) {
      throw std::logic_error(
          std::format("full park: shop {} is starved", static_cast<uint64_t>(box.Key)));
    }
  }
}

// Adds the guests, each at a keyed draw along the guest network, edges weighed by their length.
void placeGuests(World &world) {
  const std::vector<NetworkEdge> edges = parkNetwork(world, PathKind::Guest).edges();
  std::vector<double> lengths;
  lengths.reserve(edges.size());
  for (const NetworkEdge &edge : edges) {
    lengths.push_back(edge.ToDistance - edge.FromDistance);
  }
  for (uint64_t i = 0; i < FULL_PARK_GUESTS; ++i) {
    const NetworkEdge &edge =
        edges[drawPick(drawKey(world, NULL_KEY, hashName("full-park-edge"), i), lengths)];
    const double along = drawUniform(drawKey(world, NULL_KEY, hashName("full-park-along"), i));
    addGuest(world,
             Place{edge.Carrier, edge.FromDistance + along * (edge.ToDistance - edge.FromDistance)},
             FULL_PARK_WARM_TICKS + FULL_PARK_STAY);
  }
}

} // namespace

World makeFullPark() {
  World world = makeNewPark(FULL_PARK_SEED);
  layOut(world);
  resolveWorld(world);
  refuseStarvedShops(world);
  placeGuests(world);
  for (uint64_t tick = 0; tick < FULL_PARK_WARM_TICKS; ++tick) {
    stepWorld(world);
  }
  return world;
}

} // namespace tpj
```

Step 3: In src/scenarios/CMakeLists.txt, add `full_park.cpp` to tpj_scenarios_lib's sources, between `food_shop.cpp` and `park_edits.cpp`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios_lib`
Expected: the build succeeds with no warnings.

### Task 4: Add writeTextFile

Files:
- Modify: `src/bench/text_file.h`, `src/bench/text_file.cpp`

Step 1: In text_file.h, add `#include <string_view>` and, after readTextFile, the declaration under "src/bench/text_file.h gains:" in FEATURE.md.

Step 2: In text_file.cpp, after readTextFile, add:

```cpp
bool writeTextFile(const std::string &path, std::string_view text) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    return false;
  }
  file.write(text.data(), static_cast<std::streamsize>(text.size()));
  file.close();
  return !file.fail();
}
```

### Task 5: Read --full-park

Files:
- Modify: `src/bench/options.h`, `src/bench/options.cpp`

Step 1: In options.h, add `std::string FullPark;` to BenchOptions after Park, with the comment `// The path --full-park names, or empty.` Change parseBenchOptions's comment to: "Reads tpj_bench [--ticks N] FILE or tpj_bench --full-park PATH, the first argument being the program's name. An argument that begins with -- is an option, and any other is the file. None, with error naming the problem, for an unknown option, --ticks with no value or one that is not a positive decimal count, --full-park with no value or with a file, no file without --full-park, or more than one."

Step 2: In options.cpp, before the `} else if (argument.starts_with("--")) {` branch, add:

```cpp
    } else if (argument == "--full-park") {
      if (i + 1 == arguments.size()) {
        error = "--full-park needs a path";
        return std::nullopt;
      }
      options.FullPark = arguments[++i];
```

and before `if (files.empty()) {` add:

```cpp
  if (!options.FullPark.empty()) {
    if (!files.empty()) {
      error = "--full-park takes no park file";
      return std::nullopt;
    }
    return options;
  }
```

### Task 6: Write the full park from tpj_bench

Files:
- Modify: `src/bench/main.cpp`
- Modify: `src/bench/CMakeLists.txt`

Step 1: In src/bench/CMakeLists.txt, change tpj_bench_lib's link line to `target_link_libraries(tpj_bench_lib PUBLIC tpj_views tpj_legible tpj_render tpj_scenarios_lib tpj_sim)`.

Step 2: In main.cpp, add `#include "scenarios/full_park.h"`, add `//   tpj_bench --full-park PATH` under the usage line of the file comment, change the usage message to `"\nusage: tpj_bench [--ticks N] FILE | --full-park PATH\n"`, and after the options check insert:

```cpp
  if (!options->FullPark.empty()) {
    try {
      if (!tpj::writeTextFile(options->FullPark, tpj::saveWorld(tpj::makeFullPark()))) {
        std::cerr << "tpj_bench: cannot write " << options->FullPark << '\n';
        return 1;
      }
    } catch (const std::exception &failure) {
      std::cerr << "tpj_bench: " << failure.what() << '\n';
      return 1;
    }
    return 0;
  }
```

Step 3: Build both Windows presets' tpj_bench.

Run: `cmake.exe --build --preset windows-release --target tpj_bench && cmake.exe --build --preset windows-debug --target tpj_bench`
Expected: both build with no warnings.

### Task 7: Make full.park

Files:
- Create: `tests/parks/stress/full.park`

Step 1: Generate it with release, timed (criterion 1).

Run: `time build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park; echo "exit $?"`
Expected: `exit 0`. Keep the time for the report. A logic_error names a refused command or a starved shop: stop and follow the deviation procedure, since the layout in the spec is then wrong.

Step 2: Check its contents (criterion 3).

Run:
```
f=tests/parks/stress/full.park
section() { sed -n "/^\[$1\]/,/^\$/p" "$f"; }
ls -la "$f"
echo "guest paths $(section path | grep -c 'kind=guest')  backstage paths $(section path | grep -c 'kind=backstage')"
echo "shops $(section box | grep -c 'kind=shop')  depots $(section box | grep -c 'kind=depot')"
echo "guests $(section guest | grep -c ' at=')  added $(section guest | grep -c 'stay-until=37800')"
echo "queued shops $(section shop-service | grep -c 'queue=\[[0-9]')"
echo "supplies in transit $(section supplies-ledger | grep -o '{arrival=' | wc -l)"
```
Expected: about 1 MB; `guest paths 21  backstage paths 6`; `shops 30  depots 3`; guests at least 2000 with `added 2000`; queued shops at least 1; supplies in transit at least 1.

### Task 8: Check the same text on every build

Run: `build/windows-debug/tpj_bench.exe --full-park build/full-debug.park && cmake --build --preset linux-debug --target tpj_bench && build/linux-debug/tpj_bench --full-park build/full-linux.park && cmp tests/parks/stress/full.park build/full-debug.park && cmp tests/parks/stress/full.park build/full-linux.park && echo identical`
Expected: `identical` (criterion 2). Each debug run may take minutes; keep their times for the report.

### Task 9: Confirm the builds, tests, and checks

Step 1: Windows.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: no warnings; every test passes.

Step 2: Linux and tidy.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, tidy is clean.

Step 3: The cross-build check (criterion 5).

Run: `scripts/cross-build-check.sh`
Expected: it ends with `cross-build-check: both builds wrote the same 36343 lines; passed.`, the count unchanged, since it reads only tests/parks/*.park.

### Task 10: The first report on the full park

Run: `scripts/runtime-report.sh build/runtime-report/full-park-first.txt && scripts/runtime-report.sh --compare build/runtime-report/full-park-first.txt build/runtime-report/full-park-first.txt`
Expected: `runtime-report: wrote ... in <m>m <ss>s`, and a comparison listing every stage of tests/parks/stress/full.park and winding-path.park on both builds, in microseconds, ending `clear 0 of 24`. Put the comparison's lines and the time in the feature's report, with the full park's windows-release food-overlay median beside the 33 ms between ticks. If the report takes too long to wait for, say how long, for Evan's decision on fewer debug ticks.

### Task 11: Commit

Stage everything with `git add -A && git reset -q parks/`, so the untracked parks/routes.park and parks/sketch.park stay out. Commit once via the commit-hygiene skill, subject `Scenarios: Add the full park and write it from tpj_bench`.
