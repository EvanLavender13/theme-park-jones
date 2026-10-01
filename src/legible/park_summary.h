#ifndef TPJ_LEGIBLE_PARK_SUMMARY_H
#define TPJ_LEGIBLE_PARK_SUMMARY_H

#include "sim/entity_key.h"
#include "sim/operations/operations.h"
#include "sim/world.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// A shop box's key and its inspection record.
struct ShopLine {
  EntityKey Shop = NULL_KEY;
  ShopRecord Record;

  bool operator==(const ShopLine &) const = default;
};

// What the Debug panel shows about a park: its shops' records, its guests' count, mean hunger, and
// how many wait, and the meals eaten.
struct ParkSummary {
  std::vector<ShopLine> Shops;
  uint64_t Guests = 0;
  double MeanHunger = 0.0;
  uint64_t Waiting = 0;
  int64_t MealsEaten = 0;

  bool operator==(const ParkSummary &) const = default;
};

// A ShopLine for each box of parkBoxes that has a shopRecord, in parkBoxes' order; the number of
// parkGuests that have a guestRecord, the mean of their Hunger, 0 when there are none, and how
// many are Waiting; and the meals units consumed with the cause eaten. Changes nothing.
ParkSummary summarizePark(const World &world);

} // namespace tpj

#endif
