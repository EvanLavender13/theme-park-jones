# Implementation Plan: Cross-Build Check

## Goal

Add tpj_scenarios, which prints per-tick hashes of synthetic scenarios and park files plus the simulation's exp, log, and draw outputs, and a WSL script, run by pre-push when the simulation's inputs change, that compares its Linux and Windows outputs line by line.

## Approach

The scenarios and the output functions live in a static library, tpj_scenarios_lib, so the test pass can call them. The executable adds only a main that parses options, reads the clock, and writes timings to standard error. Every output line names its source and tick, so comparing is a first-differing-line search: a C++ function behind --compare, which the tests check on both presets. The script builds only the tpj_scenarios target with each preset, strips the Windows side's carriage returns, and calls the Linux runner's --compare. The pre-push hook reads the pushed ranges from standard input and runs the script only when git log shows a trigger path (RESEARCH.md).

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: At the end of the paragraph beginning "Component types are registered with a WorldSchema", append the three sentences in FEATURE.md's "Spec changes" section for src/sim/SPEC.md, exactly.

### Task 2: Write the scenarios spec

Files:
- Create: `src/scenarios/SPEC.md`

Step 1: Create the file with the text in FEATURE.md's "Spec changes" section for src/scenarios/SPEC.md, exactly. Replace its single quotes around line forms with backticks. Write the heading as `# scenarios` and the section heading as `## Contract`.

### Task 3: Add the park schema

Files:
- Create: `src/sim/park_schema.h`
- Create: `src/sim/park_schema.cpp`
- Modify: `src/sim/CMakeLists.txt:3-11`

Step 1: Create the header.

```cpp
#ifndef TPJ_SIM_PARK_SCHEMA_H
#define TPJ_SIM_PARK_SCHEMA_H

#include "sim/schema.h"

#include <memory>

namespace tpj {

// The park's schema: every capability's component types, systems, swap functions, resolvers, and
// commands, registered here in a written order. Park files are loaded with it.
std::shared_ptr<const WorldSchema> makeParkSchema();

} // namespace tpj

#endif
```

Step 2: Create the source. It is final, not a stub.

```cpp
#include "sim/park_schema.h"

namespace tpj {

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  // Capabilities register their park types here as they land, in dependency order.
  return schema;
}

} // namespace tpj
```

Step 3: Add `park_schema.cpp` to tpj_sim's source list, between `musl_log.cpp` and `save.cpp`.

### Task 4: Declare the scenarios and the runner

Files:
- Create: `src/scenarios/scenarios.h`
- Create: `src/scenarios/runner.h`

Step 1: Create `src/scenarios/scenarios.h`.

```cpp
#ifndef TPJ_SCENARIOS_SCENARIOS_H
#define TPJ_SCENARIOS_SCENARIOS_H

#include "sim/command_queue.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <span>
#include <stdint.h>
#include <string_view>

namespace tpj {

// A synthetic world for the scenario runner: its schema and seed, how to fill a new world, and the
// commands it queues before each cycle.
struct Scenario {
  // Lowercase letters, digits, and hyphens, distinct among the registered scenarios.
  std::string_view Name;
  uint64_t Seed = 0;
  std::shared_ptr<const WorldSchema> (*MakeSchema)() = nullptr;
  void (*Populate)(World &world) = nullptr;
  // Queues the commands for the cycle about to run on the world. Null when the scenario queues none.
  void (*QueueCommands)(const World &world, CommandQueue &commands) = nullptr;
};

// The registered scenarios, in a written order.
std::span<const Scenario> registeredScenarios();

} // namespace tpj

#endif
```

Step 2: Create `src/scenarios/runner.h`.

