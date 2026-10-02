# Implementation Plan: Stage Runner

## Goal

Add src/views with the food overlay as the app shows it, switch the app to it, and add src/bench with tpj_bench, which times each runtime stage on a park file and writes each stage's times beside its result, with lowfps.park moved to tests/parks/stress/winding-path.park.

## Approach

views is one function that joins render's buildFoodOverlay to legible's foodAvailability, moved out of the app's scene uploads so the app and the runner call the same code. bench is a library of four small components, the time summary, the stages, the options, and reading a park file, plus a main that only composes them. Each stage calls its work through public headers, reading std::chrono::steady_clock immediately around each call, and keeps the output past the clock so the result written beside the times proves the work was done.

## Placement

Decision 0027 places each behavior this feature adds:

- The food overlay as the app shows it, joining render's band to legible's availability: views, src/views/food_overlay.h. render and legible share a layer and may not include each other, and app may not be included by anything, so the join needs a unit above both and below app that the app and the runner can both include.
- The app building its overlay: app, src/app/scene/scene_uploads.cpp, unchanged in role. Its foodOverlayMesh calls views instead of joining render and legible itself.
- Summarizing a stage's durations: bench, src/bench/timing.h. It is the only arithmetic on times, and runtime-report's summarizing of launches will sit beside it.
- Timing each stage and formatting its line: bench, src/bench/stages.h. It owns what a stage is, how it is timed, and what its result is.
- Reading tpj_bench's command line: bench, src/bench/options.h, so it is tested through a header rather than by running the executable.
- Reading a park file's text: bench, src/bench/park_file.h. bench may not include the app's session/park_file.h (decision 0027), and the read is three lines.
- tpj_bench's entry point: bench, src/bench/main.cpp, composition only: parse, read, load, benchPark, write, and turn each failure into a message and a status.
- The layers: cmake/layers.txt gains views and bench, each on its own line.
- winding-path.park: tests/parks/stress/, the stress parks' directory, which the cross-build check's tests/parks/*.park glob and the integration tests' non-recursive listing never reach.

## Tasks

### Task 1: Write the views spec

Files:
- Create: `src/views/SPEC.md`

Step 1: Write the text under "src/views/SPEC.md, new:" in plans/scalable-runtime/measured-runtime/stage-runner/FEATURE.md, without the leading "> " of each line.

### Task 2: Write the bench spec

Files:
- Create: `src/bench/SPEC.md`

Step 1: Write the text under "src/bench/SPEC.md, new:" in FEATURE.md, without the leading "> " of each line.

### Task 3: Point the render and app specs at views

Files:
- Modify: `src/render/SPEC.md` (Food overlay, first paragraph)
- Modify: `src/app/SPEC.md` (Tooling UI, the Debug panel paragraph)

Step 1: In src/render/SPEC.md replace "which the app makes from foodAvailability's Value (legible/SPEC.md)" with "which views makes from foodAvailability's Value (views/SPEC.md, legible/SPEC.md)".

Step 2: In src/app/SPEC.md replace "it gives the renderer setOverlayMesh of buildFoodOverlay for previewedWorld of the world and the kept preview, shaded by foodAvailability's Value at each place in it, while the checkbox is checked" with "it gives the renderer setOverlayMesh of buildFoodAvailabilityOverlay for previewedWorld of the world and the kept preview (views/SPEC.md), while the checkbox is checked".

### Task 4: Name the stress parks' directory in the conventions

Files:
- Modify: `docs/conventions.md` (Tests)

Step 1: Replace "and checked-in park files in tests/parks/." with "and checked-in park files in tests/parks/, with the parks only tpj_bench runs in tests/parks/stress/."

### Task 5: Add views and bench to the layer table

Files:
- Modify: `cmake/layers.txt`

Step 1: After the line `tools render legible scenarios`, insert two lines, so the table's layers read:

```
core
sim/*
sim/medium
sim/park
sim/routes
sim/operations
sim/guests
sim/park_schema
tools render legible scenarios
views
bench
app/session app/input
app/scene app/ui
app/*
```

### Task 6: Declare views' interface

Files:
- Create: `src/views/food_overlay.h`
- Create: `src/views/food_overlay.cpp`
- Create: `src/views/CMakeLists.txt`
- Modify: `CMakeLists.txt` (the `if(TPJ_BUILD_APP)` block)

Step 1: src/views/food_overlay.h:

```cpp
#ifndef TPJ_VIEWS_FOOD_OVERLAY_H
#define TPJ_VIEWS_FOOD_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/world.h"

namespace tpj {

// The food overlay as the app shows it: render's band along each guest path, shaded by the food
// availability at each place. Changes nothing in the world.
ParkMesh buildFoodAvailabilityOverlay(const World &world);

} // namespace tpj

#endif
```

Step 2: src/views/food_overlay.cpp, a stub:

```cpp
#include "views/food_overlay.h"

namespace tpj {

ParkMesh buildFoodAvailabilityOverlay(const World & /*world*/) { return {}; }

} // namespace tpj
```

Step 3: src/views/CMakeLists.txt:

```cmake
# What the app shows, joined from render's mesh builders and legible's explanations, which share a
# layer and so cannot join themselves (decision 0027). Built on the CPU, so tests need no GPU.
add_library(tpj_views STATIC
    food_overlay.cpp)
