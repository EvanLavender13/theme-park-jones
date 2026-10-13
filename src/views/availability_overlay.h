#ifndef TPJ_VIEWS_AVAILABILITY_OVERLAY_H
#define TPJ_VIEWS_AVAILABILITY_OVERLAY_H

#include "legible/availability.h"
#include "render/paving_overlay.h"
#include "sim/world.h"

#include <vector>

namespace tpj {

// The lists shading an availability reuses: the availability's lists, the bends, the values at
// them, the shading's lists, and the shades it gives.
struct AvailabilityShadeLists {
  AvailabilityLists Availability;
  std::vector<Place> Bends;
  std::vector<double> Values;
  ShadeLists Shading;
  OverlayShades Shades;
};

// An availability's shades of an overlay shape, as the app shows them, left in lists.Shades: the
// availability at each of availabilityBends' places, halfway up the ramp at the definition's
// Half. Uses lists the caller keeps, and allocates nothing once they have grown. Changes nothing
// in the world.
void availabilityShades(const World &world, const OverlayShape &shape,
                        const AvailabilityDefinition &definition, AvailabilityShadeLists &lists);

} // namespace tpj

#endif
