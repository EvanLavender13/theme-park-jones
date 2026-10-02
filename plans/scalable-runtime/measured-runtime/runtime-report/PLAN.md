# Implementation Plan: Runtime Report

## Goal

Add tpj_bench_report, which summarizes tpj_bench's launches into a report and compares two reports by whether their launches overlap, and scripts/runtime-report.sh, which builds both Windows presets, launches tpj_bench on every stress park in rounds, writes the report, and compares two.

## Approach

The report is a second concern of bench, so it gets its own subdirectory, src/bench/report/. The runner's components stay at bench's root, where only park_file is renamed text_file, since both executables read files with it. A private header there reads lines, fields, numbers, and stage lines. Above it, three components each own one step: reading launches, summarizing them into a report and writing and reading that report, and comparing two reports and writing the comparison. A report's stage is a StageResult whose times summarize the launches' medians, so a report writes its stages with tpj_bench's own stageLine. The script only builds, launches, and hands files to tpj_bench_report. It does no arithmetic on times.

## Placement

Decision 0027 places each behavior this feature adds:

- Reading text as numbered lines of fields, and reading a field as a count, a time, a hash, or a whole stage line: bench, src/bench/report/internal/lines.h. Both readers share it, and nothing outside report/ should name it, so it is private to report/ (decision 0016).
- The error a reader or the summary throws: bench, src/bench/report/report_error.h, so each component above can include it without the others.
- Reading a stream of launches: bench, src/bench/report/launches.h. It owns what a launch is, and is the inverse of tpj_bench's own lines.
- Summarizing launches into a report, and writing and reading a report: bench, src/bench/report/report.h. It owns what a report is, and its text is its own form.
- Comparing two reports and writing the comparison: bench, src/bench/report/comparison.h. It owns the rule for a clear change.
- Reading tpj_bench_report's command line: bench, src/bench/report/report_options.h, so it is tested through a header, as tpj_bench's options are.
- tpj_bench_report's entry point: bench, src/bench/report/main.cpp. It is composition only: it parses, reads, runs one command, writes, and turns each failure into a message and a status.
- Reading a file's text: bench, src/bench/text_file.h, renamed from park_file.h. Both executables use it, and a report is not a park.
- Equality of StageResult: bench, src/bench/stages.h, beside the type, so a Launch and a ReportPark can be compared whole.
- Building both presets, launching tpj_bench in rounds, and handing files to tpj_bench_report: scripts/runtime-report.sh. Only a script can drive cmake.exe and the Windows executables from WSL, as cross-build-check.sh does. It holds no arithmetic on times, which is all in tested C++.
- src/scenarios/main.cpp and src/app/main.cpp are untouched.

## Tasks

### Task 1: Write the report's spec

Files:
- Modify: `src/bench/SPEC.md`

Step 1: Make the three changes under "Spec changes" for src/bench/SPEC.md in plans/scalable-runtime/measured-runtime/runtime-report/FEATURE.md:
- replace the first paragraph's sentence about tpj_bench_lib;
- rename readParkFile in Command line;
- add the Report section after Command line and before the paragraph that begins "Stress parks live in tests/parks/stress/".

Copy each quoted text without the leading "> " of its lines. The indented example lines keep their four leading spaces.

### Task 2: Name the script in CLAUDE.md

Files:
- Modify: `CLAUDE.md`

Step 1: In the Build and test section, after the sentence ending "pre-push runs it when the simulation's inputs change.", add the sentence quoted under "CLAUDE.md" in FEATURE.md's Spec changes, in the same paragraph.

### Task 3: Rename park_file to text_file

Files:
- Move: `src/bench/park_file.h` to `src/bench/text_file.h`
- Move: `src/bench/park_file.cpp` to `src/bench/text_file.cpp`
- Modify: `src/bench/main.cpp:7,27`, `src/bench/CMakeLists.txt:5`

Step 1: Run: `git mv src/bench/park_file.h src/bench/text_file.h && git mv src/bench/park_file.cpp src/bench/text_file.cpp`
Expected: no output.

Step 2: In src/bench/text_file.h, change the guard `TPJ_BENCH_PARK_FILE_H` to `TPJ_BENCH_TEXT_FILE_H` in both places, and change `std::optional<std::string> readParkFile(const std::string &path);` to `std::optional<std::string> readTextFile(const std::string &path);`.

Step 3: In src/bench/text_file.cpp, change `#include "bench/park_file.h"` to `#include "bench/text_file.h"` and `readParkFile` to `readTextFile`.

Step 4: In src/bench/main.cpp, change `#include "bench/park_file.h"` to `#include "bench/text_file.h"`, keeping the includes sorted, so it moves after `#include "bench/stages.h"`. Change `tpj::readParkFile(` to `tpj::readTextFile(`.

Step 5: In src/bench/CMakeLists.txt, replace the line `    park_file.cpp` with nothing, and add `    text_file.cpp` after `    stages.cpp`, keeping the list sorted. Task 6 rewrites this list in full.

### Task 4: Give StageResult equality

Files:
- Modify: `src/bench/stages.h:27-32`

Step 1: In `struct StageResult`, after `uint64_t Result = 0;`, add a blank line and:

```cpp
  bool operator==(const StageResult &) const = default;
```

### Task 5: Declare the report's interface

