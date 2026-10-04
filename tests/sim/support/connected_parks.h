#ifndef TPJ_TESTS_SIM_SUPPORT_CONNECTED_PARKS_H
#define TPJ_TESTS_SIM_SUPPORT_CONNECTED_PARKS_H

#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

// Parks whose connectors and meetings are all known, and the queries their tests share.
namespace tpj::test {

// Ground-line points and distances are sums and interpolations of rounded values, so a position or
// distance stated from the geometry may differ from the carrier's by rounding, far below the
// tolerance.
inline constexpr double SLACK = 1e-9;

inline ParkPath pathOf(uint64_t key, PathKind kind, std::vector<ParkPoint> points) {
  return ParkPath{.Key = EntityKey{key}, .Kind = kind, .Points = std::move(points)};
}

inline ParkBox boxOf(uint64_t key, BoxKind kind, Pose pose) {
  return ParkBox{.Key = EntityKey{key}, .Kind = kind, .At = pose};
}

inline World resolvedWorld(ParkIntent intent) {
  World world = worldOf(std::move(intent));
  resolveWorld(world);
  return world;
}

inline const Carrier *findCarrier(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  return found == network.carriers().end() ? nullptr : &*found;
}

// The nodes of the carrier's stops within the tolerance of the distance.
inline std::set<uint32_t> nodesNear(const Carrier &carrier, double distance) {
  std::set<uint32_t> nodes;
  for (const CarrierStop &stop : carrier.Stops) {
    if (std::abs(stop.Distance - distance) <= JUNCTION_TOLERANCE + SLACK) {
      nodes.insert(stop.Node);
    }
  }
  return nodes;
}

// A distance from a door to the middle of a line of the kind that puts the door off the line,
// beyond its half width, and within CONNECTION_REACH of it, so the door connects to the line.
inline double gapWithinReach(PathKind kind) {
  return ((pathWidth(kind) / 2.0) + CONNECTION_REACH) / 2.0;
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
// so its ground line lies on its chord, and a point on it lies at its ground distance from the
// first point.
struct ConnectedPark {
  const char *Name = "";
  ParkIntent Intent;
  std::vector<Connection> Connections;
  std::vector<PathMeeting> Meetings;
};

// Every entity faces -x, so its front door lies half its depth toward -x and its back door half
// its depth toward +x. Each door has a guest line flush against it, the line's edge touching its
// face, and a backstage line within reach of it, on its own side, so only which doors serve which
// kind decides the connectors: the entrance's front to guest, the shop's front to guest and back
// to backstage, and the depot's front to backstage.
inline ConnectedPark servingPark() {
  const Pose facingWest{.X = 0.0, .Z = 0.0, .FacingX = -1.0, .FacingZ = 0.0};
  const double guestGap = pathWidth(PathKind::Guest) / 2.0;
  const double backstageGap = gapWithinReach(PathKind::Backstage);
  ConnectedPark park{"every door near lines of both kinds", {}, {}, {}};
  uint64_t key = 1;
  const auto linesBeside = [&](double doorX, double outward, double z) {
    for (const auto &[kind, gap] :
         {std::pair{PathKind::Guest, guestGap}, std::pair{PathKind::Backstage, backstageGap}}) {
      const double x = doorX + (outward * gap);
      park.Intent.Paths.push_back(pathOf(key++, kind, {{x, z - 10.0}, {x, z + 10.0}}));
    }
  };

  const double entranceHalf = ENTRANCE_SIZE.Depth / 2.0;
  const EntityKey entrance{key++};
  park.Intent.Entrances.push_back(ParkEntrance{.Key = entrance, .At = facingWest});
  const EntityKey entranceFrontGuest{key};
  linesBeside(-entranceHalf, -1.0, 0.0);
  linesBeside(entranceHalf, 1.0, 0.0);

  const double shopHalf = boxSize(BoxKind::Shop).Depth / 2.0;
  const EntityKey shop{key++};
  Pose shopPose = facingWest;
  shopPose.Z = 40.0;
  park.Intent.Boxes.push_back(boxOf(static_cast<uint64_t>(shop), BoxKind::Shop, shopPose));
  const EntityKey shopFrontGuest{key};
  linesBeside(-shopHalf, -1.0, 40.0);
  const EntityKey shopBackBackstage{key + 1};
  linesBeside(shopHalf, 1.0, 40.0);

  const double depotHalf = boxSize(BoxKind::Depot).Depth / 2.0;
  const EntityKey depot{key++};
  Pose depotPose = facingWest;
  depotPose.Z = -40.0;
  park.Intent.Boxes.push_back(boxOf(static_cast<uint64_t>(depot), BoxKind::Depot, depotPose));
  const EntityKey depotFrontBackstage{key + 1};
  linesBeside(-depotHalf, -1.0, -40.0);
  linesBeside(depotHalf, 1.0, -40.0);

  park.Connections = {
      {entrance,
       Face::Front,
       PathKind::Guest,
       entranceFrontGuest,
       10.0,
       {-entranceHalf - guestGap, 0.0}},
      {shop, Face::Front, PathKind::Guest, shopFrontGuest, 10.0, {-shopHalf - guestGap, 40.0}},
      {shop,
       Face::Back,
       PathKind::Backstage,
       shopBackBackstage,
       10.0,
       {shopHalf + backstageGap, 40.0}},
      {depot,
       Face::Front,
       PathKind::Backstage,
       depotFrontBackstage,
       10.0,
       {-depotHalf - backstageGap, -40.0}},
  };
  return park;
}

// Guest path 1 runs along z = 0 from x = -20 to x = 10, so (x, 0) lies at distance x + 20 on it.
// Path 2 starts at its end and runs to -z, a corner, and path 3 starts on it at x = -10 and runs
// to +z. Each shop's front door connects to path 1: shop 4's and shop 5's from either side to its
// middle, one point, shop 6's past its end to the corner, and shop 7's to half the tolerance from
// path 3's start.
inline ConnectedPark connectionPlaces() {
  const double gap = gapWithinReach(PathKind::Guest);
  const double center = gap + (boxSize(BoxKind::Shop).Depth / 2.0);
  const double nearJunction = -10.0 + (JUNCTION_TOLERANCE / 2.0);
  const auto shop = [](uint64_t key, double x, double z, double facingX, double facingZ) {
    return boxOf(key, BoxKind::Shop, Pose{.X = x, .Z = z, .FacingX = facingX, .FacingZ = facingZ});
  };
  ConnectedPark park{
      "connections mid-line, at one point, at a corner, and near a junction",
      ParkIntent{.Entrances = {},
                 .Paths = {pathOf(1, PathKind::Guest, {{-20.0, 0.0}, {10.0, 0.0}}),
                           pathOf(2, PathKind::Guest, {{10.0, 0.0}, {10.0, -20.0}}),
                           pathOf(3, PathKind::Guest, {{-10.0, 0.0}, {-10.0, 20.0}})},
                 // Front doors at (0, gap), (0, -gap), (10 + gap, 0), and (nearJunction, -gap).
                 .Boxes = {shop(4, 0.0, center, 0.0, -1.0), shop(5, 0.0, -center, 0.0, 1.0),
                           shop(6, 10.0 + center, 0.0, -1.0, 0.0),
                           shop(7, nearJunction, -center, 0.0, 1.0)}},
      {},
      {{EntityKey{1}, 10.0}, {EntityKey{3}, 0.0}, {EntityKey{1}, 30.0}, {EntityKey{2}, 0.0}}};
  const EntityKey path{1};
  park.Connections = {
      {EntityKey{4}, Face::Front, PathKind::Guest, path, 20.0, {0.0, 0.0}},
      {EntityKey{5}, Face::Front, PathKind::Guest, path, 20.0, {0.0, 0.0}},
      {EntityKey{6}, Face::Front, PathKind::Guest, path, 30.0, {10.0, 0.0}},
      {EntityKey{7}, Face::Front, PathKind::Guest, path, nearJunction + 20.0, {nearJunction, 0.0}},
  };
  return park;
}

// A shop facing (3, 4), and a guest line across its facing, within reach ahead of its front door.
inline ConnectedPark turnedShop() {
  const ParkPoint forward{0.6, 0.8};
  const ParkPoint across{forward.Z, -forward.X};
  const double ahead = (boxSize(BoxKind::Shop).Depth / 2.0) + gapWithinReach(PathKind::Guest);
  const GroundPoint middle{.X = ahead * forward.X, .Z = ahead * forward.Z};
  return {
      "a shop turned off the axes",
      ParkIntent{.Entrances = {},
                 .Paths = {pathOf(2, PathKind::Guest,
                                  {{middle.X + (10.0 * across.X), middle.Z + (10.0 * across.Z)},
                                   {middle.X - (10.0 * across.X), middle.Z - (10.0 * across.Z)}})},
                 .Boxes = {boxOf(1, BoxKind::Shop,
                                 Pose{.X = 0.0, .Z = 0.0, .FacingX = 3.0, .FacingZ = 4.0})}},
      {{EntityKey{1}, Face::Front, PathKind::Guest, EntityKey{2}, 10.0, middle}},
      {}};
}

inline std::vector<ConnectedPark> connectedParks() {
  return {servingPark(), connectionPlaces(), turnedShop()};
}

} // namespace tpj::test

#endif
