#include "app/frame_clock.h"
#include "sim/world.h"

#include <algorithm>

namespace tpj {

FrameStep FrameClock::advance(uint64_t counter, uint32_t multiplier) {
  FrameStep step;
  const uint64_t elapsed = counter - Last;
  // Whole seconds and the rest apart, so a long gap cannot overflow.
  step.Nanoseconds = static_cast<int64_t>((elapsed / Frequency) * 1'000'000'000 +
                                          (elapsed % Frequency) * 1'000'000'000 / Frequency);
  step.Dt =
      std::min(static_cast<double>(elapsed) / static_cast<double>(Frequency), MAX_FRAME_SECONDS);
  Last = counter;
  Unstepped += step.Dt * static_cast<double>(multiplier);
  while (Unstepped >= SIM_TICK_SECONDS) {
    if (step.Ticks < MAX_FRAME_TICKS * multiplier) {
      ++step.Ticks;
    }
    Unstepped -= SIM_TICK_SECONDS;
  }
  return step;
}

bool FrameClock::mayStep(uint64_t started, uint64_t counter) const {
  return static_cast<double>(counter - started) < MAX_STEP_SECONDS * static_cast<double>(Frequency);
}

} // namespace tpj
