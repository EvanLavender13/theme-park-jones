#include "legible/arrivals.h"

#include "legible/preview.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"

namespace tpj {

bool keepArrivalDay(KeptArrivalDay &kept, const World &world, OpeningHours hours) {
  if (kept.Hours == hours) {
    return false;
  }
  std::optional<Preview> preview;
  if (parkHours(world) != hours) {
    preview = previewEdit(world, ParkEdit{SetOpeningHours{hours}});
  }
  const World &read = preview ? previewedWorld(world, *preview) : world;
  for (size_t i = 0; i < ARRIVAL_SAMPLES; ++i) {
    kept.Day[i] = arrivalCurve(read, static_cast<uint64_t>(i) * 60);
  }
  kept.Hours = hours;
  return true;
}

} // namespace tpj
