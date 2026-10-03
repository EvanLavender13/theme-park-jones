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
  const std::vector<TextLine> lines = readLines(output);
  const TextLine *timesLine = nullptr;
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
