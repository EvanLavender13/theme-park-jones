#ifndef TPJ_SCENARIOS_FULL_PARK_H
#define TPJ_SCENARIOS_FULL_PARK_H

#include "sim/world.h"

#include <stdint.h>

namespace tpj {

// The full park's seed, its added guests, and the ticks it steps before it is saved.
inline constexpr uint64_t FULL_PARK_SEED = 1;
inline constexpr uint64_t FULL_PARK_GUESTS = 2000;
inline constexpr uint64_t FULL_PARK_WARM_TICKS = 1800;

// The full park: a grid of guest paths with 30 shops, a third each selling food, drink, and both,
// supplied by 3 depots over backstage paths, 15 toilet blocks, 45 benches, and 2,000 added guests,
// stepped warmTicks ticks into play. Throws std::logic_error
// when a command is refused, a shop is starved, or a bench or toilet block has no guest connector.
World makeFullPark(uint64_t warmTicks);

} // namespace tpj

#endif
