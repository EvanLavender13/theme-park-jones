#include "app/frame_times.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

std::vector<int64_t> keptOf(const FrameTimes &times) {
  return {times.kept().begin(), times.kept().end()};
}

TEST_CASE("A FrameTimes keeps every value from the third on, in the order given, and drops the "
          "first two") {
  // The first two are the largest, as loading and the first frame's one-off work make them, and the
  // kept values are out of order, so neither a sort nor keeping the wrong ones can pass.
  const std::array<int64_t, 5> given{900, 800, 30, 10, 20};
  FrameTimes times;
  CHECK(times.kept().empty());
  times.add(given[0]);
  times.add(given[1]);
  CHECK(times.kept().empty());
  for (size_t index = 2; index < given.size(); ++index) {
    times.add(given[index]);
  }
  CHECK(keptOf(times) == std::vector<int64_t>{30, 10, 20});
}

TEST_CASE("A FrameTimes's line is frame-times followed by each kept value in decimal, each after a "
          "single space, with no line feed") {
  SECTION("no value kept") {
    FrameTimes times;
    times.add(7);
    times.add(8);
    CHECK(times.line() == "frame-times");
  }
  SECTION("values kept") {
    FrameTimes times;
    times.add(7);
    times.add(8);
    times.add(16'666'700);
    // Zero, and a value past 32 bits, as a stalled frame of seconds gives.
    times.add(0);
    times.add(5'000'000'000);
    CHECK(times.line() == "frame-times 16666700 0 5000000000");
  }
}

} // namespace
} // namespace tpj