target_include_directories(tpj_views PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_views PUBLIC tpj_legible tpj_render tpj_sim)
tpj_configure_target(tpj_views)
```

Step 4: In CMakeLists.txt, inside `if(TPJ_BUILD_APP)`, replace

```cmake
    add_subdirectory(src/render)
    add_subdirectory(src/app)
```

with

```cmake
    add_subdirectory(src/render)
    add_subdirectory(src/views)
    add_subdirectory(src/bench)
    add_subdirectory(src/app)
```

### Task 7: Declare bench's interface

Files:
- Create: `src/bench/timing.h`, `src/bench/timing.cpp`
- Create: `src/bench/stages.h`, `src/bench/stages.cpp`
- Create: `src/bench/options.h`, `src/bench/options.cpp`
- Create: `src/bench/park_file.h`, `src/bench/park_file.cpp`
- Create: `src/bench/main.cpp`
- Create: `src/bench/CMakeLists.txt`

Step 1: src/bench/timing.h:

```cpp
#ifndef TPJ_BENCH_TIMING_H
#define TPJ_BENCH_TIMING_H

#include <span>
#include <stddef.h>
#include <stdint.h>

namespace tpj {

// A stage's counted durations, in nanoseconds.
struct TimeSummary {
  size_t Count = 0;
  // The element at index (Count - 1) / 2 of the durations sorted ascending: of an even count, the
  // lower of the two middle ones.
  int64_t Median = 0;
  int64_t Least = 0;
  int64_t Greatest = 0;

  bool operator==(const TimeSummary &) const = default;
};

// Summarizes durations in nanoseconds. Throws std::invalid_argument for an empty list.
TimeSummary summarizeTimes(std::span<const int64_t> nanoseconds);

} // namespace tpj

#endif
```

src/bench/timing.cpp, a stub:

```cpp
#include "bench/timing.h"

namespace tpj {

TimeSummary summarizeTimes(std::span<const int64_t> /*nanoseconds*/) { return {}; }

} // namespace tpj
```

Step 2: src/bench/stages.h:

```cpp
#ifndef TPJ_BENCH_STAGES_H
#define TPJ_BENCH_STAGES_H

#include "bench/timing.h"
#include "sim/world.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// The ticks tpj_bench steps without --ticks.
inline constexpr uint64_t DEFAULT_TICKS = 300;
// The untimed calls a repeated stage makes before its counted ones.
inline constexpr size_t WARM_UPS = 2;
// The counted calls of every stage but ticks.
inline constexpr size_t REPETITIONS = 11;

// What a stage's result is: a world's hash, or a mesh's vertex count.
enum class ResultKind { Hash, Vertices };

// One stage's times and the result of its work.
struct StageResult {
  std::string Name;
  TimeSummary Times;
  ResultKind Kind = ResultKind::Hash;
  uint64_t Result = 0;
};

// Times resolution, preview (only when the resolved park holds a box), food-overlay, park-mesh,
// guest-mesh, and ticks, in that order, on a loaded world, which it leaves unchanged. Throws
// std::invalid_argument when ticks is 0, and std::runtime_error naming the box when the preview of
// moving the lowest-keyed box to its own pose has no candidate.
std::vector<StageResult> benchPark(const World &loaded, uint64_t ticks);

// stage <name> count <c> median <m> least <l> greatest <g> <hash|vertices> <result>, with a hash
// as 16 lowercase hexadecimal digits and a vertex count in decimal, and no line feed.
std::string stageLine(const StageResult &stage);

