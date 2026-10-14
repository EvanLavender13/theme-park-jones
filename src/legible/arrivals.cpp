#include "legible/arrivals.h"

#include "legible/preview.h"
#include "sim/guests/guests.h"
#include "sim/park/edits.h"

namespace tpj {

bool keepArrivalDay(KeptArrivalDay &kept, const World &world, OpeningHours hours) {
  if (kept.Hours == hours) {
    return false;
  }
  const World *read = &world;
  if (parkHours(world) != hours) {
    // The world may have changed since the kept preview was made, so it is made again.
    kept.Preview.Tick.reset();
    keepPreview(kept.Preview, world, ParkEdit{SetOpeningHours{hours}}, false);
    read = &previewedWorld(world, kept.Preview.Made);
  }
  for (size_t i = 0; i < ARRIVAL_SAMPLES; ++i) {
    kept.Day[i] = arrivalCurve(*read, static_cast<uint64_t>(i) * 60);
  }
  kept.Hours = hours;
  return true;
}

} // namespace tpj
