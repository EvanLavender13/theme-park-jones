#ifndef TPJ_SCENARIOS_FULL_PARK_H
#define TPJ_SCENARIOS_FULL_PARK_H

#include "sim/world.h"

#include <stdint.h>

namespace tpj {

// The full park's seed, its added guests, the ticks it steps before it is saved, and how long its
// added guests stay past the save.
inline constexpr uint64_t FULL_PARK_SEED = 1;
inline constexpr uint64_t FULL_PARK_GUESTS = 2000;
inline constexpr uint64_t FULL_PARK_WARM_TICKS = 1800;
inline constexpr uint64_t FULL_PARK_STAY = 36000;

// The full park: a grid of guest paths with 30 shops supplied by 3 depots over backstage paths,
// and 2,000 added guests staying FULL_PARK_STAY past the save, stepped warmTicks ticks into play.
// Throws std::logic_error when a command is refused or a shop is starved.
World makeFullPark(uint64_t warmTicks);

} // namespace tpj

#endif
