#include "support/park_worlds.h"
#include "support/route_edits.h"
#include "support/same_networks.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::routeEdit;
using test::RouteEditDraws;
using test::sameNetworks;

const Carrier *findCarrier(const Network &network, EntityKey key) {
  const auto found = std::ranges::find_if(
      network.carriers(), [key](const Carrier &carrier) { return carrier.Key == key; });
  return found == network.carriers().end() ? nullptr : &*found;
}

// Whether the worlds a sequence reached had a box's door with a connector, and a door serving a
// kind with none.
struct DoorCoverage {
  bool ConnectedBox = false;
  bool Unconnected = false;

  void note(const World &world) {
    const auto door = [&](EntityKey entity, Face face, PathKind kind) {
      const bool connected =
          findCarrier(parkNetwork(world, kind), connectorKey(entity, face)) != nullptr;
      Unconnected = Unconnected || !connected;
      return connected;
    };
    for (const ParkEntrance &entrance : parkEntrances(world)) {
      door(entrance.Key, Face::Front, PathKind::Guest);
    }
    for (const ParkBox &box : parkBoxes(world)) {
      const bool front = door(box.Key, Face::Front,
                              box.Kind == BoxKind::Shop ? PathKind::Guest : PathKind::Backstage);
      const bool back = box.Kind == BoxKind::Shop && door(box.Key, Face::Back, PathKind::Backstage);
      ConnectedBox = ConnectedBox || front || back;
    }
  }
};

// Whether some node is shared by two stops, so the sequence reached meetings. Every node has at
// least one stop, so more stops than nodes means some node has two.
bool hasJunction(const Network &network) {
  std::size_t stops = 0;
  for (const Carrier &carrier : network.carriers()) {
    stops += carrier.Stops.size();
  }
  return stops > network.nodeCount();
}

constexpr int CYCLES = 60;

TEST_CASE("Every world a random edit sequence reaches has the networks, anchors included, of its "
          "save loaded and resolved") {
  RouteEditDraws draws(21);
  World world = makeNewPark(5);
  bool reachedJunction = false;
  DoorCoverage doors;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, routeEdit(draws, world));
    stepWorld(world, queue);

    World loaded = loadWorld(makeParkSchema(), saveWorld(world));
    resolveWorld(loaded);
    REQUIRE(sameNetworks(loaded, world));
    reachedJunction = reachedJunction || hasJunction(parkNetwork(world, PathKind::Guest)) ||
                      hasJunction(parkNetwork(world, PathKind::Backstage));
    doors.note(world);
  }
  CHECK(reachedJunction);
  CHECK(doors.ConnectedBox);
  CHECK(doors.Unconnected);
}

TEST_CASE("A candidate made with an edit has the networks, anchors included, of the world after a "
          "cycle applies it") {
  RouteEditDraws draws(22);
  World world = makeNewPark(5);
  bool reachedJunction = false;
  DoorCoverage doors;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, routeEdit(draws, world));
    const World candidate = makeCandidate(world, queue);
    stepWorld(world, queue);

    REQUIRE(sameNetworks(candidate, world));
    reachedJunction = reachedJunction || hasJunction(parkNetwork(world, PathKind::Guest)) ||
                      hasJunction(parkNetwork(world, PathKind::Backstage));
    doors.note(world);
  }
  CHECK(reachedJunction);
  CHECK(doors.ConnectedBox);
  CHECK(doors.Unconnected);
}

} // namespace
} // namespace tpj