Files:
- Create: `src/bench/report/report_error.h`
- Create: `src/bench/report/launches.h`, `src/bench/report/launches.cpp`
- Create: `src/bench/report/report.h`, `src/bench/report/report.cpp`
- Create: `src/bench/report/comparison.h`, `src/bench/report/comparison.cpp`
- Create: `src/bench/report/report_options.h`, `src/bench/report/report_options.cpp`
- Create: `src/bench/report/main.cpp`

Step 1: src/bench/report/report_error.h:

```cpp
#ifndef TPJ_BENCH_REPORT_REPORT_ERROR_H
#define TPJ_BENCH_REPORT_REPORT_ERROR_H

#include <stdexcept>

namespace tpj {

// Text that breaks the form of launches or of a report, or launches that cannot be summarized
// together. A reader's message begins `line <n>: `.
class ReportError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace tpj

#endif
```

Step 2: src/bench/report/launches.h:

```cpp
#ifndef TPJ_BENCH_REPORT_LAUNCHES_H
#define TPJ_BENCH_REPORT_LAUNCHES_H

#include "bench/stages.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// What one run of tpj_bench wrote, and the build that ran it.
struct Launch {
  std::string Build;
  std::string Park;
  uint64_t Ticks = 0;
  size_t WarmUps = 0;
  size_t Repetitions = 0;
  std::vector<StageResult> Stages;

  bool operator==(const Launch &) const = default;
};

// Reads launches, each a line `build <name>` followed by tpj_bench's park line and its stage
// lines. A carriage return ending a line is ignored, and so is an empty line. Throws ReportError,
// its message beginning `line <n>: `, at the first line that breaks the form.
std::vector<Launch> parseLaunches(std::string_view text);

} // namespace tpj

#endif
```

src/bench/report/launches.cpp, a stub:

```cpp
#include "bench/report/launches.h"

namespace tpj {

std::vector<Launch> parseLaunches(std::string_view /*text*/) { return {}; }

} // namespace tpj
```

Step 3: src/bench/report/report.h:

```cpp
#ifndef TPJ_BENCH_REPORT_REPORT_H
#define TPJ_BENCH_REPORT_REPORT_H

#include "bench/report/launches.h"
#include "bench/stages.h"

#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// One park on one build in a report: the launches summarized, and each stage with the summary of
// the launches' medians as its times.
struct ReportPark {
  std::string Build;
  std::string Park;
  size_t Launches = 0;
  uint64_t Ticks = 0;
  size_t WarmUps = 0;
  size_t Repetitions = 0;
  std::vector<StageResult> Stages;

  bool operator==(const ReportPark &) const = default;
};

// One ReportPark per build and park, in the order each pair first appears. Throws ReportError,
// naming the build and park, when launches of one build and park did not do the same work.
std::vector<ReportPark> summarizeLaunches(std::span<const Launch> launches);

// Each park as `park <build> <path> launches <n> ticks <t> warm-ups <w> repetitions <r>`, then
// its stage lines, every line ending with a line feed.
std::string reportText(std::span<const ReportPark> report);

// Reads what reportText writes. Throws ReportError, its message beginning `line <n>: `, at the
// first line that breaks the form.
std::vector<ReportPark> parseReport(std::string_view text);

} // namespace tpj

#endif
```

src/bench/report/report.cpp, a stub:

```cpp
#include "bench/report/report.h"

namespace tpj {

std::vector<ReportPark> summarizeLaunches(std::span<const Launch> /*launches*/) { return {}; }

std::string reportText(std::span<const ReportPark> /*report*/) { return {}; }

std::vector<ReportPark> parseReport(std::string_view /*text*/) { return {}; }

} // namespace tpj
```

Step 4: src/bench/report/comparison.h:

```cpp
#ifndef TPJ_BENCH_REPORT_COMPARISON_H
#define TPJ_BENCH_REPORT_COMPARISON_H

#include "bench/report/report.h"
#include "bench/stages.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tpj {

// Whether a stage's launches in one report all lie beyond its launches in the other.
enum class ChangeKind { Faster, Slower, Unclear };

// One stage of one park on one build, in either report or both.
struct StageComparison {
  std::string Build;
  std::string Park;
  std::string Stage;
  std::optional<StageResult> Before;
  std::optional<StageResult> After;
  // Faster when After's Greatest is less than Before's Least, Slower when After's Least is greater
  // than Before's Greatest, and Unclear otherwise or when either is empty.
  ChangeKind Change = ChangeKind::Unclear;
  // Whether both are present and differ in Kind or Result.
  bool ResultChanged = false;

  bool operator==(const StageComparison &) const = default;
};

// One comparison per build, park, and stage name in either report: before's in its order, then
// those only in after, in its order.
std::vector<StageComparison> compareReports(std::span<const ReportPark> before,
                                            std::span<const ReportPark> after);

// One line per comparison, times in microseconds, columns aligned, then `clear <n> of <m>`.
std::string comparisonText(std::span<const StageComparison> comparisons);

} // namespace tpj

#endif
```

src/bench/report/comparison.cpp, a stub:

```cpp
#include "bench/report/comparison.h"

namespace tpj {

std::vector<StageComparison> compareReports(std::span<const ReportPark> /*before*/,
                                            std::span<const ReportPark> /*after*/) {
  return {};
}

std::string comparisonText(std::span<const StageComparison> /*comparisons*/) { return {}; }

} // namespace tpj
```

Step 5: src/bench/report/report_options.h:

