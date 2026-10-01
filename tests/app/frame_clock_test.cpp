#include "app/frame_clock.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <stdint.h>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;

constexpr double SECONDS_TOLERANCE = 1e-12;

// At this frequency one tick is 100000 readings, so frames are easily stated in ticks.
constexpr uint64_t FREQUENCY = 3'000'000;

TEST_CASE("advance gives the seconds since the previous reading, the first since the reading the "
          "clock was constructed with") {
  FrameClock clock(1'000, 1'000);
  CHECK_THAT(clock.advance(1'100).Dt, WithinAbs(0.1, SECONDS_TOLERANCE));
  CHECK_THAT(clock.advance(1'150).Dt, WithinAbs(0.05, SECONDS_TOLERANCE));
  CHECK_THAT(clock.advance(1'150).Dt, WithinAbs(0.0, SECONDS_TOLERANCE));
}

TEST_CASE("advance gives at most MAX_FRAME_SECONDS, which is 0.25") {
  CHECK(MAX_FRAME_SECONDS == 0.25);
  FrameClock clock(0, 1'000);
  CHECK_THAT(clock.advance(250).Dt, WithinAbs(0.25, SECONDS_TOLERANCE));
  CHECK_THAT(clock.advance(1'250).Dt, WithinAbs(0.25, SECONDS_TOLERANCE));
  // A clamped frame still ends at its reading, so the next frame starts there.
  CHECK_THAT(clock.advance(1'350).Dt, WithinAbs(0.1, SECONDS_TOLERANCE));
}

TEST_CASE("The ticks given so far are the whole ticks the Dts given so far hold, the rest carrying "
          "to later frames") {
  // Frames of 1.5, 0.4, 0.3, a stall clamped to 7.5, and 0.05 ticks: frames that step one, none,
  // and several ticks, with the running total never near a whole tick, where rounding may go either
  // way.
  const std::array<uint64_t, 5> readings{150'000, 190'000, 220'000, 1'220'000, 1'225'000};
  FrameClock clock(0, FREQUENCY);
  double seconds = 0.0;
  uint64_t ticks = 0;
  for (const uint64_t reading : readings) {
    const FrameStep step = clock.advance(reading);
    seconds += step.Dt;
    ticks += step.Ticks;
    CHECK(ticks == static_cast<uint64_t>(std::floor(seconds / SIM_TICK_SECONDS)));
  }
  CHECK(ticks == 9);
}

} // namespace
} // namespace tpj
