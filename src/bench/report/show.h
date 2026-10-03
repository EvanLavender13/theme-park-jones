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
