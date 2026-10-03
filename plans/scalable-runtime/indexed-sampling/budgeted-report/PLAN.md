# Implementation Plan: Budgeted Report

## Goal

Make the runtime report measure two minutes of play, keep each stage's worst call, mark every stage over the 33 ms budget, and time the app's own frames on every stress park.

## Approach

The frame clock gains each frame's unclamped nanoseconds, a small FrameTimes component keeps them from the second frame's cost on, and the app writes its line on --frame-times, leaving all summarizing to tpj_bench_report, which turns the app's output into an ordinary launch under the build name windows-release-app. Report stages gain Worst, kept in a ReportStage beside the StageResult, and a show command prints one report aligned, marking stages over STAGE_BUDGET_NANOSECONDS. tpj_bench's default run becomes 3,600 ticks, windows-debug launches pass --ticks 300, and the script launches the app every round.

## Placement

Decision 0027 places each behavior this feature adds:

- A frame's unclamped nanoseconds: app, FrameClock in src/app/frame_clock.h. It already turns the counter's readings into each frame's time, so the raw difference is its to give.
- Which frames' costs are kept, and the frame-times line: app, a new FrameTimes in src/app/frame_times.h, in tpj_app_core. The rule for dropping the first two values and the line's form are stated without a window, so its tests reach them through its header (decision 0027), and the frame clock keeps one reason to change.
- Reading --frame-times: app, parseOptions in src/app/options.h, which owns the command line.
- Giving FrameTimes each frame's Nanoseconds and writing its line after the last frame: app, Application in src/app/application.h, which states the frame order; each is one call into FrameTimes. main.cpp is unchanged.
- The Frames result kind and the 3,600-tick default: bench, src/bench/stages.h, which owns what a stage's result is and the runner's defaults.
- A report stage's Worst: bench, src/bench/report/report.h, which owns what a report is.
- Aligning columns: bench, src/bench/report/internal/columns.h, private to report/ (decision 0016), shared by comparisonText and showText so the two print alike.
- Showing one report against the budget: bench, src/bench/report/show.h, a component of its own, since it owns the budget rule and reads a report without comparing it.
- Turning the app's output into a launch: bench, src/bench/report/frames.h. It owns the inverse of the app's frame-times line, as launches.h owns the inverse of tpj_bench's lines.
- The new commands: bench, src/bench/report/report_options.h and src/bench/report/main.cpp, which stays composition only.
- Launching the app each round and printing show: scripts/runtime-report.sh, which owns the report's launches.

## Tasks

### Task 1: Spec the app's frame times

Files:
- Modify: `src/app/SPEC.md`

Step 1: In the paragraph beginning "main.cpp only composes", after the sentence ending "carrying the part of a tick left over to the next frame and dropping the whole ticks beyond the cap.", insert:

"Each FrameStep also gives Nanoseconds, the whole nanoseconds between the reading and the previous one, rounded down and never clamped, so a frame's full cost is known even when Dt is clamped."

Step 2: In the Command line section, after the paragraph beginning "--frames N exits after N frames.", insert a paragraph:

"--frame-times, given with a positive --frames, writes to standard output, once the last frame is drawn, the line of a FrameTimes (frame_times.h) given each frame's Nanoseconds as the frame starts, and a line feed. A frame's advance gives the time since the previous frame's, so it is the previous frame's whole cost. FrameTimes drops the first two values, the first covering no frame and the second the first frame, which follows loading and builds the park mesh and frames the camera once, and keeps the rest in order. Its line is `frame-times` followed by each kept value in decimal after a single space. So N frames give N - 2 durations, the costs of frames 2 to N - 1, and neither loading nor the first frame's one-off work is in any of them. It is how scripts/runtime-report.sh times the app (src/bench/SPEC.md)."

Step 3: In the paragraph beginning "An unknown option, an option missing its value,", replace "an --overlay value other than food, or --hash given with" by "an --overlay value other than food, --frame-times without a --frames value that is positive, or --hash given with".

### Task 2: Spec the report's changes

Files:
- Modify: `src/bench/SPEC.md`
- Modify: `src/scenarios/SPEC.md`

Step 1: In src/bench/SPEC.md, Stages, replace "a ResultKind, hash or vertices, and a Result" by "a ResultKind, hash, vertices, or frames, and a Result", and after that sentence add: "benchPark never gives frames, a count of frames; tpj_bench_report frames does (Frames)."

