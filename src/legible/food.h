#ifndef TPJ_LEGIBLE_FOOD_H
#define TPJ_LEGIBLE_FOOD_H

#include "legible/availability.h"
#include "legible/path_place.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <array>
#include <optional>

namespace tpj {

class World;

// How much of a shop's relief counts after an effective time in seconds (decision 0020): all of it
// at once, half after a minute, and none after four.
inline constexpr std::array<CurvePoint, 4> FOOD_DISCOUNT_CURVE{
    {{0.0, 1.0}, {60.0, 0.5}, {120.0, 0.2}, {240.0, 0.0}}};

// Food availability: shops' food offers, discounted by FOOD_DISCOUNT_CURVE, with an overlay of it
// fully saturated at a meal's relief with no walk and no wait.
extern const AvailabilityDefinition FOOD_AVAILABILITY;

// One shop's part of the food availability at a place, and the food availability at a place.
using FoodContribution = AvailabilityContribution;
using FoodAvailability = Availability;

// The food availability at a place on the world's guest network: each reachable shop whose offer
// says meals are supplied contributes its relief discounted by its effective time, and the value
// is their terms added in order. Changes nothing.
FoodAvailability foodAvailability(const World &world, const Place &place);

// The food availability at the guest path place nearest the point, when the point lies within the
// reach of it, and none otherwise.
std::optional<FoodAvailability> foodNear(const World &world, GroundPoint point, double reach);

} // namespace tpj

#endif
