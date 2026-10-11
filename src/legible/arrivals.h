#ifndef TPJ_LEGIBLE_ARRIVALS_H
#define TPJ_LEGIBLE_ARRIVALS_H

#include "sim/clock.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <array>
#include <optional>
#include <stddef.h>

namespace tpj {

// The game minutes of a day, at whose starts an arrival day samples the arrival curve.
inline constexpr size_t ARRIVAL_SAMPLES = DAY_SECONDS / 60;

// The guests per game hour each entrance admits at the start of each game minute of the day, from
// 00:00.
using ArrivalDay = std::array<double, ARRIVAL_SAMPLES>;

// An arrival day kept between frames, and the hours it was made for, or none before the first.
struct KeptArrivalDay {
  std::optional<OpeningHours> Hours;
  ArrivalDay Day{};
};

// Makes the kept day again for the hours when nothing is kept or the kept day's hours differ, and
// returns whether it did: arrivalCurve's samples of a candidate with only a SetOpeningHours of the
// hours queued, or of the world itself when it has those hours. Changes nothing.
bool keepArrivalDay(KeptArrivalDay &kept, const World &world, OpeningHours hours);

} // namespace tpj

#endif
