#include "legible/park_summary.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/park/intent.h"

#include <optional>

namespace tpj {

ParkSummary summarizePark(const World &world) {
  ParkSummary summary;
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<ShopRecord> record = shopRecord(world, box.Key)) {
      summary.Shops.push_back({box.Key, *record});
    }
  }
  double hunger = 0.0;
  for (const EntityKey guest : parkGuests(world)) {
    if (const std::optional<GuestRecord> record = guestRecord(world, guest)) {
      ++summary.Guests;
      hunger += record->Hunger;
      if (record->Activity == GuestActivity::Waiting) {
        ++summary.Waiting;
      }
    }
  }
  summary.MeanHunger = summary.Guests == 0 ? 0.0 : hunger / static_cast<double>(summary.Guests);
  summary.MealsEaten = unitsConsumed<Meals>(world, EATEN_CAUSE);
  return summary;
}

} // namespace tpj