```cpp
#ifndef TPJ_BENCH_REPORT_REPORT_OPTIONS_H
#define TPJ_BENCH_REPORT_REPORT_OPTIONS_H

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tpj {

// What tpj_bench_report is asked to do.
enum class ReportCommandKind { Summarize, Compare };

// tpj_bench_report's command and the files it reads.
struct ReportOptions {
  ReportCommandKind Command = ReportCommandKind::Summarize;
  std::vector<std::string> Paths;

  bool operator==(const ReportOptions &) const = default;
};

// Reads tpj_bench_report summarize LAUNCHES or compare BEFORE AFTER, the first argument being
// the program's name. None, with error naming the problem, for no command, an unknown command, or
// a count of files other than one for summarize and two for compare.
std::optional<ReportOptions> parseReportOptions(std::span<const std::string> arguments,
                                                std::string &error);

} // namespace tpj

#endif
```

src/bench/report/report_options.cpp, a stub:

```cpp
#include "bench/report/report_options.h"

namespace tpj {

std::optional<ReportOptions> parseReportOptions(std::span<const std::string> /*arguments*/,
                                                std::string & /*error*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 6: src/bench/report/main.cpp, a stub:

```cpp
// tpj_bench_report: summarizes tpj_bench's launches into a report, and compares two reports.
//
//   tpj_bench_report summarize LAUNCHES
//   tpj_bench_report compare BEFORE AFTER

int main() { return 1; }
```

### Task 6: Build the report into bench

Files:
- Modify: `src/bench/CMakeLists.txt`

Step 1: Replace the whole file with:

```cmake
# The runtime runner (plans/scalable-runtime). It reads the clock to time each runtime stage on a
# park file and is not simulation code, so it builds with the project's usual flags. report/
# summarizes the runner's launches and compares two reports, and reads no clock.
add_library(tpj_bench_lib STATIC
    options.cpp
    report/comparison.cpp
    report/internal/lines.cpp
    report/launches.cpp
    report/report.cpp
    report/report_options.cpp
    stages.cpp
    text_file.cpp
    timing.cpp)
target_include_directories(tpj_bench_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_bench_lib PUBLIC tpj_views tpj_legible tpj_render tpj_sim)
tpj_configure_target(tpj_bench_lib)

add_executable(tpj_bench main.cpp)
target_link_libraries(tpj_bench PRIVATE tpj_bench_lib)
tpj_configure_target(tpj_bench)

add_executable(tpj_bench_report report/main.cpp)
target_link_libraries(tpj_bench_report PRIVATE tpj_bench_lib)
tpj_configure_target(tpj_bench_report)
```

Step 2: Create src/bench/report/internal/lines.h and src/bench/report/internal/lines.cpp as stubs, so the library builds. Task 9 writes their content. lines.h:

```cpp
#ifndef TPJ_BENCH_REPORT_INTERNAL_LINES_H
#define TPJ_BENCH_REPORT_INTERNAL_LINES_H

namespace tpj {} // namespace tpj

#endif
```

lines.cpp:

```cpp
#include "bench/report/internal/lines.h"
```

### Task 7: Build the interfaces

Step 1: Run: `cmake.exe --build --preset windows-debug --target tpj_bench tpj_bench_report`
Expected: both build with no warnings.

### Task 8: Test pass

Step 1: Run the test pass as implementing-features describes, with the feature plans/scalable-runtime/measured-runtime/runtime-report/FEATURE.md, the spec src/bench/SPEC.md, and the public headers:
- src/bench/report/report_error.h
- src/bench/report/launches.h
- src/bench/report/report.h
- src/bench/report/comparison.h
- src/bench/report/report_options.h
- src/bench/stages.h
- src/bench/timing.h
- src/bench/text_file.h

It writes these tests in tests/bench/report/, and adds them to tpj_bench_tests in tests/bench/CMakeLists.txt with a TPJ_BENCH_REPORT_EXECUTABLE compile definition and a dependency on tpj_bench_report:
- launches_test.cpp
- report_test.cpp
- comparison_test.cpp
- report_options_test.cpp
- report_command_line_test.cpp

The test names have to differ from tests/bench/options_test.cpp and command_line_test.cpp, so that each file's Catch2 tag is its own. Do not write test content here. The new tests must build and fail on behavior against the stubs.

### Task 9: Read lines and fields

Files:
- Modify: `src/bench/report/internal/lines.h`, `src/bench/report/internal/lines.cpp`

Step 1: Replace src/bench/report/internal/lines.h with:

```cpp
#ifndef TPJ_BENCH_REPORT_INTERNAL_LINES_H
#define TPJ_BENCH_REPORT_INTERNAL_LINES_H

#include "bench/stages.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// A line that is not empty once a carriage return at its end is removed, split at single spaces,
// numbered counting every line of its text from 1. Its fields view the text it was read from.
struct TextLine {
  size_t Number = 0;
  std::vector<std::string_view> Fields;
};

// The lines of text separated by line feeds, without the empty ones. Throws ReportError for a line
// with an empty field.
std::vector<TextLine> readLines(std::string_view text);

// Throws ReportError with the message `line <n>: <problem>`.
[[noreturn]] void refuseLine(const TextLine &line, const std::string &problem);

// Refuses the line unless the field at index is word.
void expectWord(const TextLine &line, size_t index, std::string_view word);

// The field at index as an unsigned decimal count, or refuses the line.
uint64_t readCount(const TextLine &line, size_t index);

