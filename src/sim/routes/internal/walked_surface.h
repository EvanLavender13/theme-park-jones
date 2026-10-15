#ifndef TPJ_SIM_ROUTES_INTERNAL_WALKED_SURFACE_H
#define TPJ_SIM_ROUTES_INTERNAL_WALKED_SURFACE_H

#include "sim/schema.h"

namespace tpj {

// Registers the units the walked surface derives in, each point's walked edge lines and each
// segment's walkways, then the walked-surface resolver, which keeps the walked surface from them
// and the paved surface.
void addWalkedSurface(WorldSchema &schema);

} // namespace tpj

#endif
