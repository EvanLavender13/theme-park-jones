#ifndef TPJ_SCENARIOS_SLICE_PARKS_H
#define TPJ_SCENARIOS_SLICE_PARKS_H

#include <stdint.h>
#include <string>
#include <string_view>

namespace tpj {

// The warm-up tests/parks/warm.park takes from fed.park: about 2.6 game hours, long enough that
// cutting its backstage segment strands a waiting guest and a shipment in transit.
inline constexpr uint64_t WARM_TICKS = 4600;
// The warm-up tests/parks/warm-main-street.park takes from main-street.park: about 2.5 game hours,
// long enough that guests fill the park in every activity, some of them heading home.
inline constexpr uint64_t MAIN_STREET_WARM_TICKS = 4530;

// Cycles makeStockedPark steps at most before it gives up.
inline constexpr uint64_t STOCKED_LIMIT_TICKS = 600;

// The boxes-and-tubes slice's three park files, as saves.
struct SliceParks {
  std::string Fed;
  std::string Warm;
  std::string Cut;
};

// Loads the text with makeParkSchema and resolves it, and saves it as Fed. Then steps it warmTicks
// cycles with no commands and saves it as Warm, and steps it one more cycle with a DeleteSegment
// queued for each backstage segment, in ascending key order, and saves it as Cut. Throws LoadError
// as loadWorld does.
SliceParks makeSliceParks(std::string_view text, uint64_t warmTicks);
// Loads the text with makeParkSchema, resolves it, steps it warmTicks cycles with no commands, and
// gives its save. Throws LoadError as loadWorld does.
std::string makeWarmPark(std::string_view text, uint64_t warmTicks);
// Loads the text with makeParkSchema, adds a depot and a backstage path past both shops' backs,
// resolves it, and steps it with no commands until a shop holds food, a shop holds drink, and
// drink is in transit, and gives its save. Throws LoadError as loadWorld does, and
// std::logic_error when the world refuses the depot or the path, or the condition doesn't hold
// within STOCKED_LIMIT_TICKS cycles.
std::string makeStockedPark(std::string_view text);

} // namespace tpj

#endif