// The field at index as a duration in nanoseconds that fits int64_t, or refuses the line.
int64_t readTime(const TextLine &line, size_t index);

// The field at index as 16 lowercase hexadecimal digits, or refuses the line.
uint64_t readHash(const TextLine &line, size_t index);

// A line as stageLine writes it, or refuses the line.
StageResult readStageLine(const TextLine &line);

} // namespace tpj

#endif
```

Step 2: Replace src/bench/report/internal/lines.cpp with:

```cpp
#include "bench/report/internal/lines.h"

#include "bench/report/report_error.h"
#include "bench/timing.h"

#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace tpj {

namespace {

std::string fieldName(size_t index) { return "field " + std::to_string(index + 1); }

bool isLowerHexDigit(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }

} // namespace

std::vector<TextLine> readLines(std::string_view text) {
  std::vector<TextLine> lines;
  size_t number = 0;
  while (!text.empty()) {
    ++number;
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view() : text.substr(end + 1);
    if (line.ends_with('\r')) {
      line.remove_suffix(1);
    }
    if (line.empty()) {
      continue;
    }
    TextLine read{number, {}};
    size_t start = 0;
    while (true) {
      const size_t space = line.find(' ', start);
      read.Fields.push_back(line.substr(start, space == std::string_view::npos ? space : space - start));
      if (space == std::string_view::npos) {
        break;
      }
      start = space + 1;
    }
    if (std::ranges::any_of(read.Fields, [](std::string_view field) { return field.empty(); })) {
      refuseLine(read, "fields are separated by single spaces");
    }
    lines.push_back(std::move(read));
  }
  return lines;
}

void refuseLine(const TextLine &line, const std::string &problem) {
  throw ReportError("line " + std::to_string(line.Number) + ": " + problem);
}

void expectWord(const TextLine &line, size_t index, std::string_view word) {
  if (index >= line.Fields.size() || line.Fields[index] != word) {
    refuseLine(line, "expected '" + std::string(word) + "' as " + fieldName(index));
  }
}

uint64_t readCount(const TextLine &line, size_t index) {
  if (index < line.Fields.size()) {
    const std::string_view field = line.Fields[index];
    uint64_t value = 0;
    const char *end = field.data() + field.size();
    const auto result = std::from_chars(field.data(), end, value);
    if (result.ec == std::errc() && result.ptr == end) {
      return value;
    }
  }
  refuseLine(line, "expected an unsigned decimal count as " + fieldName(index));
}

int64_t readTime(const TextLine &line, size_t index) {
  const uint64_t value = readCount(line, index);
  if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    refuseLine(line, "the time in " + fieldName(index) + " is too large");
  }
  return static_cast<int64_t>(value);
}

uint64_t readHash(const TextLine &line, size_t index) {
  if (index < line.Fields.size()) {
    const std::string_view field = line.Fields[index];
    if (field.size() == 16 && std::ranges::all_of(field, isLowerHexDigit)) {
      uint64_t value = 0;
      const auto result = std::from_chars(field.data(), field.data() + field.size(), value, 16);
      if (result.ec == std::errc()) {
        return value;
      }
    }
  }
  refuseLine(line, "expected 16 lowercase hexadecimal digits as " + fieldName(index));
}

StageResult readStageLine(const TextLine &line) {
  if (line.Fields.size() != 12) {
    refuseLine(line, "a stage line has 12 fields");
  }
  expectWord(line, 0, "stage");
  expectWord(line, 2, "count");
  expectWord(line, 4, "median");
  expectWord(line, 6, "least");
  expectWord(line, 8, "greatest");
  StageResult stage;
  stage.Name = std::string(line.Fields[1]);
  stage.Times = TimeSummary{static_cast<size_t>(readCount(line, 3)), readTime(line, 5),
                            readTime(line, 7), readTime(line, 9)};
  if (line.Fields[10] == "hash") {
    stage.Kind = ResultKind::Hash;
    stage.Result = readHash(line, 11);
  } else if (line.Fields[10] == "vertices") {
    stage.Kind = ResultKind::Vertices;
    stage.Result = readCount(line, 11);
  } else {
    refuseLine(line, "expected 'hash' or 'vertices' as " + fieldName(10));
  }
  return stage;
}

} // namespace tpj
```

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_lib`
Expected: it builds with no warnings.

### Task 10: Read launches

