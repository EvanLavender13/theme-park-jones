#include "app/frame_clock.h"
#include "sim/world.h"

#include <algorithm>

namespace tpj {

FrameStep FrameClock::advance(uint64_t counter) {
  FrameStep step;
  step.Dt = std::min(static_cast<double>(counter - Last) / static_cast<double>(Frequency),
                     MAX_FRAME_SECONDS);
  Last = counter;
  Unstepped += step.Dt;
  while (Unstepped >= SIM_TICK_SECONDS) {
    ++step.Ticks;
    Unstepped -= SIM_TICK_SECONDS;
  }
  return step;
}

} // namespace tpj
