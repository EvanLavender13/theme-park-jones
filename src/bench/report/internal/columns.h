#ifndef TPJ_BENCH_REPORT_INTERNAL_COLUMNS_H
#define TPJ_BENCH_REPORT_INTERNAL_COLUMNS_H

#include "bench/timing.h"

#include <span>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// Nanoseconds as microseconds with one decimal place.
std::string microseconds(int64_t nanoseconds);

// A summary's median, [least, and greatest], in microseconds.
void appendTimes(std::vector<std::string> &fields, const TimeSummary &times);

// Each line's fields, each but its last followed by enough spaces to reach the widest field in its
// position on any line, and one more, every line ending with a line feed.
std::string alignedText(std::span<const std::vector<std::string>> lines);

} // namespace tpj

#endif
