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

// A door farther than this from every path of its kind has no connector.
inline constexpr double CONNECTION_REACH = 4.0;

inline constexpr uint64_t NETWORK_PURPOSE = hashName("network");

// The key of the derived entity holding the kind's network.
constexpr EntityKey networkKey(PathKind kind) {
  return deriveKey(NULL_KEY, NETWORK_PURPOSE, static_cast<uint64_t>(kind));
}

inline constexpr uint64_t CONNECTOR_PURPOSE = hashName("connector");

// A face of an entrance or box, whose midpoint is a door that may connect to a network.
enum class Face : uint8_t { Front, Back };

// The key of the connector from the door on the entity's face.
constexpr EntityKey connectorKey(EntityKey entity, Face face) {
  return deriveKey(entity, CONNECTOR_PURPOSE, static_cast<uint64_t>(face));
}

// The kind's network as the last resolution derived it, or an empty network when the world holds
// none, as before its first resolution.
const Network &parkNetwork(const World &world, PathKind kind);

// Registers the resolver path-networks.
void addRoutes(WorldSchema &schema);

} // namespace tpj

#endif
