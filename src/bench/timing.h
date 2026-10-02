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
