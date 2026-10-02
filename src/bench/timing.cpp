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