Files:
- Modify: `src/bench/report/launches.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/report/launches.h"

#include "bench/report/internal/lines.h"

#include <algorithm>
#include <utility>

namespace tpj {

namespace {

// The launch a build line and the park line after it begin.
Launch readLaunchHead(const TextLine &build, const TextLine &park) {
  if (build.Fields.size() != 2) {
    refuseLine(build, "a build line is 'build <name>'");
  }
  if (park.Fields.front() != "park" || park.Fields.size() != 8) {
    refuseLine(park, "expected tpj_bench's park line after a build line");
  }
  expectWord(park, 2, "ticks");
  expectWord(park, 4, "warm-ups");
  expectWord(park, 6, "repetitions");
  Launch launch;
  launch.Build = std::string(build.Fields[1]);
  launch.Park = std::string(park.Fields[1]);
  launch.Ticks = readCount(park, 3);
  launch.WarmUps = static_cast<size_t>(readCount(park, 5));
  launch.Repetitions = static_cast<size_t>(readCount(park, 7));
  return launch;
}

} // namespace

std::vector<Launch> parseLaunches(std::string_view text) {
  const std::vector<TextLine> lines = readLines(text);
  std::vector<Launch> launches;
  for (size_t i = 0; i < lines.size(); ++i) {
    const TextLine &line = lines[i];
    if (line.Fields.front() == "build") {
      if (i + 1 == lines.size()) {
        refuseLine(line, "a build line must be followed by a park line");
      }
      ++i;
      launches.push_back(readLaunchHead(line, lines[i]));
    } else if (line.Fields.front() == "stage" && !launches.empty()) {
      StageResult stage = readStageLine(line);
      std::vector<StageResult> &stages = launches.back().Stages;
      if (std::ranges::any_of(stages, [&stage](const StageResult &read) { return read.Name == stage.Name; })) {
        refuseLine(line, "repeats the stage " + stage.Name);
      }
      stages.push_back(std::move(stage));
    } else {
      refuseLine(line, launches.empty() ? "expected a build line"
                                        : "expected a build line or a stage line");
    }
  }
  return launches;
}

} // namespace tpj
```

Step 2: Build and run the launches tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#launches_test]"`
Expected: every test passes.

### Task 11: Summarize launches into a report

Files:
- Modify: `src/bench/report/report.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/report/report.h"

#include "bench/report/internal/lines.h"
#include "bench/report/report_error.h"
#include "bench/timing.h"

#include <algorithm>
#include <utility>

namespace tpj {

namespace {

[[noreturn]] void refuseGroup(const Launch &launch, const std::string &problem) {
  throw ReportError("launches of " + launch.Park + " on " + launch.Build + " " + problem);
}

// Refuses the launches unless every one did the work the first did.
void checkSameWork(const std::vector<const Launch *> &group) {
  const Launch &first = *group.front();
  for (const Launch *launch : group) {
    if (launch->Ticks != first.Ticks || launch->WarmUps != first.WarmUps ||
        launch->Repetitions != first.Repetitions) {
      refuseGroup(first, "differ in their ticks, warm-ups, or repetitions");
    }
    if (launch->Stages.size() != first.Stages.size()) {
      refuseGroup(first, "differ in their stages");
    }
    for (size_t s = 0; s < first.Stages.size(); ++s) {
      const StageResult &stage = launch->Stages[s];
      const StageResult &firstStage = first.Stages[s];
      if (stage.Name != firstStage.Name) {
        refuseGroup(first, "differ in their stages");
      }
      if (stage.Kind != firstStage.Kind || stage.Result != firstStage.Result) {
        refuseGroup(first, "differ in the result of stage " + firstStage.Name);
      }
    }
  }
}

ReportPark summarizeGroup(const std::vector<const Launch *> &group) {
  checkSameWork(group);
  const Launch &first = *group.front();
  ReportPark park{first.Build, first.Park, group.size(), first.Ticks,
                  first.WarmUps, first.Repetitions, {}};
  for (size_t s = 0; s < first.Stages.size(); ++s) {
    std::vector<int64_t> medians;
    medians.reserve(group.size());
    for (const Launch *launch : group) {
      medians.push_back(launch->Stages[s].Times.Median);
    }
    StageResult stage = first.Stages[s];
    stage.Times = summarizeTimes(medians);
    park.Stages.push_back(std::move(stage));
  }
  return park;
}

ReportPark readReportParkLine(const TextLine &line) {
  if (line.Fields.size() != 11) {
    refuseLine(line, "a report's park line has 11 fields");
  }
  expectWord(line, 3, "launches");
  expectWord(line, 5, "ticks");
  expectWord(line, 7, "warm-ups");
  expectWord(line, 9, "repetitions");
  return ReportPark{std::string(line.Fields[1]),
                    std::string(line.Fields[2]),
                    static_cast<size_t>(readCount(line, 4)),
                    readCount(line, 6),
                    static_cast<size_t>(readCount(line, 8)),
                    static_cast<size_t>(readCount(line, 10)),
                    {}};
}

} // namespace

std::vector<ReportPark> summarizeLaunches(std::span<const Launch> launches) {
  std::vector<std::vector<const Launch *>> groups;
  for (const Launch &launch : launches) {
    const auto group = std::ranges::find_if(groups, [&launch](const std::vector<const Launch *> &members) {
      return members.front()->Build == launch.Build && members.front()->Park == launch.Park;
    });
    if (group == groups.end()) {
      groups.push_back({&launch});
    } else {
      group->push_back(&launch);
    }
  }
  std::vector<ReportPark> report;
  report.reserve(groups.size());
  for (const std::vector<const Launch *> &group : groups) {
    report.push_back(summarizeGroup(group));
  }
  return report;
}

std::string reportText(std::span<const ReportPark> report) {
  std::string text;
  for (const ReportPark &park : report) {
    text += "park " + park.Build + " " + park.Park + " launches " + std::to_string(park.Launches) +
            " ticks " + std::to_string(park.Ticks) + " warm-ups " + std::to_string(park.WarmUps) +
            " repetitions " + std::to_string(park.Repetitions) + "\n";
    for (const StageResult &stage : park.Stages) {
      text += stageLine(stage) + "\n";
    }
  }
  return text;
}

std::vector<ReportPark> parseReport(std::string_view text) {
  std::vector<ReportPark> report;
  for (const TextLine &line : readLines(text)) {
    if (line.Fields.front() == "park") {
      ReportPark park = readReportParkLine(line);
      const bool isRepeated = std::ranges::any_of(report, [&park](const ReportPark &read) {
        return read.Build == park.Build && read.Park == park.Park;
      });
      if (isRepeated) {
        refuseLine(line, "repeats the park " + park.Park + " on " + park.Build);
      }
      report.push_back(std::move(park));
    } else if (line.Fields.front() == "stage" && !report.empty()) {
      StageResult stage = readStageLine(line);
      std::vector<StageResult> &stages = report.back().Stages;
      if (std::ranges::any_of(stages, [&stage](const StageResult &read) { return read.Name == stage.Name; })) {
        refuseLine(line, "repeats the stage " + stage.Name);
      }
      stages.push_back(std::move(stage));
    } else {
      refuseLine(line, report.empty() ? "expected a park line"
                                      : "expected a park line or a stage line");
    }
  }
  return report;
}

} // namespace tpj
```