Step 2: In Command line, replace "N defaults to DEFAULT_TICKS, 300." by "N defaults to DEFAULT_TICKS, 3600, two minutes of game time, so a launch measures sustained play rather than seconds of it." In the same section, replace "the result name is hash for resolution, preview, and ticks, with the result as 16 lowercase hexadecimal digits, and vertices for the meshes, with the result in decimal" by "the result name is hash for resolution, preview, and ticks, with the result as 16 lowercase hexadecimal digits, vertices for the meshes, and frames for a Frames result, each of those two in decimal".

Step 3: In Report, replace "launches tpj_bench several times on every stress park with windows-release, and on every one but full.park with windows-debug, which cannot step the full park in reasonable time. tpj_bench_report summarizes the launches into a report and compares two reports." by "launches tpj_bench several times on every stress park with windows-release, and on every one but full.park with windows-debug, which cannot step the full park in reasonable time, and launches the app on every stress park with windows-release. tpj_bench_report turns the app's frame times into launches, summarizes the launches into a report, shows a report against the budget, and compares two reports." Replace "Its arithmetic is on the times tpj_bench wrote." by "Its arithmetic is on the times tpj_bench and the app wrote."

Step 4: In Reading lines, replace the last sentence by: "A stage line has the form stageLine writes: `stage <name> count <c> median <m> least <l> greatest <g> hash <h>`, `... vertices <v>`, or `... frames <n>`. A report's stage line is a stage line followed by `worst <w>`, a time."

Step 5: In Summarizing, after "Its Least and Greatest are the launches' spread, and its Count is the number of launches.", add: "Each report stage also has Worst, the greatest of the launches' Greatest for the stage: the slowest single call of any launch, so a hitch survives summarizing even where the medians hide it."

Step 6: In Report text, replace "followed by its stages as stageLine writes them" by "followed by its stages, each as stageLine writes it followed by ` worst <w>`".

Step 7: In Comparing, replace "and Before and After, the stage's StageResult in each report, either one empty when the stage is not in that report. When both are present:" by "and Before and After, the stage as each report holds it, with its Worst, which the comparison text does not write, either one empty when the stage is not in that report. When both are present:".

Step 8: After the Comparison text section, add two sections:

"### Showing

showText(report) writes one line per stage of each park, in the report's order, then `over-budget <n> of <m>`, every line ending with a line feed. A stage's fields are, in order: the build, park, and stage; its median, `[` joined to its least, and its greatest joined to `]`, in microseconds as comparisonText writes times; `worst`, then the worst in microseconds; and `over-budget` when the median exceeds STAGE_BUDGET_NANOSECONDS, 33,333,333, the whole nanoseconds of SIM_TICK_SECONDS. Every stage is held to one tick's time, since any single stage that takes longer stalls play. m counts the stages and n those marked. Columns are aligned as comparisonText aligns them, on every line but the last.

### Frames

The app's --frame-times line (app/SPEC.md) becomes a launch through framesLaunch(park, text). Of text's lines, read as parseLaunches reads lines, exactly one must have frame-times as its first field, with at least one more field, each a time. framesLaunch gives the text `park <park> ticks 0 warm-ups 1 repetitions <k>` and `stage frames count <k> median <m> least <l> greatest <g> frames <k>`, each ending with a line feed, with k the number of durations and the times their summarizeTimes. An app launch steps no fixed number of ticks, so its ticks are 0, and its first frame, which follows loading, is its one warm-up. framesLaunch throws ReportError when text has no frame-times line, more than one, or one with no durations or a field that is not a time."

Step 9: In the Command line section under Report, add after the compare sentence: "tpj_bench_report show REPORT reads REPORT with parseReport and writes showText of it. tpj_bench_report frames PARK OUTPUT reads the file OUTPUT and writes framesLaunch(PARK, its text); PARK is written as given, not read." Replace "gives ReportOptions, a Command, summarize or compare, and its Paths" by "gives ReportOptions, a Command, summarize, compare, show, or frames, and its Paths", and "or a count of files other than one for summarize and two for compare" by "or a count of arguments other than one for summarize and show and two for compare and frames". Replace the usage line by `usage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER | show REPORT | frames PARK OUTPUT`.