// park <path> ticks <t> warm-ups <w> repetitions <r>, with no line feed.
std::string parkLine(std::string_view path, uint64_t ticks);

} // namespace tpj

#endif
```

src/bench/stages.cpp, a stub:

```cpp
#include "bench/stages.h"

namespace tpj {

std::vector<StageResult> benchPark(const World & /*loaded*/, uint64_t /*ticks*/) { return {}; }

std::string stageLine(const StageResult & /*stage*/) { return {}; }

std::string parkLine(std::string_view /*path*/, uint64_t /*ticks*/) { return {}; }

} // namespace tpj
```

Step 3: src/bench/options.h:

```cpp
#ifndef TPJ_BENCH_OPTIONS_H
#define TPJ_BENCH_OPTIONS_H

#include "bench/stages.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <string>

namespace tpj {

// What tpj_bench's command line asks for.
struct BenchOptions {
  uint64_t Ticks = DEFAULT_TICKS;
  std::string Park;

  bool operator==(const BenchOptions &) const = default;
};

// Reads tpj_bench [--ticks N] FILE, the first argument being the program's name. An argument that
// begins with -- is an option, and any other is the file. None, with error naming the problem, for
// an unknown option, --ticks with no value or one that is not a positive decimal count, no file,
// or more than one.
std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error);

} // namespace tpj

#endif
```

src/bench/options.cpp, a stub:

```cpp
#include "bench/options.h"

namespace tpj {

std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> /*arguments*/,
                                              std::string & /*error*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 4: src/bench/park_file.h:

```cpp
#ifndef TPJ_BENCH_PARK_FILE_H
#define TPJ_BENCH_PARK_FILE_H

#include <optional>
#include <string>

namespace tpj {

// The file's whole text, read in binary mode, or none when it cannot be read.
std::optional<std::string> readParkFile(const std::string &path);

} // namespace tpj

#endif
```

src/bench/park_file.cpp, a stub:

```cpp
#include "bench/park_file.h"

namespace tpj {

std::optional<std::string> readParkFile(const std::string & /*path*/) { return std::nullopt; }

} // namespace tpj
```

Step 5: src/bench/main.cpp, a stub:

```cpp
// tpj_bench: times each stage of the gameplay runtime on a park file, writing each stage's times
// beside its result.
//
//   tpj_bench [--ticks N] FILE

int main() { return 1; }
```

Step 6: src/bench/CMakeLists.txt:

```cmake
# The runtime runner (plans/scalable-runtime). It reads the clock to time each runtime stage on a
# park file and is not simulation code, so it builds with the project's usual flags.
add_library(tpj_bench_lib STATIC
    options.cpp
    park_file.cpp
    stages.cpp
    timing.cpp)
target_include_directories(tpj_bench_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_bench_lib PUBLIC tpj_views tpj_legible tpj_render tpj_sim)
tpj_configure_target(tpj_bench_lib)

add_executable(tpj_bench main.cpp)
target_link_libraries(tpj_bench PRIVATE tpj_bench_lib)
tpj_configure_target(tpj_bench)
```

### Task 8: Build the interfaces

Step 1: Configure and build the new targets.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_views tpj_bench`
Expected: both targets build with no warnings.

### Task 9: Test pass

