#ifndef TPJ_SCENARIOS_DAY_PARKS_H
#define TPJ_SCENARIOS_DAY_PARKS_H

#include <stdint.h>
#include <string>

namespace tpj {

// The day parks' seed.
inline constexpr uint64_t DAY_PARK_SEED = 1;
// The cycles noon.park takes from day.park: 3 game hours, from 9:00 to 12:00.
inline constexpr uint64_t NOON_TICKS = 5400;
// The cycles evening.park takes from day.park: 11.5 game hours, from 9:00 to 20:30, so a run of
// 3,000 ticks crosses closing at 21:00.
inline constexpr uint64_t EVENING_TICKS = 20700;
// The meters between day.park's tables.
inline constexpr double DAY_TABLE_SPACING = 6.0;
// The meters between day.park's benches.
inline constexpr double DAY_BENCH_SPACING = 10.0;

// The day-at-the-park slice's four day parks and the somewhere-to-sit slice's two, as saves.
struct DayParks {
  std::string Day;
  std::string NoToilet;
  std::string Noon;
  std::string Evening;
  std::string FarTables;
  std::string Unfurnished;
};

// Lays out day.park on the new-park template, furnishes it, and saves it. Saves it with its toilet
// block deleted as NoToilet, with its tables moved to the arm toward the drink shop as FarTables,
// and with no furniture as Unfurnished. Saves it stepped NOON_TICKS and EVENING_TICKS cycles as
// Noon and Evening. Throws std::logic_error when the world refuses a command, a shop is starved,
// or a shop or toilet block has no guest connector.
DayParks makeDayParks();

} // namespace tpj

#endif