Step 10: In The script, replace "It builds tpj_bench and tpj_bench_report with windows-release and tpj_bench with windows-debug" by "It builds tpj_bench, tpj_bench_report, and tpj_app with windows-release and tpj_bench with windows-debug". Replace "It launches tpj_bench with no options on every file matching tests/parks/stress/*.park, in name order, N times on each build, 10 by default, except that windows-debug skips tests/parks/stress/full.park, which it cannot step in reasonable time." by "It launches tpj_bench on every file matching tests/parks/stress/*.park, in name order, N times on each build, 10 by default, with no options on windows-release and with --ticks 300 on windows-debug, the same in every report, except that windows-debug skips tests/parks/stress/full.park, which it cannot step in reasonable time." Replace "in each round, every park runs once with windows-release and then every park but full.park once with windows-debug." by "in each round, every park runs once with windows-release, then every park but full.park once with windows-debug, and then the app, build/windows-release/ThemeParkJones.exe --park <park> --frames 300 --frame-times, on every park. Each app launch's output goes through tpj_bench_report frames of the park, and is appended after `build windows-release-app`." Replace "Once the last launch is done, tpj_bench_report summarize of that file writes the report, without carriage returns, to REPORT, through REPORT.partial renamed into place. The script ends by printing" by "Once the last launch is done, tpj_bench_report summarize of that file writes the report, without carriage returns, to REPORT, through REPORT.partial renamed into place. The script then prints tpj_bench_report show of REPORT, and ends by printing". In the list of failures, replace "a launch fails, naming its build, park, and round;" by "a launch fails, naming its build, park, and round, the app's and its conversion's included;".

Step 11: In src/scenarios/SPEC.md, replace "120 times tpj_bench's default run" by "10 times tpj_bench's default run".

### Task 3: The app's interface

Files:
- Modify: `src/app/frame_clock.h`
- Create: `src/app/frame_times.h`
- Create: `src/app/frame_times.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/options.h`

Step 1: In src/app/frame_clock.h, in FrameStep, after `uint32_t Ticks = 0;`, add:

```cpp
  // The whole nanoseconds since the previous reading, rounded down and never clamped.
  int64_t Nanoseconds = 0;
```

Step 2: Create src/app/frame_times.h:

```cpp
#ifndef TPJ_APP_FRAME_TIMES_H
#define TPJ_APP_FRAME_TIMES_H

#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// Each frame's cost, given as each frame starts: a frame's advance gives the time since the
// previous frame's, so it is the previous frame's cost. The first value covers no frame and the
// second the first frame, which follows loading and does one-off work, so both are dropped.
class FrameTimes {
public:
  // Gives the Nanoseconds of the advance at a frame's start.
  void add(int64_t nanoseconds);

  // The values kept: every one given after the first two, in order.
  std::span<const int64_t> kept() const;

  // frame-times, then each kept value in decimal after a single space, with no line feed.
  std::string line() const;

private:
  size_t Given = 0;
  std::vector<int64_t> Kept;
};

} // namespace tpj

#endif
```

Step 3: Create src/app/frame_times.cpp with stubs:

```cpp
#include "app/frame_times.h"

namespace tpj {

void FrameTimes::add(int64_t /*nanoseconds*/) {}

std::span<const int64_t> FrameTimes::kept() const { return {}; }

std::string FrameTimes::line() const { return {}; }

} // namespace tpj
```

Step 4: In src/app/CMakeLists.txt, add `frame_times.cpp` after `frame_clock.cpp` in tpj_app_core.

Step 5: In src/app/options.h, in Options after `bool ShowFoodOverlay = false;`, add:

```cpp
  // Write each frame's cost after the last frame.
  bool FrameTimes = false;
```

and extend parseOptions' comment: after "an --overlay value other than food," insert " --frame-times without a positive --frames,". After "--overlay food with the food overlay on." insert " --frame-times writes each frame's cost after the last frame."