Step 1: Dispatch the test-writer agent on plans/scalable-runtime/measured-runtime/stage-runner/FEATURE.md. It writes tests/views/ (tpj_views_tests, linking tpj_views) and tests/bench/ (tpj_bench_tests, linking tpj_bench_lib, with the executable's path, tests/parks/, and a scratch directory as compile definitions, and depending on tpj_bench), adds both to tests/CMakeLists.txt inside `if(TPJ_BUILD_APP)`, and changes the expected table of the repository_layers case in tests/checks/layer_check_test.cmake to the layers Task 5 declares. Test files mirror the src files they test: tests/views/food_overlay_test.cpp, and tests/bench/timing_test.cpp, stages_test.cpp, options_test.cpp, park_file_test.cpp, and main_test.cpp. Do not write test content here.

### Task 10: Join render and legible in views

Files:
- Modify: `src/views/food_overlay.cpp`

Step 1: Replace the stub with:

```cpp
#include "views/food_overlay.h"

#include "legible/food.h"
#include "render/food_overlay.h"

namespace tpj {

ParkMesh buildFoodAvailabilityOverlay(const World &world) {
  return buildFoodOverlay(
      world, [&world](const Place &place) { return foodAvailability(world, place).Value; });
}

} // namespace tpj
```

Step 2: Build and run views' tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_views_tests && build/windows-debug/tpj_views_tests.exe`
Expected: every test passes.

### Task 11: Build the app's overlay through views

Files:
- Modify: `src/app/scene/scene_uploads.cpp:3-5` (includes) and `:30-38` (foodOverlayMesh)
- Modify: `src/app/CMakeLists.txt` (tpj_app's link line)

Step 1: Capture the overlay as the app draws it before the change.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --overlay food --capture build/stage-runner-before.bmp`
Expected: build/stage-runner-before.bmp is written and the app exits with status 0.

Step 2: In scene_uploads.cpp replace the includes `#include "legible/food.h"` and `#include "render/food_overlay.h"` with `#include "views/food_overlay.h"`, keeping `#include "render/guest_mesh.h"` and `#include "render/park_mesh.h"`, and replace the body's last statement of foodOverlayMesh:

```cpp
  return buildFoodOverlay(
      world, [&world](const Place &place) { return foodAvailability(world, place).Value; });
```

with

```cpp
  return buildFoodAvailabilityOverlay(world);
```

Step 3: In src/app/CMakeLists.txt change tpj_app's link line to:

```cmake
target_link_libraries(tpj_app PRIVATE tpj_app_core tpj_legible tpj_render tpj_sim tpj_tools tpj_views)
```

Step 4: Capture after the change and compare.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --overlay food --capture build/stage-runner-after.bmp`
Expected: the app exits with status 0. Open both BMPs: they show the same scene, the same overlay band in the same colors. Byte equality is not expected, since frames step the simulation by elapsed time.

### Task 12: Summarize durations

Files:
- Modify: `src/bench/timing.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/timing.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace tpj {

TimeSummary summarizeTimes(std::span<const int64_t> nanoseconds) {
  if (nanoseconds.empty()) {
    throw std::invalid_argument("summarizeTimes: no durations");
  }
  std::vector<int64_t> sorted(nanoseconds.begin(), nanoseconds.end());
  std::ranges::sort(sorted);
  return TimeSummary{sorted.size(), sorted[(sorted.size() - 1) / 2], sorted.front(), sorted.back()};
}

} // namespace tpj
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#timing_test]"`
Expected: every test in timing_test.cpp passes.

### Task 13: Read the command line

Files:
- Modify: `src/bench/options.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/options.h"

#include <charconv>
#include <system_error>
#include <vector>

namespace tpj {

std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error) {
  BenchOptions options;
  std::vector<std::string> files;
  for (size_t i = 1; i < arguments.size(); ++i) {
    const std::string &argument = arguments[i];
    if (argument == "--ticks") {
      if (i + 1 == arguments.size()) {
        error = "--ticks needs a count";
        return std::nullopt;
      }
      const std::string &value = arguments[++i];
      uint64_t ticks = 0;
      const char *end = value.data() + value.size();
      const auto result = std::from_chars(value.data(), end, ticks);
      if (value.empty() || result.ec != std::errc() || result.ptr != end || ticks == 0) {
        error = "--ticks takes a positive decimal count, not '" + value + "'";
        return std::nullopt;
      }
      options.Ticks = ticks;
    } else if (argument.starts_with("--")) {
      error = "unknown option " + argument;
      return std::nullopt;
    } else {
      files.push_back(argument);
    }
  }
  if (files.empty()) {
    error = "no park file given";
    return std::nullopt;
  }
  if (files.size() > 1) {
    error = "more than one park file given";
    return std::nullopt;
  }
  options.Park = files.front();
  return options;
}

} // namespace tpj
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#options_test]"`
Expected: every test in options_test.cpp passes.

### Task 14: Read a park file

Files:
- Modify: `src/bench/park_file.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/park_file.h"

#include <fstream>
#include <sstream>

namespace tpj {

std::optional<std::string> readParkFile(const std::string &path) {
  const std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << file.rdbuf();
  if (file.bad()) {
    return std::nullopt;
  }
  return text.str();
}

} // namespace tpj
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#park_file_test]"`
Expected: every test in park_file_test.cpp passes.

### Task 15: Time the stages

Files:
- Modify: `src/bench/stages.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/stages.h"

#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "views/food_overlay.h"

#include <array>
#include <charconv>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tpj {
namespace {

using Clock = std::chrono::steady_clock;

int64_t nanosecondsBetween(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Exactly 16 lowercase hexadecimal digits.
std::string hex16(uint64_t value) {
  std::array<char, 16> digits{};
  const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value, 16);
  const auto length = static_cast<size_t>(result.ptr - digits.data());
  return std::string(16 - length, '0') + std::string(digits.data(), length);
}

// A repeated stage's times and the output of its last call.
template <typename Output> struct Timed {
  TimeSummary Times;
  Output Last;
};

static_assert(WARM_UPS >= 1, "timeRepeated keeps the first warm-up's output");

// Makes WARM_UPS untimed calls and then REPETITIONS timed ones of work on what prepare gives,
// which is made before the clock is read. The first warm-up's output starts the kept one, and
// each later call's output replaces it after the clock is read, so freeing the one before is
// never timed.
template <typename Prepare, typename Work> auto timeRepeated(Prepare prepare, Work work) {
  auto firstInput = prepare();
  auto last = work(firstInput);
  std::vector<int64_t> times;
  times.reserve(REPETITIONS);
  for (size_t call = 1; call < WARM_UPS + REPETITIONS; ++call) {
    auto input = prepare();
    const Clock::time_point start = Clock::now();
    auto output = work(input);
    const Clock::time_point end = Clock::now();
    if (call >= WARM_UPS) {
      times.push_back(nanosecondsBetween(start, end));
    }
    last = std::move(output);
  }
  return Timed<decltype(last)>{summarizeTimes(times), std::move(last)};
}

struct Nothing {};

Nothing nothing() { return {}; }

// A stage that builds a mesh from the resolved park, with its vertex count as its result.
template <typename Build>
StageResult meshStage(std::string name, const World &park, Build build) {
  const auto mesh =
      timeRepeated(nothing, [&park, &build](Nothing /*input*/) { return build(park); });
  return StageResult{std::move(name), mesh.Times, ResultKind::Vertices, mesh.Last.Vertices.size()};
}

} // namespace

std::vector<StageResult> benchPark(const World &loaded, uint64_t ticks) {
  if (ticks == 0) {
    throw std::invalid_argument("benchPark: ticks must be positive");
  }
  std::vector<StageResult> stages;

  const auto resolution = timeRepeated([&loaded] { return copyWorld(loaded); },
                                       [](World &copy) {
                                         resolveWorld(copy);
                                         return std::move(copy);
                                       });
  const World &park = resolution.Last;
  stages.push_back(StageResult{"resolution", resolution.Times, ResultKind::Hash, hashWorld(park)});

  const std::vector<ParkBox> boxes = parkBoxes(park);
  if (!boxes.empty()) {
    const std::optional<ParkEdit> edit = ParkEdit{MoveBox{boxes.front().Key, boxes.front().At}};
    const auto preview = timeRepeated(
        nothing, [&park, &edit](Nothing /*input*/) { return previewEdit(park, edit); });
    const std::optional<World> &candidate = preview.Last.Candidate;
    if (!candidate) {
      throw std::runtime_error("moving box " +
                               std::to_string(static_cast<uint64_t>(boxes.front().Key)) +
                               " to its own pose is refused, so the park has no preview");
    }
    stages.push_back(StageResult{"preview", preview.Times, ResultKind::Hash, hashWorld(*candidate)});
  }

  stages.push_back(meshStage("food-overlay", park, buildFoodAvailabilityOverlay));
  stages.push_back(meshStage("park-mesh", park, buildParkMesh));
  stages.push_back(meshStage("guest-mesh", park, buildGuestMesh));

  World stepped = copyWorld(park);
  std::vector<int64_t> tickTimes;
  tickTimes.reserve(ticks);
  for (uint64_t tick = 0; tick < ticks; ++tick) {
    const Clock::time_point start = Clock::now();
    stepWorld(stepped);
    const Clock::time_point end = Clock::now();
    tickTimes.push_back(nanosecondsBetween(start, end));
  }
  stages.push_back(
      StageResult{"ticks", summarizeTimes(tickTimes), ResultKind::Hash, hashWorld(stepped)});
  return stages;
}

std::string stageLine(const StageResult &stage) {
  const bool isHash = stage.Kind == ResultKind::Hash;
  return "stage " + stage.Name + " count " + std::to_string(stage.Times.Count) + " median " +
         std::to_string(stage.Times.Median) + " least " + std::to_string(stage.Times.Least) +
         " greatest " + std::to_string(stage.Times.Greatest) + (isHash ? " hash " : " vertices ") +
         (isHash ? hex16(stage.Result) : std::to_string(stage.Result));
}

std::string parkLine(std::string_view path, uint64_t ticks) {
  return "park " + std::string(path) + " ticks " + std::to_string(ticks) + " warm-ups " +
         std::to_string(WARM_UPS) + " repetitions " + std::to_string(REPETITIONS);
}

} // namespace tpj
```

If the compiler refuses a deduction here, such as a lambda's return type, fix only the spelling (an explicit template argument or return type); the timed calls, the order of stages, and what each result is stay as written. Keep every optional checked in the function that dereferences it, as candidate is, since tidy's bugprone-unchecked-optional-access does not follow calls.

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#stages_test]"`
Expected: every test in stages_test.cpp passes.

### Task 16: Compose tpj_bench

Files:
- Modify: `src/bench/main.cpp`

Step 1: Replace the stub with:

```cpp
// tpj_bench: times each stage of the gameplay runtime on a park file, writing each stage's times
// beside its result.
//
//   tpj_bench [--ticks N] FILE

#include "bench/options.h"
#include "bench/park_file.h"
#include "bench/stages.h"
#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  const std::vector<std::string> arguments(argv, argv + argc);
  std::string error;
  const std::optional<tpj::BenchOptions> options = tpj::parseBenchOptions(arguments, error);
  if (!options) {
    std::cerr << "tpj_bench: " << error << "\nusage: tpj_bench [--ticks N] FILE\n";
    return 2;
  }
  const std::optional<std::string> text = tpj::readParkFile(options->Park);
  if (!text) {
    std::cerr << "tpj_bench: cannot read " << options->Park << '\n';
    return 1;
  }
  try {
    const tpj::World loaded = tpj::loadWorld(tpj::makeParkSchema(), *text);
    const std::vector<tpj::StageResult> stages = tpj::benchPark(loaded, options->Ticks);
    std::cout << tpj::parkLine(options->Park, options->Ticks) << '\n';
    for (const tpj::StageResult &stage : stages) {
      std::cout << tpj::stageLine(stage) << '\n';
    }
  } catch (const tpj::LoadError &loadError) {
    std::cerr << "tpj_bench: cannot load " << options->Park << ": " << loadError.what() << '\n';
    return 1;
  } catch (const std::exception &failure) {
    std::cerr << "tpj_bench: " << failure.what() << '\n';
    return 1;
  }
  return 0;
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe`
Expected: every test in tpj_bench_tests passes.

### Task 17: Move winding-path.park into the stress parks

Files:
- Move: `parks/lowfps.park` to `tests/parks/stress/winding-path.park`

Step 1: Run: `mkdir -p tests/parks/stress && mv parks/lowfps.park tests/parks/stress/winding-path.park && git add tests/parks/stress/winding-path.park`
Expected: no output.

Step 2: Confirm the cross-build check's glob does not reach it.

Run: `ls tests/parks/*.park`
Expected: exactly cut.park, fed.park, new.park, routes.park, sketch.park, supply.park, and warm.park.

Step 3: Run it.

Run: `build/windows-debug/tpj_bench.exe --ticks 30 tests/parks/stress/winding-path.park`
Expected: a park line, then lines for resolution, preview, food-overlay, park-mesh, guest-mesh, and ticks, and status 0.

### Task 18: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: everything builds with no warnings, and every test passes.

Step 2: Run: `cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: everything builds with no warnings, and every test passes.

Step 3: Run: `scripts/tidy.sh`
Expected: no findings.

Step 4: Run: `scripts/cross-build-check.sh`
Expected: it ends with "passed.", since no simulation code changed.

Step 5: Time winding-path.park on both builds, for the feature's report.

Run: `cmake.exe --preset windows-release && cmake.exe --build --preset windows-release --target tpj_bench && build/windows-release/tpj_bench.exe tests/parks/stress/winding-path.park && build/windows-debug/tpj_bench.exe tests/parks/stress/winding-path.park`
Expected: both write a park line and six stage lines, with the same result in each stage on both builds. The report gives both outputs.

### Task 19: Commit

Step 1: Commit the feature as one commit through the commit-hygiene skill, subject `Bench: Add the stage runner and the views unit`.
