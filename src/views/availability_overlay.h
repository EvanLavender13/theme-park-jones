#ifndef TPJ_VIEWS_AVAILABILITY_OVERLAY_H
#define TPJ_VIEWS_AVAILABILITY_OVERLAY_H

#include "legible/availability.h"
#include "render/park_mesh.h"
#include "sim/world.h"

namespace tpj {

// An availability's overlay as the app shows it: render's band along each guest path, shaded by
// the availability at each place and saturating at the definition's Full. Changes nothing in the
// world.
ParkMesh buildAvailabilityOverlay(const World &world, const AvailabilityDefinition &definition);

} // namespace tpj

#endif
