#ifndef TPJ_SIM_CLOCK_H
#define TPJ_SIM_CLOCK_H

#include <stdint.h>

namespace tpj {

// The game seconds each tick adds to the clock, so at 1x, SIM_TICK_SECONDS of real time, one game
// minute passes each real second.
inline constexpr uint64_t GAME_SECONDS_PER_TICK = 2;
// The game seconds in a day.
inline constexpr uint64_t DAY_SECONDS = 86400;
// The ticks in a day: 43200.
inline constexpr uint64_t DAY_TICKS = DAY_SECONDS / GAME_SECONDS_PER_TICK;
// The second of day 1 that tick 0 falls on: 9:00.
inline constexpr uint64_t FIRST_SECOND = 9ULL * 3600;

// A moment of the park's clock: the day, counted from 1, and the second of that day, from 0 to
// DAY_SECONDS - 1.
struct ParkTime {
  // The day, counted from 1.
  uint64_t Day = 1;
  // The second of the day, from 0 at midnight.
  uint64_t Second = 0;

  bool operator==(const ParkTime &) const = default;
};

// The park's time at a tick. Tick 0 is FIRST_SECOND of day 1, each tick adds
// GAME_SECONDS_PER_TICK, and a new day starts at midnight. Whole numbers only, so every build
// agrees.
ParkTime parkTime(uint64_t tick);

} // namespace tpj

#endif
