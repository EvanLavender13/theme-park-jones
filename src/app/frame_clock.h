#ifndef TPJ_APP_FRAME_CLOCK_H
#define TPJ_APP_FRAME_CLOCK_H

#include <stdint.h>

namespace tpj {

// The longest time one frame covers, so a stall does not step a burst of ticks.
inline constexpr double MAX_FRAME_SECONDS = 0.25;

// The most ticks one frame steps at 1x, so ticks slower than the time they cover slow the park
// down instead of making every frame step more of them. The cap grows with the speed.
inline constexpr uint32_t MAX_FRAME_TICKS = 2;

// The longest one frame spends stepping ticks, after its first, so a park whose ticks are slow at a
// high speed runs slower than the speed instead of making each frame long.
inline constexpr double MAX_STEP_SECONDS = 1.0 / 60.0;

// What one frame covers: its elapsed seconds, clamped, and the simulation ticks it steps.
struct FrameStep {
  double Dt = 0.0;
  uint32_t Ticks = 0;
  // The whole nanoseconds since the previous reading, rounded down and never clamped.
  int64_t Nanoseconds = 0;

  bool operator==(const FrameStep &) const = default;
};

// Turns performance counter readings into each frame's time and ticks, carrying the time a frame
// does not step to the next.
class FrameClock {
public:
  // Starts at a counter reading, the counter advancing frequency readings per second.
  FrameClock(uint64_t counter, uint64_t frequency) : Last(counter), Frequency(frequency) {}

  // For the reading at a frame's start: the seconds since the last reading, at most
  // MAX_FRAME_SECONDS, and as many SIM_TICK_SECONDS ticks as the time not yet stepped then holds,
  // at most MAX_FRAME_TICKS times the multiplier, keeping the part of a tick left over and dropping
  // the whole ticks beyond the cap. The time a frame owes the park is its seconds times the
  // multiplier, so a multiplier of 0, while paused, owes none.
  FrameStep advance(uint64_t counter, uint32_t multiplier);
  // Whether a frame whose ticks started at the reading started may step another at the reading
  // counter: while its ticks have taken less than MAX_STEP_SECONDS.
  [[nodiscard]] bool mayStep(uint64_t started, uint64_t counter) const;

private:
  uint64_t Last;
  uint64_t Frequency;
  double Unstepped = 0.0;
};

} // namespace tpj

#endif
