#include "support/park_worlds.h"
#include "support/same_networks.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
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
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::ParkIntent;
using test::sameNetworks;
using test::worldOf;

// Ground-line points and distances are sums and interpolations of rounded values, so a position or
// distance stated from the geometry may differ from the carrier's by rounding, far below the
// tolerance.
constexpr double SLACK = 1e-9;

ParkPath pathOf(uint64_t key, PathKind kind, std::vector<ParkPoint> points) {
  return ParkPath{.Key = EntityKey{key}, .Kind = kind, .Points = std::move(points)};
}

ParkBox boxOf(uint64_t key, BoxKind kind, Pose pose) {
  return ParkBox{.Key = EntityKey{key}, .Kind = kind, .At = pose};
}

World resolvedWorld(ParkIntent intent) {
  World world = worldOf(std::move(intent));
  resolveWorld(world);
  return world;
}

const Carrier *findCarrier(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  return found == network.carriers().end() ? nullptr : &*found;
}

// The nodes of the carrier's stops within the tolerance of the distance.
std::set<uint32_t> nodesNear(const Carrier &carrier, double distance) {
  std::set<uint32_t> nodes;
  for (const CarrierStop &stop : carrier.Stops) {
    if (std::abs(stop.Distance - distance) <= JUNCTION_TOLERANCE + SLACK) {
      nodes.insert(stop.Node);
    }
  }
  return nodes;
}

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

// A door's connector as the geometry states it: the path it reaches, the distance on that path,
// and the ground point there.
struct Connection {
  EntityKey Entity = NULL_KEY;
  Face DoorFace = Face::Front;
  PathKind Kind = PathKind::Guest;
  EntityKey Path = NULL_KEY;
  double PathDistance = 0.0;
  GroundPoint Point;
};

// Where two paths meet, as a distance on one of them.
struct PathMeeting {
  EntityKey Path = NULL_KEY;
  double Distance = 0.0;
};

// A park whose connectors and meetings are all known. Every path is straight between two points,
// so its ground line lies on its chord with a line point at every whole meter from its first
// point, and a point on it lies at its ground distance from the first point.
struct ConnectedPark {
  const char *Name = "";
  ParkIntent Intent;
  std::vector<Connection> Connections;
  std::vector<PathMeeting> Meetings;
};

// Every entity faces -x, so its front door lies half its depth toward -x and its back door half
// its depth toward +x. Each door has a guest line 2 m from it and a backstage line 3 m from it, on
// its own side, so only which doors serve which kind decides the connectors: the entrance's front
// to guest, the shop's front to guest and back to backstage, and the depot's front to backstage.
ConnectedPark servingPark() {
  const Pose facingWest{.X = 0.0, .Z = 0.0, .FacingX = -1.0, .FacingZ = 0.0};
  ConnectedPark park{"every door near lines of both kinds", {}, {}, {}};
  uint64_t key = 1;
  const auto linesBeside = [&](double doorX, double outward, double z) {
    for (const auto &[kind, gap] :
         {std::pair{PathKind::Guest, 2.0}, std::pair{PathKind::Backstage, 3.0}}) {
      const double x = doorX + (outward * gap);
      park.Intent.Paths.push_back(pathOf(key++, kind, {{x, z - 10.0}, {x, z + 10.0}}));
    }
  };

  // The entrance at z = 0, 3 m deep: doors at x = -1.5 and 1.5.
  const EntityKey entrance{key++};
  park.Intent.Entrances.push_back(ParkEntrance{.Key = entrance, .At = facingWest});
  const EntityKey entranceFrontGuest{key};
  linesBeside(-1.5, -1.0, 0.0);
  linesBeside(1.5, 1.0, 0.0);

  // The shop at z = 40, 6 m deep: doors at x = -3 and 3.
  const EntityKey shop{key++};
  Pose shopPose = facingWest;
  shopPose.Z = 40.0;
  park.Intent.Boxes.push_back(boxOf(static_cast<uint64_t>(shop), BoxKind::Shop, shopPose));
  const EntityKey shopFrontGuest{key};
  linesBeside(-3.0, -1.0, 40.0);
  const EntityKey shopBackBackstage{key + 1};
  linesBeside(3.0, 1.0, 40.0);

  // The depot at z = -40, 8 m deep: doors at x = -4 and 4.
  const EntityKey depot{key++};
  Pose depotPose = facingWest;
  depotPose.Z = -40.0;
  park.Intent.Boxes.push_back(boxOf(static_cast<uint64_t>(depot), BoxKind::Depot, depotPose));
  const EntityKey depotFrontBackstage{key + 1};
  linesBeside(-4.0, -1.0, -40.0);
  linesBeside(4.0, 1.0, -40.0);

  park.Connections = {
      {entrance, Face::Front, PathKind::Guest, entranceFrontGuest, 10.0, {-3.5, 0.0}},
      {shop, Face::Front, PathKind::Guest, shopFrontGuest, 10.0, {-5.0, 40.0}},
      {shop, Face::Back, PathKind::Backstage, shopBackBackstage, 10.0, {6.0, 40.0}},
      {depot, Face::Front, PathKind::Backstage, depotFrontBackstage, 10.0, {-7.0, -40.0}},
  };
  return park;
}

