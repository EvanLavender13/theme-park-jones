#ifndef TPJ_SIM_PARK_SCHEMA_H
#define TPJ_SIM_PARK_SCHEMA_H

#include "sim/schema.h"

#include <memory>

namespace tpj {

// The park's schema: exactly what addPark registers. Park files are loaded with it.
std::shared_ptr<const WorldSchema> makeParkSchema();

// Registers every capability's component types, systems, swap functions, resolvers, and commands
// for the park, in a written order. A scenario that adds synthetic entities to the park registers
// them after these.
void addPark(WorldSchema &schema);

} // namespace tpj

#endif
