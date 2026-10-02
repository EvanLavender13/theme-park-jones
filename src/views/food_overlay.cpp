#include "views/food_overlay.h"

#include "legible/food.h"
#include "render/food_overlay.h"

namespace tpj {

ParkMesh buildFoodAvailabilityOverlay(const World &world) {
  return buildFoodOverlay(
      world, [&world](const Place &place) { return foodAvailability(world, place).Value; });
}

} // namespace tpj