// Guest path 1 runs along z = 0 from x = -20 to x = 10, so (x, 0) lies at distance x + 20 on it.
// Path 2 starts at its end and runs to -z, a corner, and path 3 starts on it at x = -10 and runs
// to +z. Each shop's front door connects to path 1: shop 4's and shop 5's from either side to its
// middle, one point, shop 6's past its end to the corner, and shop 7's to 0.5 mm from path 3's
// start.
ConnectedPark connectionPlaces() {
  const auto shop = [](uint64_t key, double x, double z, double facingX, double facingZ) {
    return boxOf(key, BoxKind::Shop, Pose{.X = x, .Z = z, .FacingX = facingX, .FacingZ = facingZ});
  };
  ConnectedPark park{
      "connections mid-line, at one point, at a corner, and near a junction",
      ParkIntent{.Entrances = {},
                 .Paths = {pathOf(1, PathKind::Guest, {{-20.0, 0.0}, {10.0, 0.0}}),
                           pathOf(2, PathKind::Guest, {{10.0, 0.0}, {10.0, -20.0}}),
                           pathOf(3, PathKind::Guest, {{-10.0, 0.0}, {-10.0, 20.0}})},
                 // Front doors at (0, 2), (0, -2), (12, 0), and (-9.9995, -3).
                 .Boxes = {shop(4, 0.0, 5.0, 0.0, -1.0), shop(5, 0.0, -5.0, 0.0, 1.0),
                           shop(6, 15.0, 0.0, -1.0, 0.0), shop(7, -9.9995, -6.0, 0.0, 1.0)}},
      {},
      {{EntityKey{1}, 10.0}, {EntityKey{3}, 0.0}, {EntityKey{1}, 30.0}, {EntityKey{2}, 0.0}}};
  const EntityKey path{1};
  park.Connections = {
      {EntityKey{4}, Face::Front, PathKind::Guest, path, 20.0, {0.0, 0.0}},
      {EntityKey{5}, Face::Front, PathKind::Guest, path, 20.0, {0.0, 0.0}},
      {EntityKey{6}, Face::Front, PathKind::Guest, path, 30.0, {10.0, 0.0}},
      {EntityKey{7}, Face::Front, PathKind::Guest, path, 10.0005, {-9.9995, 0.0}},
  };
  return park;
}

// A shop facing (3, 4), whose front door lies at about (1.8, 2.4), and a guest line across its
// facing through (3, 4), 2 m ahead of the door.
ConnectedPark turnedShop() {
  return {"a shop turned off the axes",
          ParkIntent{.Entrances = {},
                     .Paths = {pathOf(2, PathKind::Guest, {{11.0, -2.0}, {-5.0, 10.0}})},
                     .Boxes = {boxOf(1, BoxKind::Shop,
                                     Pose{.X = 0.0, .Z = 0.0, .FacingX = 3.0, .FacingZ = 4.0})}},
          {{EntityKey{1}, Face::Front, PathKind::Guest, EntityKey{2}, 10.0, {3.0, 4.0}}},
          {}};
}

std::vector<ConnectedPark> connectedParks() {
  return {servingPark(), connectionPlaces(), turnedShop()};
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
      CHECK(door.X == expectedDoor.X);
      CHECK(door.Z == expectedDoor.Z);
      CHECK(door.Distance == 0.0);
      CHECK(std::abs(point.X - connection.Point.X) <= SLACK);
      CHECK(std::abs(point.Z - connection.Point.Z) <= SLACK);
      const double dx = point.X - door.X;
      const double dz = point.Z - door.Z;
      CHECK(point.Distance == std::sqrt((dx * dx) + (dz * dz)));
      CHECK(point.Distance > JUNCTION_TOLERANCE);
      CHECK(point.Distance <= CONNECTION_REACH);
    }
  }
}

