#ifndef TPJ_SIM_OPERATIONS_SEATS_H
#define TPJ_SIM_OPERATIONS_SEATS_H

#include "sim/medium/field.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"

#include <stdint.h>
#include <string_view>

namespace tpj {

class WorldSchema;

// A guest's visit to a piece of furniture, addressed to the guest, which the piece returns to it.
struct SeatVisits {
  static constexpr std::string_view Name = "seat-visits";
};

// What a seat at a piece offers: the tiredness a rest there relieves and the hunger a meal eaten
// there relieves, each on a scale from 0 to 1, and the piece's number of seats.
struct SeatOffer {
  double RestRelief = 0.0;
  double MealRelief = 0.0;
  uint32_t Seats = 0;

  bool operator==(const SeatOffer &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, SeatOffer &offer) {
  visitor.field("rest-relief", offer.RestRelief);
  visitor.field("meal-relief", offer.MealRelief);
  visitor.field("seats", offer.Seats);
}

// The seat offers field: each piece's offer, at its seat place.
struct SeatOffers {
  using Entry = SeatOffer;
  static constexpr std::string_view Name = "seat-offers";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// Ticks a guest keeps its seat at a piece.
inline constexpr uint32_t SEAT_TIME = 300;
// The relief a seat at each kind gives.
inline constexpr double BENCH_REST_RELIEF = REST_RELIEF;
inline constexpr double BENCH_MEAL_RELIEF = 0.4;
inline constexpr double TABLE_REST_RELIEF = 0.2;
inline constexpr double TABLE_MEAL_RELIEF = MEAL_RELIEF;

// The offer of a seat at a piece of the kind with the seats.
constexpr SeatOffer seatOffer(FurnitureKind kind, uint32_t seats) {
  switch (kind) {
  case FurnitureKind::Bench:
    return {BENCH_REST_RELIEF, BENCH_MEAL_RELIEF, seats};
  case FurnitureKind::Table:
    return {TABLE_REST_RELIEF, TABLE_MEAL_RELIEF, seats};
  }
  return {BENCH_REST_RELIEF, BENCH_MEAL_RELIEF, seats};
}

// Registers the flow kind seat-visits, the seat-service state, the seat-offers field and its
// resolver, then the system that steps the pieces holding seat visits. addOperations calls it
// last.
void addSeats(WorldSchema &schema);

} // namespace tpj

#endif