Step 6: Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests 2>&1 | tail -1`
Expected: the link line of tpj_app_tests.exe, no warnings.

### Task 4: Test pass

Dispatch the test-writer for criteria 1 to 3 against src/app/frame_clock.h, src/app/frame_times.h, src/app/options.h, src/app/SPEC.md, and docs/principles.md, in tests/app/frame_clock_test.cpp, a new tests/app/frame_times_test.cpp registered in tests/app/CMakeLists.txt, and tests/app/options_test.cpp. Criteria 4 to 8 are checks on test utilities and get no tests.

### Task 5: Nanoseconds

Files:
- Modify: `src/app/frame_clock.cpp`

Step 1: Replace advance's first statement with:

```cpp
  FrameStep step;
  const uint64_t elapsed = counter - Last;
  // Whole seconds and the rest apart, so a long gap cannot overflow.
  step.Nanoseconds = static_cast<int64_t>((elapsed / Frequency) * 1'000'000'000 +
                                          (elapsed % Frequency) * 1'000'000'000 / Frequency);
  step.Dt = std::min(static_cast<double>(elapsed) / static_cast<double>(Frequency),
                     MAX_FRAME_SECONDS);
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests 2>&1 | tail -1 && build/windows-debug/tpj_app_tests.exe -# "[#frame_clock_test]"`
Expected: all frame clock tests pass.

### Task 6: FrameTimes

Files:
- Modify: `src/app/frame_times.cpp`

Step 1: Replace the stubs with:

```cpp
void FrameTimes::add(int64_t nanoseconds) {
  // The first covers no frame, and the second the first frame, which follows loading.
  if (++Given > 2) {
    Kept.push_back(nanoseconds);
  }
}

std::span<const int64_t> FrameTimes::kept() const { return Kept; }

std::string FrameTimes::line() const {
  std::string text = "frame-times";
  for (const int64_t duration : Kept) {
    text += ' ';
    text += std::to_string(duration);
  }
  return text;
}
```

Step 2: Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests 2>&1 | tail -1 && build/windows-debug/tpj_app_tests.exe -# "[#frame_times_test]"`
Expected: all FrameTimes tests pass.

### Task 7: --frame-times

Files:
- Modify: `src/app/options.cpp`

Step 1: In parseOptions' loop, before the final `} else {`, add:

```cpp
    } else if (strcmp(argv[i], "--frame-times") == 0) {
      options.FrameTimes = true;
```

Step 2: After the --hash check, add:

```cpp
  if (options.FrameTimes && options.FrameLimit <= 0) {
    valid = false;
  }
```

Step 3: In the usage string, after `[--overlay food]` add ` [--frame-times]`.

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests 2>&1 | tail -1 && build/windows-debug/tpj_app_tests.exe -# "[#options_test]"`
Expected: all options tests pass.

### Task 8: The app writes its frame times

Files:
- Modify: `src/app/application.h`
- Modify: `src/app/application.cpp`

Step 1: In application.h, add `#include "app/frame_times.h"`, and after `const char *CapturePath;` add:

```cpp
  bool WriteFrameTimes;
  FrameTimes Frames;
```

Step 2: In application.cpp, add `#include <stdio.h>` and initialize `WriteFrameTimes(options.FrameTimes)` after `CapturePath(options.CapturePath)`.

Step 3: In runFrames, after `const FrameStep step = Clock.advance(SDL_GetPerformanceCounter());`, add:

```cpp
    Frames.add(step.Nanoseconds);
```

Step 4: In run, replace `return runFrames();` by:

```cpp
    if (!runFrames()) {
      return false;
    }
    if (WriteFrameTimes) {
      (void)printf("%s\n", Frames.line().c_str());
    }
    return true;
```

Step 5: Run: `cmake.exe --build --preset windows-debug --target tpj_app 2>&1 | tail -1 && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --frames 5 --frame-times | tr -d '\r' | awk '{print $1, NF - 1}'`
Expected: `frame-times 3`

### Task 9: The Frames kind and the sustained default

Files:
- Modify: `src/bench/stages.h`
- Modify: `src/bench/stages.cpp`
- Modify: `src/bench/report/internal/lines.cpp`

Step 1: In stages.h, set `inline constexpr uint64_t DEFAULT_TICKS = 3600;` and its comment to `// The ticks tpj_bench steps without --ticks: two minutes of game time.`. Replace the ResultKind comment and enum by:

```cpp
// What a stage's result is: a world's hash, a mesh's vertex count, or a count of frames.
enum class ResultKind { Hash, Vertices, Frames };
```

and stageLine's comment `<hash|vertices> <result>` by `<hash|vertices|frames> <result>`, "and a vertex count in decimal" by "and a count in decimal".

Step 2: In stages.cpp, replace stageLine's body with:

```cpp
  std::string result;
  switch (stage.Kind) {
  case ResultKind::Hash:
    result = " hash " + hex16(stage.Result);
    break;
  case ResultKind::Vertices:
    result = " vertices " + std::to_string(stage.Result);
    break;
  case ResultKind::Frames:
    result = " frames " + std::to_string(stage.Result);
    break;
  }
  return "stage " + stage.Name + " count " + std::to_string(stage.Times.Count) + " median " +
         std::to_string(stage.Times.Median) + " least " + std::to_string(stage.Times.Least) +
         " greatest " + std::to_string(stage.Times.Greatest) + result;
```

Step 3: In lines.cpp, readStageLine, before the final `} else {`, add:

```cpp
  } else if (line.Fields[10] == "frames") {
    stage.Kind = ResultKind::Frames;
    stage.Result = readCount(line, 11);
```

and change the refusal to `"expected 'hash', 'vertices', or 'frames' as " + fieldName(10)`. Change readStageLine to read the first 12 fields of a longer line too, so a report's stage line can reuse it: replace `if (line.Fields.size() != 12) {` by `if (line.Fields.size() < 12) {` and the message by `"a stage line has at least 12 fields"`. In lines.h, change its comment to `// The first 12 fields of a line as stageLine writes them, or refuses the line.` The launch reader keeps its exact count: in src/bench/report/launches.cpp, where it calls readStageLine for a stage line, refuse first when `line.Fields.size() != 12` with `refuseLine(line, "a stage line has 12 fields")`.

Step 4: Run: `cmake.exe --build --preset windows-release --target tpj_bench 2>&1 | tail -1 && build/windows-release/tpj_bench.exe tests/parks/stress/winding-path.park | tr -d '\r' | grep -E "^park|stage ticks"`
Expected: `park tests/parks/stress/winding-path.park ticks 3600 warm-ups 2 repetitions 11` and a `stage ticks count 3600 ...` line.

### Task 10: Report stages keep their worst call

Files:
- Modify: `src/bench/report/report.h`
- Modify: `src/bench/report/report.cpp`
- Modify: `src/bench/report/comparison.h`
- Modify: `src/bench/report/comparison.cpp`

Step 1: In report.h, before ReportPark, add:

```cpp
// A stage of a report: the launches' medians summarized, and the slowest call of any launch.
struct ReportStage {
  StageResult Stage;
  int64_t Worst = 0;

  bool operator==(const ReportStage &) const = default;
};
```

and change ReportPark's `std::vector<StageResult> Stages;` to `std::vector<ReportStage> Stages;`. Update reportText's comment to say each stage line ends with ` worst <w>`.

Step 2: In report.cpp, summarizeGroup: also collect `int64_t worst = 0;` as `worst = std::max(worst, launch->Stages[s].Times.Greatest);` in the loop over the group, and push `ReportStage{std::move(stage), worst}`. In reportText, write `stageLine(stage.Stage) + " worst " + std::to_string(stage.Worst) + "\n"`. In parseReport, for a stage line: require 14 fields (`refuseLine(line, "a report's stage line has 14 fields")` otherwise), `expectWord(line, 12, "worst")`, and build `ReportStage{readStageLine(line), readTime(line, 13)}`; the repeated-name check compares `read.Stage.Name`.

Step 3: In comparison.h, change `std::optional<StageResult> Before;` and `After` to `std::optional<ReportStage>`, and add `#include "bench/report/report.h"` if not present. In comparison.cpp, findStage returns `const ReportStage *`, finding by `stage.Stage.Name` (use `std::ranges::find_if` with a lambda comparing `read.Stage.Name == stage`). In compareReports, iterate `const ReportStage &stage`, use `stage.Stage.Name`, `stage.Stage.Times`, `stage.Stage.Kind`, and `stage.Stage.Result`. In comparisonFields, read times as `comparison.Before->Stage.Times` and `comparison.After->Stage.Times`.

Step 4: Run: `cmake.exe --build --preset windows-release --target tpj_bench_report 2>&1 | tail -1`
Expected: the link line, no warnings.

### Task 11: Shared column alignment

Files:
- Create: `src/bench/report/internal/columns.h`
- Create: `src/bench/report/internal/columns.cpp`
- Modify: `src/bench/report/comparison.cpp`
- Modify: `src/bench/CMakeLists.txt`

Step 1: Create columns.h:

```cpp
#ifndef TPJ_BENCH_REPORT_INTERNAL_COLUMNS_H
#define TPJ_BENCH_REPORT_INTERNAL_COLUMNS_H

#include <span>
#include <string>
#include <vector>

namespace tpj {

// Each line's fields, each but its last followed by enough spaces to reach the widest field in its
// position on any line, and one more, every line ending with a line feed.
std::string alignedText(std::span<const std::vector<std::string>> lines);

} // namespace tpj

#endif
```

Step 2: Create columns.cpp:

```cpp
#include "bench/report/internal/columns.h"

#include <algorithm>
#include <stddef.h>

namespace tpj {

std::string alignedText(std::span<const std::vector<std::string>> lines) {
  std::vector<size_t> widths;
  for (const std::vector<std::string> &fields : lines) {
    widths.resize(std::max(widths.size(), fields.size()), 0);
    for (size_t i = 0; i < fields.size(); ++i) {
      widths[i] = std::max(widths[i], fields[i].size());
    }
  }
  std::string text;
  for (const std::vector<std::string> &fields : lines) {
    for (size_t i = 0; i < fields.size(); ++i) {
      text += fields[i];
      if (i + 1 < fields.size()) {
        text += std::string(widths[i] - fields[i].size() + 1, ' ');
      }
    }
    text += '\n';
  }
  return text;
}

} // namespace tpj
```

Step 3: In comparison.cpp, include columns.h, drop the widths bookkeeping from comparisonText, and build its text as `alignedText(lines)` followed by the `clear <n> of <m>` line. Move `microseconds` and `appendTimes` into columns.h and columns.cpp as public functions of the internal header, with the comments `// Nanoseconds as microseconds with one decimal place.` and `// A summary's median, [least, and greatest], in microseconds.`, so show writes times alike.

Step 4: In src/bench/CMakeLists.txt, add `report/internal/columns.cpp` after `report/comparison.cpp`.

Step 5: Run: `cmake.exe --build --preset windows-release --target tpj_bench_report 2>&1 | tail -1`
Expected: the link line, no warnings.

### Task 12: show

Files:
- Create: `src/bench/report/show.h`
- Create: `src/bench/report/show.cpp`
- Modify: `src/bench/CMakeLists.txt`

Step 1: Create show.h:

```cpp
#ifndef TPJ_BENCH_REPORT_SHOW_H
#define TPJ_BENCH_REPORT_SHOW_H

#include "bench/report/report.h"

#include <span>
#include <stdint.h>
#include <string>

namespace tpj {

// The whole nanoseconds of SIM_TICK_SECONDS. A stage whose median exceeds it stalls play.
inline constexpr int64_t STAGE_BUDGET_NANOSECONDS = 33'333'333;

// One aligned line per stage, its times in microseconds and its worst call, marked over-budget
// when its median exceeds STAGE_BUDGET_NANOSECONDS, then `over-budget <n> of <m>`.
std::string showText(std::span<const ReportPark> report);

} // namespace tpj

#endif
```

Step 2: Create show.cpp:

```cpp
#include "bench/report/show.h"

#include "bench/report/internal/columns.h"

#include <stddef.h>
#include <vector>

namespace tpj {

std::string showText(std::span<const ReportPark> report) {
  std::vector<std::vector<std::string>> lines;
  size_t over = 0;
  for (const ReportPark &park : report) {
    for (const ReportStage &stage : park.Stages) {
      std::vector<std::string> fields{park.Build, park.Park, stage.Stage.Name};
      appendTimes(fields, stage.Stage.Times);
      fields.emplace_back("worst");
      fields.push_back(microseconds(stage.Worst));
      if (stage.Stage.Times.Median > STAGE_BUDGET_NANOSECONDS) {
        fields.emplace_back("over-budget");
        ++over;
      }
      lines.push_back(std::move(fields));
    }
  }
  return alignedText(lines) + "over-budget " + std::to_string(over) + " of " +
         std::to_string(lines.size()) + "\n";
}

} // namespace tpj
```

Add `#include <utility>` for std::move.

Step 3: Add `report/show.cpp` to tpj_bench_lib in src/bench/CMakeLists.txt, in name order.

Step 4: Run: `cmake.exe --build --preset windows-release --target tpj_bench_report 2>&1 | tail -1`
Expected: the link line, no warnings.

### Task 13: frames

Files:
- Create: `src/bench/report/frames.h`
- Create: `src/bench/report/frames.cpp`
- Modify: `src/bench/CMakeLists.txt`

Step 1: Create frames.h:

```cpp
#ifndef TPJ_BENCH_REPORT_FRAMES_H
#define TPJ_BENCH_REPORT_FRAMES_H

#include <string>
#include <string_view>

namespace tpj {

// The launch text of the app's output on a park: `park <park> ticks 0 warm-ups 1 repetitions <k>`
// and a frames stage summarizing its frame-times line's k durations. Throws ReportError unless the
// output holds exactly one frame-times line, with at least one duration, each a time.
std::string framesLaunch(std::string_view park, std::string_view output);

} // namespace tpj

#endif
```

Step 2: Create frames.cpp:

```cpp
#include "bench/report/frames.h"

#include "bench/report/internal/lines.h"
#include "bench/report/report_error.h"
#include "bench/stages.h"
#include "bench/timing.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {

std::string framesLaunch(std::string_view park, std::string_view output) {
  const TextLine *timesLine = nullptr;
  const std::vector<TextLine> lines = readLines(output);
  for (const TextLine &line : lines) {
    if (line.Fields.front() == "frame-times") {
      if (timesLine != nullptr) {
        refuseLine(line, "a second frame-times line");
      }
      timesLine = &line;
    }
  }
  if (timesLine == nullptr) {
    throw ReportError("no frame-times line");
  }
  if (timesLine->Fields.size() < 2) {
    refuseLine(*timesLine, "a frame-times line has at least one duration");
  }
  std::vector<int64_t> durations;
  for (size_t i = 1; i < timesLine->Fields.size(); ++i) {
    durations.push_back(readTime(*timesLine, i));
  }
  const StageResult stage{"frames", summarizeTimes(durations), ResultKind::Frames,
                          durations.size()};
  return "park " + std::string(park) + " ticks 0 warm-ups 1 repetitions " +
         std::to_string(durations.size()) + "\n" + stageLine(stage) + "\n";
}

} // namespace tpj
```

Step 3: Add `report/frames.cpp` to tpj_bench_lib in src/bench/CMakeLists.txt, in name order.

Step 4: Run: `cmake.exe --build --preset windows-release --target tpj_bench_report 2>&1 | tail -1`
Expected: the link line, no warnings.

### Task 14: The show and frames commands

Files:
- Modify: `src/bench/report/report_options.h`
- Modify: `src/bench/report/report_options.cpp`
- Modify: `src/bench/report/main.cpp`

Step 1: In report_options.h, make the enum `enum class ReportCommandKind { Summarize, Compare, Show, Frames };` and update parseReportOptions' comment: "tpj_bench_report summarize LAUNCHES, compare BEFORE AFTER, show REPORT, or frames PARK OUTPUT ... a count of arguments other than one for summarize and show and two for compare and frames."

Step 2: In report_options.cpp, add before the unknown-command branch:

```cpp
  } else if (command == "show") {
    options.Command = ReportCommandKind::Show;
    files = 1;
  } else if (command == "frames") {
    options.Command = ReportCommandKind::Frames;
    files = 2;
```

and change the message to `command + (files == 1 ? " takes one argument" : " takes two arguments")`.

Step 3: In main.cpp, include `bench/report/frames.h` and `bench/report/show.h`; update the header comment's command list and the usage line to `usage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER | show REPORT | frames PARK OUTPUT`. The frames command reads only its second path, so replace the reading loop's start with `const size_t firstRead = options->Command == tpj::ReportCommandKind::Frames ? 1 : 0;` and loop `for (size_t p = firstRead; p < options->Paths.size(); ++p)` over `options->Paths[p]`, and set `reading = firstRead;` before the try. Replace the if/else that makes output by a switch:

```cpp
    switch (options->Command) {
    case tpj::ReportCommandKind::Summarize:
      output = tpj::reportText(tpj::summarizeLaunches(tpj::parseLaunches(texts[0])));
      break;
    case tpj::ReportCommandKind::Compare: {
      const std::vector<tpj::ReportPark> before = tpj::parseReport(texts[0]);
      reading = 1;
      const std::vector<tpj::ReportPark> after = tpj::parseReport(texts[1]);
      output = tpj::comparisonText(tpj::compareReports(before, after));
      break;
    }
    case tpj::ReportCommandKind::Show:
      output = tpj::showText(tpj::parseReport(texts[0]));
      break;
    case tpj::ReportCommandKind::Frames:
      output = tpj::framesLaunch(options->Paths[0], texts[0]);
      break;
    }
```

Step 4: Run: `cmake.exe --build --preset windows-release --target tpj_bench_report tpj_app 2>&1 | tail -1 && build/windows-release/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --frames 5 --frame-times > build/frames-check.txt && build/windows-release/tpj_bench_report.exe frames tests/parks/stress/winding-path.park build/frames-check.txt | tr -d '\r'`
Expected: `park tests/parks/stress/winding-path.park ticks 0 warm-ups 1 repetitions 3` and `stage frames count 3 median ... frames 3`.

### Task 15: The script

Files:
- Modify: `scripts/runtime-report.sh`

Step 1: Update the header comment to say windows-debug launches step 300 ticks, that every round also launches the app on every park with windows-release, and that the script prints the report's show.

Step 2: Replace `build windows-release tpj_bench tpj_bench_report` by `build windows-release tpj_bench tpj_bench_report tpj_app`.

Step 3: After `release_only=tests/parks/stress/full.park`, add:

```bash
# windows-debug steps fewer ticks, the same in every report, since it is reported beside release.
debug_ticks=300
# Frames of the app per launch; the first, after loading, is not timed.
app_frames=300
app_output="$out/app-output.txt"
```

Step 4: In the round's tpj_bench launch, pass `--ticks "$debug_ticks"` on windows-debug only:

```bash
            ticks=()
            [ "$preset" = windows-debug ] && ticks=(--ticks "$debug_ticks")
            echo "build $preset" >>"$out/launches.txt"
            "build/$preset/tpj_bench.exe" "${ticks[@]}" "$park" | tr -d '\r' >>"$out/launches.txt" ||
                fail "tpj_bench failed on $park with $preset in round $round"
```

Step 5: After the presets loop, inside the round loop, add:

```bash
    for park in "${parks[@]}"; do
        build/windows-release/ThemeParkJones.exe --park "$park" --frames "$app_frames" \
            --frame-times >"$app_output" || fail "the app failed on $park in round $round"
        echo "build windows-release-app" >>"$out/launches.txt"
        build/windows-release/tpj_bench_report.exe frames "$park" "$(wslpath -w "$app_output")" |
            tr -d '\r' >>"$out/launches.txt" ||
            fail "converting the app's frames on $park in round $round failed"
    done
```

Step 6: After `mv "$report.partial" "$report" || fail "cannot write $report"`, add:

```bash
build/windows-release/tpj_bench_report.exe show "$(wslpath -w "$report")" | tr -d '\r' ||
    fail "showing the report failed"
```

Step 7: Run: `bash -n scripts/runtime-report.sh && echo ok`
Expected: `ok`

### Task 16: Confirm the criteria

Step 1: Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: no diagnostics, `100% tests passed`, `tidy: clean.`

Step 2 (criterion 4): Run: `cmake.exe --build --preset windows-release --target tpj_app 2>&1 | tail -1 && build/windows-release/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --frames 5 --frame-times | tr -d '\r' | awk '/^frame-times/ {print $1, NF - 1}'; echo status ${PIPESTATUS[0]}`
Expected: `frame-times 3` and `status 0`.

Step 3 (criterion 5): Run: `build/windows-release/tpj_bench.exe tests/parks/stress/winding-path.park | tr -d '\r' | grep -E "^park|stage ticks" | cut -d' ' -f1-6`
Expected: `park tests/parks/stress/winding-path.park ticks 3600 warm-ups 2` and `stage ticks count 3600 median <m>`.

Step 4 (criteria 6 and 7): Run: `scripts/runtime-report.sh --launches 2 build/runtime-report/check.txt`, then `grep -c "^park" build/runtime-report/check.txt` and `grep "^stage" build/runtime-report/check.txt | awk '$13 != "worst" || $14 < $10 {bad++} END {print bad + 0}'`.
Expected: the show output with `over-budget <n> of <m>` last, then the wrote line; 5 park lines (2 stress parks on windows-release and windows-release-app, 1 on windows-debug); `0`. Check by eye that every stage line of show whose median is over 33333.3 µs ends with over-budget and no other does.

Step 5 (criterion 8): Run: `scripts/runtime-report.sh --compare build/runtime-report/check.txt build/runtime-report/check.txt | tail -3`
Expected: lines for windows-release-app frames, and `clear 0 of <m>`.

Step 6: The milestone's before report. Run: `scripts/runtime-report.sh build/runtime-report/indexed-sampling-before.txt`. Put its show output and how long it took in the feature's report. It is kept for place-indexed-entries to compare with.

### Task 17: Commit

Stage everything with `git add -A` (the test pass's files included), review per implementing-features, and commit once via commit-hygiene with the subject `Bench: Measure two minutes of play against the tick budget`.
