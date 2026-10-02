#ifndef TPJ_BENCH_OPTIONS_H
#define TPJ_BENCH_OPTIONS_H

#include "bench/stages.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <string>

namespace tpj {

// What tpj_bench's command line asks for.
struct BenchOptions {
  uint64_t Ticks = DEFAULT_TICKS;
  std::string Park;

  bool operator==(const BenchOptions &) const = default;
};

// Reads tpj_bench [--ticks N] FILE, the first argument being the program's name. An argument that
// begins with -- is an option, and any other is the file. None, with error naming the problem, for
// an unknown option, --ticks with no value or one that is not a positive decimal count, no file,
// or more than one.
std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error);

} // namespace tpj

#endif
