#include "bench/timing.h"

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

TEST_CASE("summarizeTimes gives the count, the least, the greatest, and the median at index "
          "(Count - 1) / 2 of the durations sorted ascending") {
  struct Case {
    std::string Name;
    std::vector<int64_t> Durations;
    TimeSummary Expected;
  };
  const std::vector<Case> cases = {
      // One duration is its own median, least, and greatest.
      {"a single duration", {700}, {1, 700, 700, 700}},
      // Out of order, so a median read before sorting would be 40, the middle as given.
      {"an odd count, unsorted", {50, 10, 40, 20, 30}, {5, 30, 10, 50}},
      // Of an even count, the lower of the two middle ones, never their mean.
      {"an even count, unsorted", {40, 10, 30, 20}, {4, 20, 10, 40}},
      // Repeated values each count once per occurrence.
      {"repeated durations", {5, 9, 5, 5}, {4, 5, 5, 9}},
  };
  for (const Case &example : cases) {
    INFO(example.Name);
    CHECK(summarizeTimes(example.Durations) == example.Expected);
  }
}

TEST_CASE("summarizeTimes throws std::invalid_argument for an empty list") {
  const std::vector<int64_t> none;
  CHECK_THROWS_AS(summarizeTimes(none), std::invalid_argument);
}

} // namespace
} // namespace tpj
