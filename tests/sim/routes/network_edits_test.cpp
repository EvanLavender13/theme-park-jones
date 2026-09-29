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

TEST_CASE("Every world a random edit sequence reaches resolves without throwing, including worlds "
          "with no paths, a lone path, paths of one kind only, a connected box, and an unconnected "
          "door") {
  RouteEditDraws draws(28);
  World world = test::worldOf({});
  bool reachedNone = false;
  bool reachedLone = false;
  bool reachedOneKind = false;
  bool reachedBothKinds = false;
  DoorCoverage doors;
  const auto note = [&](const World &resolved) {
    doors.note(resolved);
    const std::vector<ParkPath> paths = parkPaths(resolved);
    const auto guests = std::ranges::count_if(
        paths, [](const ParkPath &path) { return path.Kind == PathKind::Guest; });
    const auto count = static_cast<std::ptrdiff_t>(paths.size());
    reachedNone = reachedNone || count == 0;
    reachedLone = reachedLone || count == 1;
    reachedOneKind = reachedOneKind || (count > 1 && (guests == 0 || guests == count));
    reachedBothKinds = reachedBothKinds || (guests > 0 && guests < count);
  };

  REQUIRE_NOTHROW(resolveWorld(world));
  note(world);
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, routeEdit(draws, world));
    REQUIRE_NOTHROW(stepWorld(world, queue));
    note(world);
  }
  CHECK(reachedNone);
  CHECK(reachedLone);
  CHECK(reachedOneKind);
  CHECK(reachedBothKinds);
  CHECK(doors.ConnectedBox);
  CHECK(doors.Unconnected);
}

bool sameDistances(const std::vector<CarrierStop> &left, const std::vector<CarrierStop> &right) {
  return std::ranges::equal(left, right, [](const CarrierStop &a, const CarrierStop &b) {
    return test::sameBits(a.Distance, b.Distance);
  });
}

TEST_CASE("Across every cycle of a random edit sequence, carryOver keeps a place on a path in both "
          "networks, carries a place on a connector in both to that connector, and retires a place "
          "on a carrier gone") {
  RouteEditDraws draws(24);
  World world = makeNewPark(5);
  resolveWorld(world);
  bool reachedSplitPath = false;
  bool reachedMovedConnector = false;
  bool reachedGone = false;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    std::set<EntityKey> paths;
    for (const ParkPath &path : parkPaths(world)) {
      paths.insert(path.Key);
    }
    const Network guest = parkNetwork(world, PathKind::Guest);
    const Network backstage = parkNetwork(world, PathKind::Backstage);
    CommandQueue queue;
    queueEdit(queue, routeEdit(draws, world));
    stepWorld(world, queue);

    for (const Network *before : {&guest, &backstage}) {
      const Network &after =
          parkNetwork(world, before == &guest ? PathKind::Guest : PathKind::Backstage);
      for (const Carrier &carrier : before->carriers()) {
        INFO("carrier " << static_cast<uint64_t>(carrier.Key));
        const Carrier *later = findCarrier(after, carrier.Key);
        const bool isPath = paths.contains(carrier.Key);
        if (later == nullptr) {
          reachedGone = true;
        } else if (isPath) {
          reachedSplitPath = reachedSplitPath || !sameDistances(carrier.Stops, later->Stops);
        } else {
          reachedMovedConnector = reachedMovedConnector || carrier.Points != later->Points;
        }
        // The carrier's two ends, and a place inside it that a new node may split off.
        const double length = carrier.Points.back().Distance;
        for (const double distance : {0.0, length / 2.0, length}) {
          INFO("distance " << distance);
          const Place place{.Carrier = carrier.Key, .Distance = distance};
          const std::optional<Place> carried = carryOver(place, *before, after);
          if (later == nullptr) {
            CHECK_FALSE(carried.has_value());
          } else if (isPath) {
            CHECK(carried == std::optional<Place>{place});
          } else {
            REQUIRE(carried.has_value());
            const Place moved = carried.value_or(Place{});
            CHECK(moved.Carrier == carrier.Key);
            CHECK(after.resolve(moved).has_value());
          }
        }
      }
    }
  }
  CHECK(reachedSplitPath);
  CHECK(reachedMovedConnector);
  CHECK(reachedGone);
}

} // namespace
} // namespace tpj