```cpp
#ifndef TPJ_SCENARIOS_RUNNER_H
#define TPJ_SCENARIOS_RUNNER_H

#include "scenarios/scenarios.h"
#include "sim/schema.h"

#include <iosfwd>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string_view>

namespace tpj {

// Makes, populates, and resolves the scenario's world, then writes its hash line and, for each of
// ticks cycles, steps it with the commands the scenario queues and writes the line again:
//   scenario <name> tick <t> hash <h>
void runScenario(const Scenario &scenario, uint64_t ticks, std::ostream &out);

// The same for a save: loads text with schema, resolves, and steps with no commands, writing
//   file <label> tick <t> hash <h>
// Throws LoadError, as loadWorld does.
void runSave(std::string_view label, std::shared_ptr<const WorldSchema> schema,
             std::string_view text, uint64_t ticks, std::ostream &out);

// simExp and simLog for every argument of sim-math's reference tables, as bits:
//   exp argument <a> result <r>
//   log argument <a> result <r>
void writeSimMathLines(std::ostream &out);

// The draws for every key of keyed-draws' expected-draw table:
//   draw <i> bits <b> uniform <u> integer-pick <p> double-pick <q>
void writeDrawLines(std::ostream &out);

// Where two outputs first differ: a 1-based line number, and each output's line there, or none
// after its end.
struct LineDifference {
  size_t Line = 0;
  std::optional<std::string_view> Left;
  std::optional<std::string_view> Right;
};

// Empty when both texts hold the same lines. A line ends at a line feed, and a last line may have
// none. Lines are compared byte for byte.
std::optional<LineDifference> firstDifference(std::string_view left, std::string_view right);

} // namespace tpj

#endif
```

### Task 5: Stub the library and the executable, and build them

Files:
- Create: `src/scenarios/scenarios.cpp`
- Create: `src/scenarios/runner.cpp`
- Create: `src/scenarios/main.cpp`
- Create: `src/scenarios/CMakeLists.txt`
- Modify: `CMakeLists.txt:43-44`

Step 1: Create `src/scenarios/scenarios.cpp` as a stub that registers no scenario, replaced in Task 9.

```cpp
#include "scenarios/scenarios.h"

namespace tpj {

std::span<const Scenario> registeredScenarios() { return {}; }

} // namespace tpj
```

Step 2: Create `src/scenarios/runner.cpp` as a stub whose functions write nothing and find no difference, replaced in Tasks 10 and 11. The definitions match runner.h's declarations, with unnamed parameters commented as in sim-math's stubs, and firstDifference returns `std::nullopt`.

Step 3: Create `src/scenarios/main.cpp` as a stub returning 0 from `int main(int, char **)`, replaced in Task 12.

Step 4: Create `src/scenarios/CMakeLists.txt`.

```cmake
# The scenario runner (decision 0022). The library holds the scenarios and the output functions,
# and is simulation code: it builds with tpj_sim's floating-point flags, and only main reads the
# clock. It reads the generated draw and math tables in tests/sim/support.
add_library(tpj_scenarios_lib STATIC
    beacons.cpp
    runner.cpp
    scenarios.cpp
    walkers.cpp)
target_include_directories(tpj_scenarios_lib PUBLIC ${CMAKE_SOURCE_DIR}/src PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tpj_scenarios_lib PUBLIC tpj_sim)
tpj_configure_target(tpj_scenarios_lib)
target_compile_options(tpj_scenarios_lib PRIVATE -ffp-contract=off)

add_executable(tpj_scenarios main.cpp)
target_link_libraries(tpj_scenarios PRIVATE tpj_scenarios_lib)
tpj_configure_target(tpj_scenarios)
```

Step 5: Create `src/scenarios/walkers.cpp` and `src/scenarios/beacons.cpp` holding only `#include "scenarios/synthetic.h"`, and create `src/scenarios/synthetic.h`, the library's internal header:

```cpp
#ifndef TPJ_SCENARIOS_SYNTHETIC_H
#define TPJ_SCENARIOS_SYNTHETIC_H

#include "scenarios/scenarios.h"

namespace tpj {

Scenario walkersScenario();
Scenario beaconsScenario();

} // namespace tpj

#endif
```

Step 6: In the top-level `CMakeLists.txt`, add `add_subdirectory(src/scenarios)` directly after `add_subdirectory(src/sim)`, outside the TPJ_BUILD_APP block, so every preset builds it.

Step 7: Build and test.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 92 existing tests pass.

### Task 6: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/scenarios/SPEC.md, src/sim/SPEC.md, and the public headers src/scenarios/scenarios.h, src/scenarios/runner.h, and src/sim/park_schema.h. It creates tests/scenarios/ with the tpj_scenarios_tests executable and the tests that run tpj_scenarios itself. It registers the symbol-check test on tpj_scenarios_lib's archive, and adds `add_subdirectory(scenarios)` to tests/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 92 existing tests pass. The new tests of lines, draws changing with the seed, differences between unequal texts, and the executable's output fail against the stubs. Tests that hold vacuously may pass against them: the checks on scenario names, a repeated run's identity, equal texts giving no difference, compare mode's status 0 on equal files, and the symbol check on the scenarios library.