Step 2: Build and run the report tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#report_test]"`
Expected: every test passes.

### Task 12: Compare two reports

Files:
- Modify: `src/bench/report/comparison.cpp`

Step 1: Replace the whole file with the following. comparisonText keeps its stub until Task 13.

```cpp
#include "bench/report/comparison.h"

#include "bench/timing.h"

#include <algorithm>
#include <utility>

namespace tpj {

namespace {

// The stage of that name in that park on that build, or null.
const StageResult *findStage(std::span<const ReportPark> report, const std::string &build,
                             const std::string &park, const std::string &stage) {
  for (const ReportPark &read : report) {
    if (read.Build == build && read.Park == park) {
      const auto found = std::ranges::find(read.Stages, stage, &StageResult::Name);
      return found == read.Stages.end() ? nullptr : &*found;
    }
  }
  return nullptr;
}

ChangeKind changeBetween(const TimeSummary &before, const TimeSummary &after) {
  if (after.Greatest < before.Least) {
    return ChangeKind::Faster;
  }
  if (after.Least > before.Greatest) {
    return ChangeKind::Slower;
  }
  return ChangeKind::Unclear;
}

} // namespace

std::vector<StageComparison> compareReports(std::span<const ReportPark> before,
                                            std::span<const ReportPark> after) {
  std::vector<StageComparison> comparisons;
  for (const ReportPark &park : before) {
    for (const StageResult &stage : park.Stages) {
      StageComparison comparison{park.Build, park.Park, stage.Name, stage, std::nullopt,
                                 ChangeKind::Unclear, false};
      const StageResult *other = findStage(after, park.Build, park.Park, stage.Name);
      if (other != nullptr) {
        comparison.After = *other;
        comparison.Change = changeBetween(stage.Times, other->Times);
        comparison.ResultChanged = stage.Kind != other->Kind || stage.Result != other->Result;
      }
      comparisons.push_back(std::move(comparison));
    }
  }
  for (const ReportPark &park : after) {
    for (const StageResult &stage : park.Stages) {
      if (findStage(before, park.Build, park.Park, stage.Name) == nullptr) {
        comparisons.push_back(StageComparison{park.Build, park.Park, stage.Name, std::nullopt,
                                              stage, ChangeKind::Unclear, false});
      }
    }
  }
  return comparisons;
}

std::string comparisonText(std::span<const StageComparison> /*comparisons*/) { return {}; }

} // namespace tpj
```

Step 2: Build and run the comparison tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#comparison_test]"`
Expected: the tests of compareReports pass. The tests of comparisonText still fail, until Task 13.

### Task 13: Write the comparison

Files:
- Modify: `src/bench/report/comparison.cpp`

Step 1: Add `#include <format>`, `#include <string_view>`, and `#include <vector>` to the system includes. Inside the anonymous namespace, after changeBetween, add:

```cpp
std::string microseconds(int64_t nanoseconds) {
  return std::format("{:.1f}", static_cast<double>(nanoseconds) / 1000.0);
}

void appendTimes(std::vector<std::string> &fields, const TimeSummary &times) {
  fields.push_back(microseconds(times.Median));
  fields.push_back("[" + microseconds(times.Least));
  fields.push_back(microseconds(times.Greatest) + "]");
}

std::string changeText(const TimeSummary &before, const TimeSummary &after) {
  if (before.Median == 0) {
    return "n/a";
  }
  const double change = static_cast<double>(after.Median - before.Median) /
                        static_cast<double>(before.Median) * 100.0;
  return std::format("{:+.1f}%", change);
}

std::string_view changeWord(ChangeKind change) {
  switch (change) {
  case ChangeKind::Faster:
    return "faster";
  case ChangeKind::Slower:
    return "slower";
  case ChangeKind::Unclear:
    break;
  }
  return "unclear";
}

std::vector<std::string> comparisonFields(const StageComparison &comparison) {
  std::vector<std::string> fields{comparison.Build, comparison.Park, comparison.Stage};
  if (comparison.Before.has_value() && comparison.After.has_value()) {
    const TimeSummary &before = comparison.Before->Times;
    const TimeSummary &after = comparison.After->Times;
    fields.emplace_back(changeWord(comparison.Change));
    fields.emplace_back("before");
    appendTimes(fields, before);
    fields.emplace_back("after");
    appendTimes(fields, after);
    fields.push_back(changeText(before, after));
    if (comparison.ResultChanged) {
      fields.emplace_back("result-changed");
    }
  } else if (comparison.Before.has_value()) {
    const TimeSummary &before = comparison.Before->Times;
    fields.emplace_back("only-before");
    appendTimes(fields, before);
  } else if (comparison.After.has_value()) {
    const TimeSummary &after = comparison.After->Times;
    fields.emplace_back("only-after");
    appendTimes(fields, after);
  }
  return fields;
}
```

