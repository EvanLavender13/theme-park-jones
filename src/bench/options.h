#ifndef TPJ_BENCH_OPTIONS_H
#define TPJ_BENCH_OPTIONS_H

#include "bench/stages.h"
#include "scenarios/full_park.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <string>

namespace tpj {

// What tpj_bench's command line asks for.
struct BenchOptions {
  uint64_t Ticks = DEFAULT_TICKS;
  std::string Park;
  // The path --full-park names, or empty.
  std::string FullPark;
  // The full park's warm-up, which --warm-ticks sets.
  uint64_t WarmTicks = FULL_PARK_WARM_TICKS;

  bool operator==(const BenchOptions &) const = default;
};

// Reads tpj_bench [--ticks N] FILE or tpj_bench --full-park PATH [--warm-ticks N], the first
// argument being the program's name. An argument that begins with -- is an option, and any other
// is the file. None, with error naming the problem, for an unknown option, --ticks or --warm-ticks
// with no value or one that is not a positive decimal count, --full-park with no value, with a
// file, or with --ticks, --warm-ticks without --full-park, no file without --full-park, or more
// than one.
std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error);

} // namespace tpj

#endif
