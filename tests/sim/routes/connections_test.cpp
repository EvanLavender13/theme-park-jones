#include "support/connected_parks.h"
#include "support/park_worlds.h"
#include "support/same_networks.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::boxOf;
using test::ConnectedPark;
using test::connectedParks;
using test::Connection;
using test::findCarrier;
using test::nodesNear;
using test::ParkIntent;
using test::pathOf;
using test::resolvedWorld;
using test::sameNetworks;
using test::SLACK;

// The midpoint of a face of the entity's footprint: the front of its first two corners, and the
// back of its last two.
GroundPoint doorOf(const ParkIntent &intent, EntityKey entity, Face face) {
  std::optional<Footprint> footprint;
  for (const ParkEntrance &entrance : intent.Entrances) {
    if (entrance.Key == entity) {
      footprint = footprintOf(entrance.At, ENTRANCE_SIZE);
    }
  }
  for (const ParkBox &box : intent.Boxes) {
    if (box.Key == entity) {
      footprint = footprintOf(box.At, boxSize(box.Kind));
    }
  }
  REQUIRE(footprint.has_value());
  const std::array<ParkPoint, 4> &corners = footprint.value_or(Footprint{}).Corners;
  const ParkPoint &first = face == Face::Front ? corners[0] : corners[2];
  const ParkPoint &second = face == Face::Front ? corners[1] : corners[3];
  return {.X = (first.X + second.X) / 2, .Z = (first.Z + second.Z) / 2};
}

constexpr std::array<PathKind, 2> KINDS = {PathKind::Guest, PathKind::Backstage};

TEST_CASE("Each kind's network has its paths as carriers and, besides them, exactly one carrier "
          "for each door serving that kind with a connector, keyed connectorKey of its entity and "
          "face") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const PathKind kind : KINDS) {
      INFO("kind " << static_cast<int>(kind));
      std::set<EntityKey> expected;
      for (const ParkPath &path : park.Intent.Paths) {
        if (path.Kind == kind) {
          expected.insert(path.Key);
        }
      }
      for (const Connection &connection : park.Connections) {
        if (connection.Kind == kind) {
          expected.insert(connectorKey(connection.Entity, connection.DoorFace));
        }
      }
      std::set<EntityKey> keys;
      for (const Carrier &carrier : parkNetwork(world, kind).carriers()) {
        keys.insert(carrier.Key);
      }
      CHECK(keys == expected);
    }
  }
}

TEST_CASE("A connector has two points, its door at distance 0 and its connection point at the "
          "reach, the straight distance between them, more than the tolerance and at most "
          "CONNECTION_REACH") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const Connection &connection : park.Connections) {
      INFO("entity " << static_cast<uint64_t>(connection.Entity) << ", face "
                     << static_cast<int>(connection.DoorFace));
      const Carrier *connector = findCarrier(parkNetwork(world, connection.Kind),
                                             connectorKey(connection.Entity, connection.DoorFace));
      REQUIRE(connector != nullptr);
      REQUIRE(connector->Points.size() == 2);
      const CarrierPoint &door = connector->Points.front();
      const CarrierPoint &point = connector->Points.back();
      const GroundPoint expectedDoor = doorOf(park.Intent, connection.Entity, connection.DoorFace);
      CHECK(std::abs(door.X - expectedDoor.X) <= SLACK);
      CHECK(std::abs(door.Z - expectedDoor.Z) <= SLACK);
      CHECK(door.Distance == 0.0);
      CHECK(std::abs(point.X - connection.Point.X) <= SLACK);
      CHECK(std::abs(point.Z - connection.Point.Z) <= SLACK);
      const double dx = point.X - door.X;
      const double dz = point.Z - door.Z;
      CHECK(std::abs(point.Distance - std::sqrt((dx * dx) + (dz * dz))) <= SLACK);
      CHECK(point.Distance > JUNCTION_TOLERANCE);
      CHECK(point.Distance <= CONNECTION_REACH);
    }
  }
}

// Each case is a world holding an entity whose doors all go without a connector, and the reason.
struct Unconnected {
  const char *Name = "";
  ParkIntent Intent;
  EntityKey Entity = NULL_KEY;
};

