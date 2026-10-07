#ifndef TPJ_VIEWS_AVAILABILITY_OVERLAY_H
#define TPJ_VIEWS_AVAILABILITY_OVERLAY_H

#include "legible/availability.h"
#include "render/paving_overlay.h"
#include "sim/world.h"

namespace tpj {

// An availability's shades of an overlay shape, as the app shows them: the availability at each of
// availabilityBends' places, halfway up the ramp at the definition's Half. Changes nothing in the
// world.
OverlayShades availabilityShades(const World &world, const OverlayShape &shape,
                                 const AvailabilityDefinition &definition);

} // namespace tpj

#endif
