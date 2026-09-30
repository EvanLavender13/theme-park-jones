#ifndef TPJ_LEGIBLE_FOOD_H
#define TPJ_LEGIBLE_FOOD_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <array>
#include <vector>

namespace tpj {

class World;

// The walking speed, in meters per second, that turns a route distance into time: the guests'
// own, so an effective time is what a guest would feel.
inline constexpr double REFERENCE_SPEED = WALK_SPEED;

// How much of a shop's relief counts after an effective time in seconds (decision 0020): all of it
// at once, half after a minute, and none after four.
inline constexpr std::array<CurvePoint, 4> FOOD_DISCOUNT_CURVE{
    {{0.0, 1.0}, {60.0, 0.5}, {120.0, 0.2}, {240.0, 0.0}}};

// One shop's part of the food availability at a place: its offer's relief, its route distance in
// meters, its offer's wait and the effective time in seconds, and its term, the relief discounted
// by that time.
struct FoodContribution {
  EntityKey Shop = NULL_KEY;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Time = 0.0;
  double Term = 0.0;

  bool operator==(const FoodContribution &) const = default;
};

// The food availability at a place and the contributions it is the sum of, in ascending shop key.
struct FoodAvailability {
  std::vector<FoodContribution> Contributions;
  double Value = 0.0;

  bool operator==(const FoodAvailability &) const = default;
};

// The discount curve at a time in seconds: 1 at or below its first point, 0 at or beyond its last,
// and 0 for a NaN.
double foodDiscount(double seconds);

// The food availability at a place on the world's guest network: each reachable shop whose offer
// says meals are supplied contributes its relief discounted by its effective time, and the value
// is their terms added in order. Changes nothing.
FoodAvailability foodAvailability(const World &world, const Place &place);

} // namespace tpj

#endif
