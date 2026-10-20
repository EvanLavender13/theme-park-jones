#ifndef TPJ_SIM_ROUTES_NETWORKS_H
#define TPJ_SIM_ROUTES_NETWORKS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/tick_buffers.h"

#include <array>
#include <stdint.h>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// Connectors that meet a carrier this close to each other, or to its end, stop there at one
// node. Segments whose paving lies this close to equally near a door tie for it.
inline constexpr double JUNCTION_TOLERANCE = 0.001;

// A door farther than this from the paving of every segment of its kind has no connector.
inline constexpr double CONNECTION_REACH = 3.0;

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

// A door: its entity, the face it is on, the middle of that face, the unit direction out of the
// face, and the face's width.
struct ParkDoor {
  EntityKey Entity = NULL_KEY;
  Face Side = Face::Front;
  ParkPoint At;
  ParkPoint Out;
  double Width = 0.0;

  bool operator==(const ParkDoor &) const = default;
};

// The doors serving the kind: the entrances' first, then the boxes', each in key order of its
// entity. An entity with no footprint has no door.
std::vector<ParkDoor> parkDoors(const World &world, PathKind kind);
// As the returning form, into a list the caller keeps, which it clears first, keeping the
// entrances and boxes it reads in the buffers. Allocates nothing once the lists have grown.
void parkDoors(const World &world, PathKind kind, std::vector<ParkDoor> &doors,
               TickBuffers &buffers);

// A walkway for each connector of the kind's network, in key order of the connector.
std::vector<Walkway> connectorWalkways(const World &world, PathKind kind);

// A connector's walkway: the connector's key, and the walkway made for it.
struct KeyedWalkway {
  EntityKey Connector = NULL_KEY;
  Walkway Way;

  bool operator==(const KeyedWalkway &) const = default;
};

// Each walkway connectorWalkways gives, in the same order, with its connector's key.
std::vector<KeyedWalkway> keyedWalkways(const World &world, PathKind kind);
// As the returning forms of keyedWalkways and connectorWalkways, appended to a list the caller
// keeps, keeping the entrances and boxes they read in the buffers. Allocate nothing once the lists
// have grown.
void keyedWalkways(const World &world, PathKind kind, std::vector<KeyedWalkway> &walkways,
                   TickBuffers &buffers);
void connectorWalkways(const World &world, PathKind kind, std::vector<Walkway> &walkways,
                       TickBuffers &buffers);

// Puts in pieces and strips, clearing them first, the walkway pieces and the doors' frontage strips
// whose bounds meet the rectangle from least to most, each entity's once, in key order. It can also
// put in others. A frontage strip runs straight out from a door's face, as wide as the door, to
// where the door's walkway ends. For a resolver. Allocates nothing once the lists have grown.
void walkwaysNear(const World &world, ParkPoint least, ParkPoint most,
                  std::vector<const SurfacePiece *> &pieces,
                  std::vector<std::array<ParkPoint, 4>> &strips, TickBuffers &buffers);
// Adds to the cells, keeping the list ascending without repeats, the cells of pavingCells' grid
// where the running resolution added, removed, or changed a walkway piece or a frontage strip.
// Allocates nothing once the lists have grown.
void changedWalkwayCells(const World &world, std::vector<EntityKey> &cells, TickBuffers &buffers);

// The park's paved surface with every connector's walkway, guest walkways first, as the last
// resolution derived it with parkSurface, or an empty surface when the world holds none, as before
// its first resolution. Resolvers that read it depend on walked-surface.
const PavedSurface &walkedSurface(const World &world);

// The kind's network as the last resolution derived it, or an empty network when the world holds
// none, as before its first resolution.
const Network &parkNetwork(const World &world, PathKind kind);

// The kind's network as the resolution before the current one derived it, or none when the world
// holds none: always outside a resolution, and in a world's first. Meant for finishers that carry
// places held on it to parkNetwork with carryOver.
const Network *previousNetwork(const World &world, PathKind kind);

// Registers the derived type previous-network, the resolver path-networks, and then route
// distance's fields and resolver.
void addRoutes(WorldSchema &schema);

// Registers the finisher that drops the previous networks. Register it after every finisher that
// reads them.
void addDropPreviousNetworks(WorldSchema &schema);

} // namespace tpj

#endif
