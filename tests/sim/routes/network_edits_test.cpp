#include "support/park_worlds.h"

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
#include <cstddef>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using test::sameBits;

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

// One park edit: mostly adding and deleting paths, which the networks follow, with some boxes,
// which block paths.
ParkEdit edit(Draws &draws, const World &world) {
  const std::vector<ParkPath> paths = parkPaths(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  const uint64_t choice = draws.below(10);
  if (choice < 5) {
    return addPath(draws, paths);
  }
  if (choice < 8 && !paths.empty()) {
    return DeletePath{paths[draws.below(paths.size())].Key};
  }
  if (choice == 8 || boxes.empty()) {
    return AddBox{static_cast<BoxKind>(draws.below(2)), posed(draws)};
  }
  const EntityKey box = boxes[draws.below(boxes.size())].Key;
  if (draws.oneIn(2)) {
    return MoveBox{box, posed(draws)};
  }
  return DeleteBox{box};
}

// Networks are equal when their carriers, stops, and anchors are, doubles bit for bit.
bool sameNetwork(const Network &left, const Network &right) {
  if (left.nodeCount() != right.nodeCount() || left.carriers().size() != right.carriers().size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.carriers().size(); ++index) {
    const Carrier &a = left.carriers()[index];
    const Carrier &b = right.carriers()[index];
    const bool samePoints =
        std::ranges::equal(a.Points, b.Points, [](const CarrierPoint &p, const CarrierPoint &q) {
          return sameBits(p.X, q.X) && sameBits(p.Z, q.Z) && sameBits(p.Distance, q.Distance);
        });
    const bool sameStops =
        std::ranges::equal(a.Stops, b.Stops, [](const CarrierStop &p, const CarrierStop &q) {
          return sameBits(p.Distance, q.Distance) && p.Node == q.Node;
        });
    if (a.Key != b.Key || !samePoints || !sameStops) {
      return false;
    }
  }
  for (uint32_t node = 0; node < left.nodeCount(); ++node) {
    if (left.nodeAnchor(node) != right.nodeAnchor(node)) {
      return false;
    }
  }
  return true;
}

bool sameNetworks(const World &left, const World &right) {
  return sameNetwork(parkNetwork(left, PathKind::Guest), parkNetwork(right, PathKind::Guest)) &&
         sameNetwork(parkNetwork(left, PathKind::Backstage),
                     parkNetwork(right, PathKind::Backstage));
}

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

TEST_CASE("Every world a random edit sequence reaches has the networks of its save loaded and "
          "resolved") {
  Draws draws(21);
  World world = makeNewPark(5);
  bool reachedJunction = false;
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
  }
  CHECK(reachedJunction);
}

TEST_CASE("A candidate made with an edit has the networks of the world after a cycle applies it") {
  Draws draws(22);
  World world = makeNewPark(5);
  bool reachedJunction = false;
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    queueEdit(queue, edit(draws, world));
    const World candidate = makeCandidate(world, queue);
    stepWorld(world, queue);

    REQUIRE(sameNetworks(candidate, world));
    reachedJunction = reachedJunction || hasJunction(parkNetwork(world, PathKind::Guest)) ||
                      hasJunction(parkNetwork(world, PathKind::Backstage));
  }
  CHECK(reachedJunction);
}

TEST_CASE("Every world a random edit sequence reaches resolves without throwing, including worlds "
          "with no paths, a lone path, or paths of one kind only") {
  Draws draws(28);
  World world = test::worldOf({});
  bool reachedNone = false;
  bool reachedLone = false;
  bool reachedOneKind = false;
  bool reachedBothKinds = false;
  const auto note = [&](const World &resolved) {
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
}

} // namespace
} // namespace tpj