### Task 7: The walkers scenario

Files:
- Modify: `src/scenarios/walkers.cpp`

Step 1: In an anonymous namespace inside `tpj`, define the state component and its fields.

```cpp
struct Walker {
  double Position = 0.0;
  double Energy = 0.0;
  uint32_t Age = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Walker &walker) {
  visitor.field("position", walker.Position);
  visitor.field("energy", walker.Energy);
  visitor.field("age", walker.Age);
}
```

Step 2: Define the constants: `STEP_PURPOSE = hashName("walker-step")`, `REST_PURPOSE = hashName("walker-rest")`, `SPAWN_PURPOSE = hashName("walker-spawn")`, `HOME = 8.0`, `LIFETIME = 400` (uint32_t), `SPAWN_INTERVAL = 25` (uint64_t), and `START_WALKERS = 16` (int).

Step 3: Write the system `stepWalkers(World &world)`. For each key of `world.keys()`, in order, that holds a Walker:
- Score the three steps -0.5, 0, and +0.5 as `-std::fabs(walker.Position + step - HOME) * walker.Energy`.
- Set each weight to `simExp(score - highest score)`.
- Pick with `drawPick(drawKey(world, key, STEP_PURPOSE, 0), weights)`, where weights is a `std::array<double, 3>` passed as a span, and move Position by the picked step.
- Set Energy to `walker.Energy * 0.99 + 0.05 * simLog(1.0 + drawUniform(drawKey(world, key, REST_PURPOSE, 0)))`.
- Add 1 to Age.

Step 4: Write the system `turnOverWalkers(World &world)`. Collect the keys of walkers whose Age is at least LIFETIME, then destroyEntity each. Then, when `world.Tick % SPAWN_INTERVAL == 0`, create an entity and give it `Walker{.Position = 16.0 * drawUniform(drawKey(world, NULL_KEY, SPAWN_PURPOSE, 0)), .Energy = 1.0, .Age = 0}`.

Step 5: Write `walkersScenario()` outside the anonymous namespace, returning:
- Name `walkers` and Seed `1001`.
- MakeSchema: a lambda that registers `Walker` as `"walker"`, DataKind::State, then adds the systems stepWalkers and turnOverWalkers, in that order.
- Populate: a lambda that creates START_WALKERS entities, where walker i gets `Walker{.Position = 0.5 * i, .Energy = 1.0 + 0.125 * i, .Age = 0}`.
- QueueCommands: null.

Includes: `scenarios/synthetic.h`, then `sim/draw.h`, `sim/entity_key.h`, `sim/mix.h`, `sim/schema.h`, `sim/sim_math.h`, and `sim/world.h`, then `<algorithm>`, `<array>`, `<cmath>`, `<memory>`, `<stdint.h>`, and `<vector>`.

Run: `cmake --build --preset linux-debug`
Expected: the build succeeds with no warnings.

### Task 8: The beacons scenario

Files:
- Modify: `src/scenarios/beacons.cpp`

Step 1: In an anonymous namespace inside `tpj`, define the components, each with a visitFields listing its fields in declaration order with lowercase names (`x`, `strength`, `level`, `total`, `glows`):

```cpp
// Intent: a beacon the command placed.
struct Beacon {
  double X = 0.0;
  uint32_t Strength = 0;
};
// Derived: one glow per unit of a beacon's strength, on entities keyed from the beacon.
struct Glow {
  double Level = 0.0;
};
// State: the running sum of glow levels over time.
struct Tally {
  double Total = 0.0;
  uint64_t Glows = 0;
};
```

Step 2: Define the command and its applyCommand in the same anonymous namespace, so argument-dependent lookup finds it:

```cpp
struct PlaceBeacon {
  double X = 0.0;
  uint32_t Strength = 0;
};

void applyCommand(World &world, const PlaceBeacon &command) {
  const EntityKey key = world.createEntity();
  world.Registry.emplace<Beacon>(world.findEntity(key),
                                 Beacon{.X = command.X, .Strength = command.Strength});
}
```

Step 3: Write the resolver `resolveGlows(World &world)`, with `GLOW_PURPOSE = hashName("beacon-glow")`. For each key of `world.keys()` that holds a Beacon, and for each i from 0 below its Strength, call `world.createDerivedEntity(key, GLOW_PURPOSE, i)` and `emplace_or_replace<Glow>` on that entity with `Level = std::sqrt(beacon.X * beacon.X + 1.0) / static_cast<double>(i + 1)`. Copy the Beacon's values before creating entities, since creation may move the registry's storage.