Step 2: Replace comparisonText's stub with:

```cpp
std::string comparisonText(std::span<const StageComparison> comparisons) {
  std::vector<std::vector<std::string>> lines;
  std::vector<size_t> widths;
  size_t compared = 0;
  size_t clear = 0;
  for (const StageComparison &comparison : comparisons) {
    if (comparison.Before.has_value() && comparison.After.has_value()) {
      ++compared;
      if (comparison.Change != ChangeKind::Unclear) {
        ++clear;
      }
    }
    lines.push_back(comparisonFields(comparison));
    const std::vector<std::string> &fields = lines.back();
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
  text += "clear " + std::to_string(clear) + " of " + std::to_string(compared) + "\n";
  return text;
}
```

Step 3: Build and run the comparison tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#comparison_test]"`
Expected: every test passes.

### Task 14: Read tpj_bench_report's command line

Files:
- Modify: `src/bench/report/report_options.cpp`

Step 1: Replace the stub with:

```cpp
#include "bench/report/report_options.h"

#include <stddef.h>

namespace tpj {

std::optional<ReportOptions> parseReportOptions(std::span<const std::string> arguments,
                                                std::string &error) {
  if (arguments.size() < 2) {
    error = "no command given";
    return std::nullopt;
  }
  const std::string &command = arguments[1];
  ReportOptions options;
  size_t files = 0;
  if (command == "summarize") {
    options.Command = ReportCommandKind::Summarize;
    files = 1;
  } else if (command == "compare") {
    options.Command = ReportCommandKind::Compare;
    files = 2;
  } else {
    error = "unknown command " + command;
    return std::nullopt;
  }
  options.Paths.assign(arguments.begin() + 2, arguments.end());
  if (options.Paths.size() != files) {
    error = command + (files == 1 ? " takes one file" : " takes two files");
    return std::nullopt;
  }
  return options;
}

} // namespace tpj
```

Step 2: Build and run the options tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe -# "[#report_options_test]"`
Expected: every test passes.

### Task 15: Compose tpj_bench_report

Files:
- Modify: `src/bench/report/main.cpp`

Step 1: Replace the stub with:

```cpp
// tpj_bench_report: summarizes tpj_bench's launches into a report, and compares two reports.
//
//   tpj_bench_report summarize LAUNCHES
//   tpj_bench_report compare BEFORE AFTER

#include "bench/report/comparison.h"
#include "bench/report/launches.h"
#include "bench/report/report.h"
#include "bench/report/report_error.h"
#include "bench/report/report_options.h"
#include "bench/text_file.h"

#include <exception>
#include <iostream>
#include <optional>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  const std::vector<std::string> arguments(argv, argv + argc);
  std::string error;
  const std::optional<tpj::ReportOptions> options = tpj::parseReportOptions(arguments, error);
  if (!options) {
    std::cerr << "tpj_bench_report: " << error
              << "\nusage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER\n";
    return 2;
  }
  std::vector<std::string> texts;
  for (const std::string &path : options->Paths) {
    std::optional<std::string> text = tpj::readTextFile(path);
    if (!text) {
      std::cerr << "tpj_bench_report: cannot read " << path << '\n';
      return 1;
    }
    texts.push_back(std::move(*text));
  }
  // The file being read, which a ReportError's message names.
  size_t reading = 0;
  try {
    std::string output;
    if (options->Command == tpj::ReportCommandKind::Summarize) {
      output = tpj::reportText(tpj::summarizeLaunches(tpj::parseLaunches(texts[0])));
    } else {
      const std::vector<tpj::ReportPark> before = tpj::parseReport(texts[0]);
      reading = 1;
      const std::vector<tpj::ReportPark> after = tpj::parseReport(texts[1]);
      output = tpj::comparisonText(tpj::compareReports(before, after));
    }
    std::cout << output;
  } catch (const tpj::ReportError &failure) {
    std::cerr << "tpj_bench_report: " << options->Paths[reading] << ": " << failure.what() << '\n';
    return 1;
  } catch (const std::exception &failure) {
    std::cerr << "tpj_bench_report: " << failure.what() << '\n';
    return 1;
  }
  return 0;
}
```

Step 2: Build and run every bench test.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench_tests && build/windows-debug/tpj_bench_tests.exe`
Expected: every test passes, the stage-runner's tests included.

### Task 16: Write the script

Files:
- Create: `scripts/runtime-report.sh`

Step 1: Create scripts/runtime-report.sh with:

```bash
#!/bin/bash
# runtime-report: times the gameplay runtime on the stress parks (src/bench/SPEC.md).
#
#   scripts/runtime-report.sh [--launches N] REPORT
#   scripts/runtime-report.sh --compare BEFORE AFTER
#
# The first builds tpj_bench with windows-release and windows-debug, launches it N times, 10 by
# default, on every park in tests/parks/stress/ with each build, in rounds, and writes the report
# tpj_bench_report makes of the launches to REPORT. The second prints tpj_bench_report's
# comparison of two reports. Run from WSL. Nothing checks a time: reports are read, never gated on.
set -uo pipefail
export LC_ALL=C

