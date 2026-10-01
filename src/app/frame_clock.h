#ifndef TPJ_APP_FRAME_CLOCK_H
#define TPJ_APP_FRAME_CLOCK_H

#include <stdint.h>

namespace tpj {

// The longest time one frame covers, so a stall does not step a burst of ticks.
inline constexpr double MAX_FRAME_SECONDS = 0.25;

// What one frame covers: its elapsed seconds, clamped, and the simulation ticks it steps.
struct FrameStep {
  double Dt = 0.0;
  uint32_t Ticks = 0;

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
  // keeping the rest.
  FrameStep advance(uint64_t counter);

private:
  uint64_t Last;
  uint64_t Frequency;
  double Unstepped = 0.0;
};

} // namespace tpj

#endif
