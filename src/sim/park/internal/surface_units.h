#ifndef TPJ_SIM_PARK_INTERNAL_SURFACE_UNITS_H
#define TPJ_SIM_PARK_INTERNAL_SURFACE_UNITS_H

#include "sim/entity_key.h"
#include "sim/schema.h"

namespace tpj {

// Registers the units the paved surface derives in: each group's label, each point's asks, fit,
// and paving, each segment's paving, the cells their pieces cover, and each point's hidden edge
// lines, then the paved-surface resolver, which keeps the surface from them.
void addSurfaceUnits(WorldSchema &schema);

// The derived key of the entity that holds the paved surface.
EntityKey pavedSurfaceKey();

} // namespace tpj

#endif
