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

TEST_CASE("No frame steps more than MAX_FRAME_TICKS, which is 2") {
  CHECK(MAX_FRAME_TICKS == 2);
  FrameClock clock(0, FREQUENCY);
  // 3.5 ticks: more than the cap holds, though under MAX_FRAME_SECONDS.
  CHECK(clock.advance(350'000).Ticks == MAX_FRAME_TICKS);
  // A stall clamped to MAX_FRAME_SECONDS, 7.5 ticks.
  CHECK(clock.advance(10'350'000).Ticks == MAX_FRAME_TICKS);
}

TEST_CASE("While no frame holds more than MAX_FRAME_TICKS whole ticks, the ticks given so far are "
          "the whole ticks the Dts given so far hold, the rest carrying to later frames") {
  // Frames of 1.5, 0.4, 0.3, 1.9, and 0.05 ticks: frames that step one, none, and two ticks, with
  // the running total never near a whole tick, where rounding may go either way.
  const std::array<uint64_t, 5> readings{150'000, 190'000, 220'000, 410'000, 415'000};
  FrameClock clock(0, FREQUENCY);
  double seconds = 0.0;
  uint64_t ticks = 0;
  for (const uint64_t reading : readings) {
    const FrameStep step = clock.advance(reading);
    seconds += step.Dt;
    ticks += step.Ticks;
    CHECK(ticks == static_cast<uint64_t>(std::floor(seconds / SIM_TICK_SECONDS)));
  }
  CHECK(ticks == 4);
}

TEST_CASE("A frame over MAX_FRAME_TICKS drops the whole ticks beyond it and carries the part of a "
          "tick left over to the next frame") {
  // A frame of 3.4 ticks steps two, drops one, and leaves 0.4. The next frame of 0.8 ticks then
  // holds 1.2 and steps one: two if the dropped tick carried, none if the 0.4 were lost.
  FrameClock clock(0, FREQUENCY);
  clock.advance(340'000);
  CHECK(clock.advance(420'000).Ticks == 1);
}

} // namespace
} // namespace tpj
