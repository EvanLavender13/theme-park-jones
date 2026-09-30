#ifndef TPJ_SCENARIOS_SLICE_PARKS_H
#define TPJ_SCENARIOS_SLICE_PARKS_H

#include <stdint.h>
#include <string>
#include <string_view>

namespace tpj {

// The warm-up tests/parks/warm.park takes from fed.park: 64 s of game time, long enough that
// cutting its backstage path strands a waiting guest and a shipment in transit.
inline constexpr uint64_t WARM_TICKS = 1920;

// The boxes-and-tubes slice's three park files, as saves.
struct SliceParks {
  std::string Fed;
  std::string Warm;
  std::string Cut;
};

// Loads the text with makeParkSchema and resolves it, and saves it as Fed. Then steps it warmTicks
// cycles with no commands and saves it as Warm, and steps it one more cycle with a DeletePath
// queued for each backstage path, in ascending key order, and saves it as Cut. Throws LoadError as
// loadWorld does.
SliceParks makeSliceParks(std::string_view text, uint64_t warmTicks);

} // namespace tpj

#endif
