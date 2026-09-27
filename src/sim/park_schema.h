#ifndef TPJ_SIM_PARK_SCHEMA_H
#define TPJ_SIM_PARK_SCHEMA_H

#include "sim/schema.h"

#include <memory>

namespace tpj {

// The park's schema: every capability's component types, systems, swap functions, resolvers, and
// commands, registered here in a written order. Park files are loaded with it.
std::shared_ptr<const WorldSchema> makeParkSchema();

} // namespace tpj

#endif