Step 4: Write the system `tallyGlows(World &world)`. Sum `glow.Level * SIM_TICK_SECONDS` over the keys of `world.keys()` that hold a Glow, in key order, and count them. Then add the sum to the Total of the one entity holding a Tally, and set its Glows to the count.

Step 5: Write `beaconsScenario()` outside the anonymous namespace, returning:
- Name `beacons` and Seed `2002`.
- MakeSchema: a lambda that registers Beacon as `"beacon"`, DataKind::Intent, Glow as `"glow"`, DataKind::Derived, and Tally as `"tally"`, DataKind::State. It then adds the system tallyGlows, the resolver `"glows"` with resolveGlows and no dependencies, and the command PlaceBeacon.
- Populate: a lambda that creates an entity holding `Tally{}`, then one holding `Beacon{.X = 1.5, .Strength = 2}`.
- QueueCommands: a lambda that, when `world.Tick % 100 == 50`, pushes `PlaceBeacon{.X = static_cast<double>(world.Tick) / 7.0, .Strength = static_cast<uint32_t>(1 + (world.Tick / 100) % 4)}`.

Beacons are never removed and their strengths never change, so resolution never leaves a glow behind.

Run: `cmake --build --preset linux-debug`
Expected: the build succeeds with no warnings.

### Task 9: Register the scenarios

Files:
- Modify: `src/scenarios/scenarios.cpp` (replace the stub)

Step 1: Include `scenarios/synthetic.h` and `<array>`. Make registeredScenarios return a span over a function-local `static const std::array<Scenario, 2>` holding `walkersScenario()` and then `beaconsScenario()`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug -R scenario`
Expected: the build succeeds with no warnings. The tests of scenario names and of draws changing with the seed pass.

### Task 10: The runner's lines

Files:
- Modify: `src/scenarios/runner.cpp` (replace the stubs of runScenario, runSave, writeSimMathLines, and writeDrawLines)

Step 1: In an anonymous namespace, write `std::string hex16(uint64_t value)`, which returns value as exactly 16 lowercase hexadecimal digits, zero-padded, using std::to_chars with base 16. Write `void writeHashLine(std::ostream &out, std::string_view source, std::string_view label, const World &world)`, which writes `<source> <label> tick <world.Tick> hash <hex16(hashWorld(world))>\n`.

Step 2: runScenario constructs `World world(scenario.MakeSchema(), scenario.Seed)`, calls `scenario.Populate(world)` and `resolveWorld(world)`, and writes the hash line with source `scenario` and the scenario's name. Then, ticks times, it makes a CommandQueue, calls QueueCommands(world, queue) if it is not null, calls `stepWorld(world, queue)`, and writes the line again.

Step 3: runSave calls `loadWorld(std::move(schema), text)`, then `resolveWorld`, and writes the line with source `file` and the label. Then, ticks times, it calls `stepWorld(world)` and writes the line again.

Step 4: writeSimMathLines writes, for each entry of `test::EXP_REFERENCE`, `exp argument <hex16 of the argument's bits> result <hex16 of simExp(argument)'s bits>\n`, using std::bit_cast. It then does the same for `test::LOG_REFERENCE` with `log` and simLog. Include `"tests/sim/support/sim_math_reference.h"`.

Step 5: writeDrawLines writes, for each index i of `test::EXPECTED_DRAWS`, `draw <i> bits <hex16(drawBits(key))> uniform <hex16 of drawUniform(key)'s bits> integer-pick <drawPick(key, test::EXPECTED_DRAW_INTEGER_WEIGHTS)> double-pick <drawPick(key, test::EXPECTED_DRAW_DOUBLE_WEIGHTS)>\n`. Include `"tests/sim/support/expected_draws.h"`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings. Every new test passes except those of firstDifference and of the executable.

### Task 11: Finding the first difference

Files:
- Modify: `src/scenarios/runner.cpp` (replace the stub of firstDifference)

Step 1: In the anonymous namespace, write `std::vector<std::string_view> splitLines(std::string_view text)`. It returns the pieces of text ending at each line feed, without the line feed, plus the remainder if it is not empty. So an empty text has no lines, and a text ending in a line feed has no empty last line.

