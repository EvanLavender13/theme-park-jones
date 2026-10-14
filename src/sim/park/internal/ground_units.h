#ifndef TPJ_SIM_PARK_INTERNAL_GROUND_UNITS_H
#define TPJ_SIM_PARK_INTERNAL_GROUND_UNITS_H

#include "sim/park/internal/layout.h"
#include "sim/world.h"

namespace tpj {

// Puts the world's layout in lists.Layout and its ground in lists.Ground. When the ground units
// hold every change made to the world, it copies the ground from them. Otherwise it derives the
// whole layout's ground, as groundOf does. Both give the same ground. Allocates nothing once the
// lists have grown.
void worldGround(const World &world, GroundLists &lists);

} // namespace tpj

#endif
