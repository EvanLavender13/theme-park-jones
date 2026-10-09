#include "sim/clock.h"

namespace tpj {

ParkTime parkTime(uint64_t tick) {
  const uint64_t seconds = FIRST_SECOND + tick * GAME_SECONDS_PER_TICK;
  return {seconds / DAY_SECONDS + 1, seconds % DAY_SECONDS};
}

} // namespace tpj