Step 2: firstDifference splits both texts. For each 0-based i below the larger count, when either text has no line i or the two lines differ, it returns `LineDifference{.Line = i + 1, .Left = the left line or std::nullopt, .Right = the right line or std::nullopt}`. It returns std::nullopt when no line differs.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings. Every new test passes except those of the executable.

### Task 12: The executable

Files:
- Modify: `src/scenarios/main.cpp` (replace the stub)

Step 1: Write `std::optional<std::string> readFile(const char *path)`, which reads the whole file in binary mode, or returns std::nullopt if it cannot be opened or read.

Step 2: Write the compare mode. When argv[1] is `--compare`, exactly two more arguments must follow. Otherwise print `tpj_scenarios: --compare takes two files` to standard error and return 2. Read both files, and for one that cannot be read, print `tpj_scenarios: cannot read <path>` and return 2. When firstDifference finds nothing, return 0. Otherwise print to standard output:
```
<left> and <right> first differ at line <k>:
<left>: <left line, or (no line)>
<right>: <right line, or (no line)>
```
and return 1.

Step 3: Write the run mode. Parse the arguments in order:
- `--ticks` followed by a value that std::from_chars reads in full as a uint64_t sets the tick count, which defaults to 3000.
- A missing or malformed value prints `tpj_scenarios: --ticks takes a decimal count` and returns 1.
- Any other argument starting with `--` prints `tpj_scenarios: unknown option <argument>` and returns 1.
- Every other argument is a park file path, kept in order.

Step 4: Write the runs. Time each with std::chrono::steady_clock:
- Run each of registeredScenarios() with runScenario to std::cout.
- Then, for each path, read the file and run it with `runSave(path, makeParkSchema(), text, ticks, std::cout)`. A file that cannot be read prints `tpj_scenarios: cannot read <path>` and returns 1.
- After each run, write `<source> <name or path>: <ticks> ticks in <seconds, fixed with 3 decimals> s, <ticks per second, fixed with 0 decimals> ticks per second` to std::cerr, where source is `scenario` or `file`. When the elapsed time is zero, write 0 ticks per second.
- Then call writeSimMathLines and writeDrawLines on std::cout, and return 0.

Step 5: Wrap the runs in a try block. Catch LoadError while running a file and print `tpj_scenarios: <path>: <what()>`, which names the line, and catch any other std::exception and print `tpj_scenarios: <what()>`. Either way, return 1.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and every test passes.

### Task 13: The cross-build script

Files:
- Create: `scripts/cross-build-check.sh` (executable: `chmod +x`, and `git update-index --chmod=+x` once it is staged)

Step 1: Write the script with this behavior, in this order. Each stage message is printed as `cross-build-check: <message>` before the stage starts.
1. A header comment says what the script checks (decision 0022), that it runs from WSL at the repository root, and that pre-push runs it when the simulation's inputs change. The script uses `set -uo pipefail`.
2. When not on Linux, or when `cmake.exe` is not on the PATH, print `cross-build-check: run this from WSL with cmake.exe on the PATH` and exit 1.
3. Change to `git rev-parse --show-toplevel`.
4. For linux-debug with `cmake`, then windows-debug with `cmake.exe`, configure with `--preset <preset>` if `build/<preset>/CMakeCache.txt` is missing, with stage `configuring <preset>`. Then build with `--build --preset <preset> --target tpj_scenarios`, with stage `building tpj_scenarios with <preset>`. A failure prints `cross-build-check: <preset> failed to configure` or `to build` and exits 1.
5. Collect `tests/parks/*.park` with nullglob, in the glob's name order.
6. With stage `running linux-debug over the scenarios and <count> park files`, run `build/linux-debug/tpj_scenarios <parks>` into `build/cross-build-check/linux-debug.txt`, making the directory first. With stage `running windows-debug`, run `build/windows-debug/tpj_scenarios.exe <parks> | tr -d '\r'` into `build/cross-build-check/windows-debug.txt`. A nonzero status from either prints `cross-build-check: the <preset> run failed` and exits 1. The runners' standard error, with the timings, passes through to the terminal.
7. With stage `comparing`, run `build/linux-debug/tpj_scenarios --compare` on the two files. Status 1 prints `cross-build-check: the builds differ; see the first differing line above` and exits 1. Any other nonzero status prints `cross-build-check: the comparison failed` and exits 1.
8. Print `cross-build-check: both builds wrote the same <line count> lines; passed.` and exit 0.