// The shop's front door is (0, -3), and a guest line starting exactly 4 m from it, at (0, -7), runs
// away from it, so the reach is exactly CONNECTION_REACH.
TEST_CASE("A door exactly CONNECTION_REACH from the nearest point of its kind's lines has a "
          "connector") {
  const World world =
      resolvedWorld(ParkIntent{.Entrances = {},
                               .Paths = {pathOf(1, PathKind::Guest, {{0.0, -7.0}, {0.0, -20.0}})},
                               .Boxes = {boxOf(2, BoxKind::Shop, Pose{})}});
  const Carrier *connector =
      findCarrier(parkNetwork(world, PathKind::Guest), connectorKey(EntityKey{2}, Face::Front));
  REQUIRE(connector != nullptr);
  CHECK(connector->Points.back().Distance == CONNECTION_REACH);
}

// Each case is a world holding an entity whose doors all go without a connector, and the reason.
struct Unconnected {
  const char *Name = "";
  ParkIntent Intent;
  EntityKey Entity = NULL_KEY;
};

// A default Pose is a shop at the origin facing -z, whose front door is (0, -3) and back door
// (0, 3).
std::vector<Unconnected> unconnectedDoors() {
  const Pose origin{};
  const auto shopAt = [](const Pose &pose) { return boxOf(2, BoxKind::Shop, pose); };
  return {
      {"a door just beyond CONNECTION_REACH",
       {{}, {pathOf(1, PathKind::Guest, {{0.0, -7.01}, {0.0, -20.0}})}, {shopAt(origin)}},
       EntityKey{2}},
      {"a door within the tolerance of a line",
       {{}, {pathOf(1, PathKind::Guest, {{0.0, -3.0005}, {0.0, -20.0}})}, {shopAt(origin)}},
       EntityKey{2}},
      // The backstage line lies 2 m from the front door, which serves only guests, and 8 m from
      // the back door.
      {"no line of the kind a door serves",
       {{}, {pathOf(1, PathKind::Backstage, {{-10.0, -5.0}, {10.0, -5.0}})}, {shopAt(origin)}},
       EntityKey{2}},
      {"an entity with no footprint",
       {{},
        {pathOf(1, PathKind::Guest, {{-10.0, -5.0}, {10.0, -5.0}})},
        {shopAt(Pose{.X = 0.0, .Z = 0.0, .FacingX = 0.0, .FacingZ = 0.0})}},
       EntityKey{2}},
      // Only a hand-written save holds a path under a derived key.
      {"a path keyed like the door's connector",
       {{},
        {pathOf(1, PathKind::Guest, {{-10.0, -5.0}, {10.0, -5.0}}),
         pathOf(static_cast<uint64_t>(connectorKey(EntityKey{2}, Face::Front)), PathKind::Guest,
                {{50.0, 50.0}, {60.0, 50.0}})},
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

TEST_CASE("Every stop of a path carrier other than its first and last lies within the tolerance "
          "of a meeting's or a connection's distance on it") {
  for (const ConnectedPark &park : connectedParks()) {
    INFO(park.Name);
    const World world = resolvedWorld(park.Intent);
    for (const ParkPath &path : park.Intent.Paths) {
      INFO("path " << static_cast<uint64_t>(path.Key));
      std::vector<double> distances;
      for (const PathMeeting &meeting : park.Meetings) {
        if (meeting.Path == path.Key) {
          distances.push_back(meeting.Distance);
        }
      }
      for (const Connection &connection : park.Connections) {
        if (connection.Path == path.Key) {
          distances.push_back(connection.PathDistance);
        }
      }
      const Carrier *carrier = findCarrier(parkNetwork(world, path.Kind), path.Key);
      REQUIRE(carrier != nullptr);
      for (std::size_t index = 1; index + 1 < carrier->Stops.size(); ++index) {
        const double stop = carrier->Stops[index].Distance;
        INFO("stop at " << stop);
        CHECK(std::ranges::any_of(distances, [stop](double distance) {
          return std::abs(stop - distance) <= JUNCTION_TOLERANCE + SLACK;
        }));
      }
    }
  }
}

TEST_CASE("The new park's entrance has a connector to its guest path") {
  World world = makeNewPark(1);
  resolveWorld(world);
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  const std::vector<ParkPath> paths = parkPaths(world);
  REQUIRE(entrances.size() == 1);
  REQUIRE(paths.size() == 1);
  const Network &guest = parkNetwork(world, PathKind::Guest);
  const Carrier *connector = findCarrier(guest, connectorKey(entrances.front().Key, Face::Front));
  const Carrier *path = findCarrier(guest, paths.front().Key);
  REQUIRE(connector != nullptr);
  REQUIRE(path != nullptr);
  REQUIRE_FALSE(connector->Stops.empty());
  const uint32_t connection = connector->Stops.back().Node;
  CHECK(std::ranges::any_of(
      path->Stops, [connection](const CarrierStop &stop) { return stop.Node == connection; }));
}

} // namespace
} // namespace tpj
