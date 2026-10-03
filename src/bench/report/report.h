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

// A stage of a report: the launches' medians summarized, and the slowest call of any launch.
struct ReportStage {
  StageResult Stage;
  int64_t Worst = 0;

  bool operator==(const ReportStage &) const = default;
};

// One park on one build in a report: the launches summarized, and each stage with the summary of
// the launches' medians as its times.
struct ReportPark {
  std::string Build;
  std::string Park;
  size_t Launches = 0;
  uint64_t Ticks = 0;
  size_t WarmUps = 0;
  size_t Repetitions = 0;
  std::vector<ReportStage> Stages;

  bool operator==(const ReportPark &) const = default;
};

// One ReportPark per build and park, in the order each pair first appears. Throws ReportError,
// naming the build and park, when launches of one build and park did not do the same work.
std::vector<ReportPark> summarizeLaunches(std::span<const Launch> launches);

// Each park as `park <build> <path> launches <n> ticks <t> warm-ups <w> repetitions <r>`, then
// its stage lines, each ending with ` worst <w>`, every line ending with a line feed.
std::string reportText(std::span<const ReportPark> report);

// Reads what reportText writes. Throws ReportError, its message beginning `line <n>: `, at the
// first line that breaks the form.
std::vector<ReportPark> parseReport(std::string_view text);

} // namespace tpj

#endif