say() { echo "runtime-report: $*" >&2; }
fail() {
    echo "runtime-report: $*" >&2
    exit 1
}
usage() {
    echo "usage: scripts/runtime-report.sh [--launches N] REPORT" >&2
    echo "       scripts/runtime-report.sh --compare BEFORE AFTER" >&2
    exit 2
}

# Configures the preset when it has no build directory yet, and builds the targets given.
build() {
    local preset=$1
    shift
    if [ ! -f "build/$preset/CMakeCache.txt" ]; then
        say "configuring $preset"
        cmake.exe --preset "$preset" >&2 || fail "$preset failed to configure"
    fi
    say "building $* with $preset"
    cmake.exe --build --preset "$preset" --target "$@" >&2 || fail "$preset failed to build"
}

if [ "$(uname -s)" != "Linux" ] || ! command -v cmake.exe >/dev/null ||
    ! command -v wslpath >/dev/null; then
    fail "run this from WSL with cmake.exe on the PATH"
fi

# Paths the user gives are made absolute before moving to the repository's root.
launches=10
if [ "${1:-}" = --compare ]; then
    [ $# -eq 3 ] || usage
    [ -f "$2" ] || fail "no report at $2"
    [ -f "$3" ] || fail "no report at $3"
    before=$(realpath -- "$2")
    after=$(realpath -- "$3")
    cd "$(git rev-parse --show-toplevel)" || exit 1
    build windows-release tpj_bench_report
    build/windows-release/tpj_bench_report.exe compare "$(wslpath -w "$before")" \
        "$(wslpath -w "$after")" | tr -d '\r'
    exit "${PIPESTATUS[0]}"
fi
if [ "${1:-}" = --launches ]; then
    [ $# -ge 2 ] || usage
    [[ "$2" =~ ^[1-9][0-9]*$ ]] || fail "--launches takes a positive count, not '$2'"
    launches=$2
    shift 2
fi
[ $# -eq 1 ] || usage
case "$1" in -*) usage ;; esac
report=$(realpath -m -- "$1")
cd "$(git rev-parse --show-toplevel)" || exit 1

start=$SECONDS
build windows-release tpj_bench tpj_bench_report
build windows-debug tpj_bench

shopt -s nullglob
parks=(tests/parks/stress/*.park)
[ ${#parks[@]} -gt 0 ] || fail "no stress parks in tests/parks/stress/"
out=build/runtime-report
mkdir -p "$out"
: >"$out/launches.txt"

# Rounds: each park's launches span the whole report, so drift during it widens every park's
# spread alike rather than moving one park's median.
for ((round = 1; round <= launches; round++)); do
    say "round $round of $launches"
    for preset in windows-release windows-debug; do
        for park in "${parks[@]}"; do
            echo "build $preset" >>"$out/launches.txt"
            "build/$preset/tpj_bench.exe" "$park" | tr -d '\r' >>"$out/launches.txt" ||
                fail "tpj_bench failed on $park with $preset in round $round"
        done
    done
done

build/windows-release/tpj_bench_report.exe summarize "$out/launches.txt" | tr -d '\r' \
    >"$out/report.txt" || fail "summarizing the launches failed"
cp "$out/report.txt" "$report" || fail "cannot write $report"
elapsed=$((SECONDS - start))
echo "runtime-report: wrote $report in $((elapsed / 60))m $(printf '%02d' $((elapsed % 60)))s"
```

Step 2: Make it executable.

Run: `chmod +x scripts/runtime-report.sh && git add scripts/runtime-report.sh && git update-index --chmod=+x scripts/runtime-report.sh`
Expected: no output.

Step 3: Check its refusals.

Run: `scripts/runtime-report.sh; echo "status $?"; scripts/runtime-report.sh --launches 0 x; echo "status $?"; scripts/runtime-report.sh --compare nowhere elsewhere; echo "status $?"`
Expected: the usage lines and `status 2`; then `runtime-report: --launches takes a positive count, not '0'` and `status 1`; then `runtime-report: no report at nowhere` and `status 1`.

### Task 17: Report the unchanged tree twice

Step 1: Make two reports of the unchanged tree and compare them.

Run: `scripts/runtime-report.sh build/runtime-report/first.txt && scripts/runtime-report.sh build/runtime-report/second.txt && scripts/runtime-report.sh --compare build/runtime-report/first.txt build/runtime-report/second.txt`
Expected: each report ends with `runtime-report: wrote <path> in <m>m <ss>s`. first.txt holds a park line for winding-path.park on each build, each with `launches 10` and six stage lines, and each stage's result is the same on both builds. The comparison has one line per stage of each build, each marked unclear, and ends with `clear 0 of 12`. The feature's report gives first.txt, the comparison, and both durations. A clear change here is a deviation: report it rather than rerunning until none shows.

### Task 18: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: everything builds with no warnings, and every test passes.

Step 2: Run: `cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: everything builds with no warnings, and every test passes.

Step 3: Run: `scripts/tidy.sh`
Expected: no findings.

### Task 19: Commit

Step 1: Commit the feature as one commit through the commit-hygiene skill, subject `Bench: Add the runtime report and its script`. Stage with `git add -A && git reset -q parks/`, so the untracked parks/routes.park and parks/sketch.park stay out.
