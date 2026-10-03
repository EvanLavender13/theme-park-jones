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
      if (line.Fields.size() != 12) {
        refuseLine(line, "a stage line has 12 fields");
      }
      StageResult stage = readStageLine(line);
      std::vector<StageResult> &stages = launches.back().Stages;
      if (std::ranges::any_of(
              stages, [&stage](const StageResult &read) { return read.Name == stage.Name; })) {
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
