#include "legible/food.h"

#include "sim/medium/field.h"
#include "sim/operations/operations.h"

#include <optional>

namespace tpj {

const AvailabilityDefinition FOOD_AVAILABILITY{&sourceEntryAtNode<FoodOffer>, FOOD_DISCOUNT_CURVE,
                                               MEAL_RELIEF};

FoodAvailability foodAvailability(const World &world, const Place &place) {
  return availabilityAt(world, FOOD_AVAILABILITY, place);
}

} // namespace tpj
