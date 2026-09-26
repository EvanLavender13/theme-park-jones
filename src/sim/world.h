#ifndef TPJ_SIM_WORLD_H
#define TPJ_SIM_WORLD_H

#include <stdint.h>

namespace tpj {

// Simulation time advances in fixed ticks, independent of the render frame rate.
constexpr double SIM_TICK_SECONDS = 1.0 / 30.0;

struct World {
  uint64_t Tick = 0;
};

void stepWorld(World &world);

} // namespace tpj

#endif