// A default Pose is a shop at the origin facing -z, whose front door, which serves guests, lies
// half its depth toward -z. Its back door serves backstage, and no case has a backstage line. An
// entrance's back door serves no network.
std::vector<Unconnected> unconnectedDoors() {
  const Pose origin{};
  const double door = -boxSize(BoxKind::Shop).Depth / 2.0;
  const auto shopAt = [](const Pose &pose) { return boxOf(2, BoxKind::Shop, pose); };
  // A guest line across the front door's facing, starting the given distance in front of it.
  const auto guestAhead = [door](double gap) {
    return pathOf(1, PathKind::Guest, {{0.0, door - gap}, {0.0, door - gap - 20.0}});
  };
  const ParkPath withinReach = pathOf(1, PathKind::Guest,
                                      {{-10.0, door - test::gapWithinReach(PathKind::Guest)},
                                       {10.0, door - test::gapWithinReach(PathKind::Guest)}});
  const double entranceDoor = -ENTRANCE_SIZE.Depth / 2.0;
  const double backstageGap = test::gapWithinReach(PathKind::Backstage);
  return {
      {"a door just beyond CONNECTION_REACH",
       {{}, {guestAhead(CONNECTION_REACH + JUNCTION_TOLERANCE)}, {shopAt(origin)}},
       EntityKey{2}},
      {"a door within the tolerance of a line",
       {{}, {guestAhead(JUNCTION_TOLERANCE / 2.0)}, {shopAt(origin)}},
       EntityKey{2}},
      {"no line of the kind a door serves",
       {{{EntityKey{2}, origin}},
        {pathOf(1, PathKind::Backstage,
                {{-10.0, entranceDoor - backstageGap}, {10.0, entranceDoor - backstageGap}})},
        {}},
       EntityKey{2}},
      {"an entity with no footprint",
       {{}, {withinReach}, {shopAt(Pose{.X = 0.0, .Z = 0.0, .FacingX = 0.0, .FacingZ = 0.0})}},
       EntityKey{2}},
      // Only a hand-written save holds a path under a derived key.
      {"a path keyed like the door's connector",
       {{},
        {withinReach, pathOf(static_cast<uint64_t>(connectorKey(EntityKey{2}, Face::Front)),
                             PathKind::Guest, {{50.0, 50.0}, {60.0, 50.0}})},
        {shopAt(origin)}},
       EntityKey{2}},
  };
}

TEST_CASE("An entity none of whose doors has a connector leaves the networks as they are without "
          "it") {
  for (const Unconnected &test : unconnectedDoors()) {
    INFO(test.Name);
    ParkIntent without = test.Intent;
    std::erase_if(without.Boxes, [&](const ParkBox &box) { return box.Key == test.Entity; });
    std::erase_if(without.Entrances,
                  [&](const ParkEntrance &entrance) { return entrance.Key == test.Entity; });
    const World with = resolvedWorld(test.Intent);
    CHECK(sameNetworks(with, resolvedWorld(without)));
  }
}

TEST_CASE("A connector's first stop is at a node no other stop of its network has") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const Connection &connection : park.Connections) {
      INFO("entity " << static_cast<uint64_t>(connection.Entity));
      const Network &network = parkNetwork(world, connection.Kind);
      const Carrier *connector =
          findCarrier(network, connectorKey(connection.Entity, connection.DoorFace));
      REQUIRE(connector != nullptr);
      REQUIRE_FALSE(connector->Stops.empty());
      const uint32_t door = connector->Stops.front().Node;
      std::size_t stops = 0;
      for (const Carrier &carrier : network.carriers()) {
        stops += static_cast<std::size_t>(std::ranges::count_if(
            carrier.Stops, [door](const CarrierStop &stop) { return stop.Node == door; }));
      }
      CHECK(stops == 1);
    }
  }
}

TEST_CASE("A connector stops only at its door and its connection, whose node is its path's stop "
          "within the tolerance of the connection place") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const Connection &connection : park.Connections) {
      INFO("entity " << static_cast<uint64_t>(connection.Entity));
      const Network &network = parkNetwork(world, connection.Kind);
      const Carrier *connector =
          findCarrier(network, connectorKey(connection.Entity, connection.DoorFace));
      const Carrier *path = findCarrier(network, connection.Path);
      REQUIRE(connector != nullptr);
      REQUIRE(path != nullptr);
      REQUIRE(connector->Stops.size() == 2);
      const std::set<uint32_t> nodes = nodesNear(*path, connection.PathDistance);
      CHECK(nodes.size() == 1);
      CHECK(nodes.contains(connector->Stops.back().Node));
    }
  }
}

TEST_CASE("A network's anchors are exactly its connectors' door nodes, each anchored to its door's "
          "entity") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    std::vector<EntityKey> entities;
    entities.reserve(park.Intent.Entrances.size() + park.Intent.Boxes.size());
    for (const ParkEntrance &entrance : park.Intent.Entrances) {
      entities.push_back(entrance.Key);
    }
    for (const ParkBox &box : park.Intent.Boxes) {
      entities.push_back(box.Key);
    }
    for (const PathKind kind : KINDS) {
      INFO("kind " << static_cast<int>(kind));
      const Network &network = parkNetwork(world, kind);
      std::map<uint32_t, EntityKey> doors;
      for (const Connection &connection : park.Connections) {
        if (connection.Kind == kind) {
          const Carrier *connector =
              findCarrier(network, connectorKey(connection.Entity, connection.DoorFace));
          REQUIRE(connector != nullptr);
          REQUIRE_FALSE(connector->Stops.empty());
          doors[connector->Stops.front().Node] = connection.Entity;
        }
      }
      for (uint32_t node = 0; node < network.nodeCount(); ++node) {
        INFO("node " << node);
        const auto door = doors.find(node);
        CHECK(network.nodeAnchor(node) == (door == doors.end() ? NULL_KEY : door->second));
      }
      for (const EntityKey entity : entities) {
        INFO("entity " << static_cast<uint64_t>(entity));
        std::vector<uint32_t> expected;
        for (const auto &[node, anchored] : doors) {
          if (anchored == entity) {
            expected.push_back(node);
          }
        }
        CHECK(network.anchoredNodes(entity) == expected);
      }
    }
  }
}

} // namespace
} // namespace tpj
