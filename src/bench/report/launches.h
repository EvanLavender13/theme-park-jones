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
