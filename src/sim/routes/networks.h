#ifndef TPJ_SIM_ROUTES_NETWORKS_H
#define TPJ_SIM_ROUTES_NETWORKS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/intent.h"

#include <stdint.h>

namespace tpj {

class World;
class WorldSchema;

// Two lines of a kind that come this close meet, and a carrier's meetings this close are one stop.
inline constexpr double JUNCTION_TOLERANCE = 0.001;

inline constexpr uint64_t NETWORK_PURPOSE = hashName("network");

// The key of the derived entity holding the kind's network.
constexpr EntityKey networkKey(PathKind kind) {
  return deriveKey(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
}

// The kind's network as the last resolution derived it, or an empty network when the world holds
// none. See sim/routes/SPEC.md.
const Network &parkNetwork(const World &world, PathKind kind);

// Registers the resolver path-networks.
void addRoutes(WorldSchema &schema);

} // namespace tpj

#endif