Step 2: Run it.

Run: `scripts/cross-build-check.sh`
Expected: each stage message in order, two timing lines per build (walkers and beacons), and `cross-build-check: both builds wrote the same 6333 lines; passed.` That is 2 × 3001 hash lines, 169 exp lines, 152 log lines, and 10 draw lines, one per entry of EXPECTED_DRAWS.

### Task 14: Run the script from pre-push

Files:
- Modify: `.githooks/pre-push`

Step 1: Extend the header comment. After the linux-debug checks, the hook runs scripts/cross-build-check.sh when the pushed commits touch the simulation's build inputs.

Step 2: Directly after the `uname` check, read the pushed refs before anything else can consume standard input: `PUSHED_REFS=$(cat)`.

Step 3: Replace the final `echo "pre-push: all checks passed; pushing."` with this logic:
- Set `CROSS_BUILD_INPUTS='^(src/sim/|src/core/|src/scenarios/|cmake/|CMakeLists\.txt$|CMakePresets\.json$|tests/parks/|tests/sim/support/|scripts/cross-build-check\.sh$)'`.
- For each line `local_ref local_sha remote_ref remote_sha` of PUSHED_REFS, skip empty lines and deletions, where local_sha is all zeros.
- List the touched files with `git log --format= --name-only "$remote_sha..$local_sha"`, or with `git log --format= --name-only "$local_sha" --not --remotes` when remote_sha is all zeros.
- The check is needed when git log fails, or when any listed file matches CROSS_BUILD_INPUTS under `grep -qE`.
- When it is needed, print `pre-push: the pushed commits touch the simulation's build inputs; running the cross-build check...` and run `scripts/cross-build-check.sh`. On failure, print `pre-push: the cross-build check failed; push aborted.` and exit 1.
- Otherwise print `pre-push: the pushed commits touch none of the simulation's build inputs; skipping the cross-build check.`
- Finally print `pre-push: all checks passed; pushing.` and exit 0.

Step 4: Check the trigger both ways. Feed the hook the stdin git would give.

Run: `printf 'refs/heads/main %s refs/heads/main %s\n' "$(git rev-parse cde078f)" "$(git rev-parse cde078f^)" | .githooks/pre-push`
Expected: the linux-debug checks pass, then `pre-push: the pushed commits touch none of the simulation's build inputs; skipping the cross-build check.` cde078f touches only plans.

Run: `printf 'refs/heads/main %s refs/heads/main %s\n' "$(git rev-parse HEAD)" "$(git rev-parse origin/main)" | .githooks/pre-push`
Expected: the linux-debug checks pass, then the running message, the script's stages, and `pre-push: all checks passed; pushing.`

### Task 15: Point to the check, and resolve keyed-draws' question

Files:
- Modify: `CLAUDE.md` (the paragraph beginning "A change is finished when")
- Modify: `plans/deterministic-simulation/world-as-value/keyed-draws/FEATURE.md` (Open questions)

Step 1: In CLAUDE.md, after the sentence on Git hooks, add: "scripts/cross-build-check.sh compares the Windows and Linux builds' simulation outputs (decision 0022), and pre-push runs it when the simulation's inputs change."

Step 2: In keyed-draws' FEATURE.md, delete the open question beginning "Where the expected-draw table lives". It stays in tests/sim/support, and tpj_scenarios prints the draws for its keys.

### Task 16: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Step 3: Show that the script fails on a divergence. Temporarily change walkers' Seed in `src/scenarios/walkers.cpp` to `1001 + TPJ_SEED_SKEW`, with `#ifdef _WIN32` defining `TPJ_SEED_SKEW` as 1 and otherwise 0, and run the script.

Run: `scripts/cross-build-check.sh; echo "status $?"`
Expected: the report `build/cross-build-check/linux-debug.txt and build/cross-build-check/windows-debug.txt first differ at line 1:`, with the two `scenario walkers tick 0 hash` lines, then `cross-build-check: the builds differ; see the first differing line above` and `status 1`.

Step 4: Revert the change, then rerun the script and expect it to pass.

Run: `grep -c TPJ_SEED_SKEW src/scenarios/walkers.cpp; scripts/cross-build-check.sh`
Expected: `0`, and the script passes as in Task 13.

### Task 17: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Scenarios: Add tpj_scenarios and the cross-build check`.
