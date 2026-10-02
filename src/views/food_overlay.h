#ifndef TPJ_VIEWS_FOOD_OVERLAY_H
#define TPJ_VIEWS_FOOD_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/world.h"

namespace tpj {

// The food overlay as the app shows it: render's band along each guest path, shaded by the food
// availability at each place. Changes nothing in the world.
ParkMesh buildFoodAvailabilityOverlay(const World &world);

} // namespace tpj

#endif
