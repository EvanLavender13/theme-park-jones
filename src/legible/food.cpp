#include "legible/food.h"

#include "sim/medium/field.h"
#include "sim/operations/operations.h"

#include <optional>

namespace tpj {

const AvailabilityDefinition FOOD_AVAILABILITY{&sourceEntryAtNode<FoodOffer>, FOOD_DISCOUNT_CURVE,
                                               0.5};

FoodAvailability foodAvailability(const World &world, const Place &place) {
  return availabilityAt(world, FOOD_AVAILABILITY, place);
}

std::optional<FoodAvailability> foodNear(const World &world, GroundPoint point, double reach) {
  const std::optional<PathPlace> nearest = nearestGuestPathPlace(world, point);
  if (!nearest || nearest->Distance > reach) {
    return std::nullopt;
  }
  return foodAvailability(world, nearest->At);
}

} // namespace tpj
