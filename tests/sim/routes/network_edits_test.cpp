#include "support/park_worlds.h"
#include "support/same_networks.h"

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::sameNetworks;

// Keyed draws, so a failing sequence is the same on every build and in every run.
class Draws {
public:
  explicit Draws(uint64_t seed) : Seed(seed) {}

  double uniform() {
    return drawUniform(DrawKey{Seed, NULL_KEY, hashName("route-edits"), 0, Index++});
  }
  double between(double low, double high) { return low + ((high - low) * uniform()); }
  uint64_t below(uint64_t count) {
    return static_cast<uint64_t>(uniform() * static_cast<double>(count));
  }
  bool oneIn(uint64_t count) { return below(count) == 0; }

private:
  uint64_t Seed;
  uint64_t Index = 0;
};

// Paths gather in the middle of the park, so new ones often meet old ones.
constexpr double REGION = 40.0;

// A point on a line already drawn, as the path tool snaps a click: one of its line points, or a
// point between two of them.
ParkPoint snappedPoint(Draws &draws, const ParkPath &path) {
  const std::vector<CarrierPoint> line = groundLine(path.Points);
  if (line.size() < 2) {
    return path.Points.front();
  }
  const std::size_t index = draws.below(line.size() - 1);
  const CarrierPoint &a = line[index];
  if (draws.oneIn(2)) {
    return {a.X, a.Z};
  }
  const CarrierPoint &b = line[index + 1];
  const double t = draws.uniform();
  return {a.X + (t * (b.X - a.X)), a.Z + (t * (b.Z - a.Z))};
}

AddPath addPath(Draws &draws, const std::vector<ParkPath> &paths) {
  // Now and then a copy of a path already drawn, which overlaps it along its whole length when
  // the kinds agree.
  if (!paths.empty() && draws.oneIn(10)) {
    const ParkPath &copied = paths[draws.below(paths.size())];
    return AddPath{static_cast<PathKind>(draws.below(2)), copied.Points};
  }
  AddPath command{static_cast<PathKind>(draws.below(2)), {}};
  const std::size_t count = 2 + draws.below(3);
  ParkPoint point{draws.between(-REGION, REGION), draws.between(-REGION, REGION)};
  for (std::size_t index = 0; index < count; ++index) {
    if (!paths.empty() && draws.oneIn(3)) {
      point = snappedPoint(draws, paths[draws.below(paths.size())]);
    } else if (index > 0) {
      point = {std::clamp(point.X + draws.between(-25.0, 25.0), -REGION, REGION),
               std::clamp(point.Z + draws.between(-25.0, 25.0), -REGION, REGION)};
    }
    command.Points.push_back(point);
  }
  return command;
}

Pose posed(Draws &draws) {
  return Pose{draws.between(-REGION, REGION), draws.between(-REGION, REGION),
              draws.between(-1.0, 1.0), draws.between(-1.0, 1.0)};
}

// A box beside a line already drawn, its front, or a shop's back, facing the line across a gap,
// so its door lies sometimes within CONNECTION_REACH of the line and sometimes beyond it.
AddBox besideLine(Draws &draws, const std::vector<ParkPath> &paths) {
  const ParkPath &path = paths[draws.below(paths.size())];
  const std::vector<CarrierPoint> line = groundLine(path.Points);
  const auto kind = static_cast<BoxKind>(draws.below(2));
  if (line.size() < 2) {
    return AddBox{kind, posed(draws)};
  }
  const std::size_t index = draws.below(line.size() - 1);
  const CarrierPoint &a = line[index];
  const CarrierPoint &b = line[index + 1];
  const double length = std::sqrt(((b.X - a.X) * (b.X - a.X)) + ((b.Z - a.Z) * (b.Z - a.Z)));
  const double side = draws.oneIn(2) ? 1.0 : -1.0;
  const double normalX = side * -(b.Z - a.Z) / length;
  const double normalZ = side * (b.X - a.X) / length;
  const double door = (pathWidth(path.Kind) / 2.0) + draws.between(0.05, 3.0);
  const double center = door + (boxSize(kind).Depth / 2.0);
  const bool backToLine = kind == BoxKind::Shop && draws.oneIn(2);
  const double facing = backToLine ? 1.0 : -1.0;
  return AddBox{kind, Pose{a.X + (center * normalX), a.Z + (center * normalZ), facing * normalX,
                           facing * normalZ}};
}

// One park edit: mostly adding and deleting paths, which the networks follow, with some boxes,
// which block paths and connect to them, and small moves, which keep a box's connectors.
ParkEdit edit(Draws &draws, const World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  const uint64_t choice = draws.below(12);
  if (choice < 4) {
    return addPath(draws, paths);
  }
  if (choice < 6 && !paths.empty()) {
    return DeletePath{paths[draws.below(paths.size())].Key};
  }
  if (choice < 9 && choice > 6 && !paths.empty()) {
    return besideLine(draws, paths);
  }
  if (choice < 9 || boxes.empty()) {
    return AddBox{static_cast<BoxKind>(draws.below(2)), posed(draws)};
  }
  const ParkBox &box = boxes[draws.below(boxes.size())];
  switch (draws.below(4)) {
  case 0:
    return MoveBox{box.Key, posed(draws)};
  case 1:
    return DeleteBox{box.Key};
  default:
    return MoveBox{box.Key,
                   Pose{box.At.X + draws.between(-1.0, 1.0), box.At.Z + draws.between(-1.0, 1.0),
                        box.At.FacingX, box.At.FacingZ}};
  }
}

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
  Draws draws(21);
  World world = makeNewPark(5);
  bool reachedJunction = false;
  DoorCoverage doors;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, edit(draws, world));
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
  Draws draws(22);
  World world = makeNewPark(5);
  bool reachedJunction = false;
  DoorCoverage doors;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, edit(draws, world));
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
  Draws draws(28);
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
    queueEdit(queue, edit(draws, world));
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
  Draws draws(24);
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
    queueEdit(queue, edit(draws, world));
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
