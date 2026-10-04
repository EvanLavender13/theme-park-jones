#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/type_info.hpp>

#include <algorithm>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::intentOf;
using test::ParkIntent;
using test::samePoints;
using test::samePose;
using test::worldOf;

// Kinds with no enumerator, which a command can still hold.

constexpr EntityKey ENTRANCE{1};
constexpr EntityKey TEMPLATE_PATH{2};
constexpr EntityKey BACKSTAGE{3};
constexpr EntityKey SHOP{4};
constexpr EntityKey DEPOT{5};

constexpr Pose facingSouth(double x, double z) { return Pose{x, z, 0.0, -1.0}; }

// The template's entrance and guest path, a backstage path along x = -40 from z = -50 to 50, a
// shop covering x from 36 to 44 and z from -3 to 3, and a depot covering x from 74 to 86 and z
// from 36 to 44. Its next key is 6.
World editedPark() {
  return worldOf(ParkIntent{{{ENTRANCE, Pose{0.0, 126.5, 0.0, -1.0}}},
                            {{TEMPLATE_PATH, PathKind::Guest, {{0.0, 123.0}, {0.0, 103.0}}},
                             {BACKSTAGE, PathKind::Backstage, {{-40.0, -50.0}, {-40.0, 50.0}}}},
                            {{SHOP, BoxKind::Shop, facingSouth(40.0, 0.0)},
                             {DEPOT, BoxKind::Depot, facingSouth(80.0, 40.0)}}});
}

bool sameIntent(const World &world, const ParkIntent &expected) {
  const auto entrances = parkEntrances(world);
  const auto paths = parkPaths(world);
  const auto boxes = parkBoxes(world);
  const bool sameEntrances = std::ranges::equal(
      entrances, expected.Entrances, [](const ParkEntrance &left, const ParkEntrance &right) {
        return left.Key == right.Key && samePose(left.At, right.At);
      });
  const bool samePaths =
      std::ranges::equal(paths, expected.Paths, [](const ParkPath &left, const ParkPath &right) {
        return left.Key == right.Key && left.Kind == right.Kind &&
               samePoints(left.Points, right.Points);
      });
  const bool sameBoxes =
      std::ranges::equal(boxes, expected.Boxes, [](const ParkBox &left, const ParkBox &right) {
        return left.Key == right.Key && left.Kind == right.Kind && samePose(left.At, right.At);
      });
  return sameEntrances && samePaths && sameBoxes;
}

std::vector<EntityKey> keysOf(const ParkIntent &intent) {
  std::vector<EntityKey> keys;
  keys.reserve(intent.Entrances.size() + intent.Paths.size() + intent.Boxes.size());
  for (const ParkEntrance &entrance : intent.Entrances) {
    keys.push_back(entrance.Key);
  }
  for (const ParkPath &path : intent.Paths) {
    keys.push_back(path.Key);
  }
  for (const ParkBox &box : intent.Boxes) {
    keys.push_back(box.Key);
  }
  std::ranges::sort(keys);
  return keys;
}

// The world holds exactly the expected intent, and its entities, seed, and tick are those of the
// intent and the world before.
void checkHoldsOnly(const World &world, const World &before, const ParkIntent &expected) {
  CHECK(sameIntent(world, expected));
  CHECK(world.keys() == keysOf(expected));
  CHECK(world.Seed == before.Seed);
  CHECK(world.Tick == before.Tick);
}

template <typename Command>
bool acceptedWithoutThrowing(const World &world, const Command &command) {
  bool accepted = true;
  CHECK_NOTHROW(accepted = isAccepted(world, command));
  return accepted;
}

TEST_CASE("An accepted AddPath gives the next key an entity holding a path of its kind through its "
          "kept points, and changes nothing else") {
  World world = editedPark();
  const World before = copyWorld(world);
  // A repeated point and one within 1 cm of the last kept are dropped.
  const AddPath command{
      PathKind::Guest,
      {{-60.0, -20.0}, {-60.0, -20.0}, {-60.0, -19.995}, {-60.0, 0.0}, {-50.0, 10.0}}};
  const std::vector<ParkPoint> kept = keptPoints(command.Points);
  REQUIRE(kept.size() == 3);
  REQUIRE(isAccepted(world, command));

  applyCommand(world, command);

  ParkIntent expected = intentOf(before);
  const EntityKey added{before.nextKey()};
  expected.Paths.push_back({added, PathKind::Guest, kept});
  checkHoldsOnly(world, before, expected);
  CHECK(world.nextKey() == before.nextKey() + 1);
}

TEST_CASE("An accepted AddBox gives the next key an entity holding a box of its kind with its pose "
          "exactly as given, and changes nothing else") {
  World world = editedPark();
  const World before = copyWorld(world);
  // Negative zero and an unnormalized facing, which the box keeps bit for bit.
  const AddBox command{BoxKind::Depot, Pose{-0.0, -70.25, 3.0, -4.0}};
  REQUIRE(isAccepted(world, command));

  applyCommand(world, command);

  ParkIntent expected = intentOf(before);
  expected.Boxes.push_back({EntityKey{before.nextKey()}, BoxKind::Depot, command.At});
  checkHoldsOnly(world, before, expected);
  CHECK(world.nextKey() == before.nextKey() + 1);
}

TEST_CASE("An accepted MoveBox replaces the box's pose exactly with the one given, keeping its key "
          "and kind, and changes nothing else") {
  World world = editedPark();
  const World before = copyWorld(world);
  const MoveBox command{SHOP, Pose{60.5, -0.0, -0.5, 2.0}};
  REQUIRE(isAccepted(world, command));

  applyCommand(world, command);

  ParkIntent expected = intentOf(before);
  for (ParkBox &box : expected.Boxes) {
    if (box.Key == SHOP) {
      box.At = command.At;
    }
  }
  checkHoldsOnly(world, before, expected);
  CHECK(world.nextKey() == before.nextKey());
}

TEST_CASE("An accepted DeletePath or DeleteBox destroys the entity it names, and changes nothing "
          "else") {
  World world = editedPark();
  const World before = copyWorld(world);
  const DeletePath deletePath{BACKSTAGE};
  const DeleteBox deleteBox{SHOP};
  REQUIRE(isAccepted(world, deletePath));
  applyCommand(world, deletePath);
  REQUIRE(isAccepted(world, deleteBox));
  applyCommand(world, deleteBox);

  ParkIntent expected = intentOf(before);
  std::erase_if(expected.Paths, [](const ParkPath &path) { return path.Key == BACKSTAGE; });
  std::erase_if(expected.Boxes, [](const ParkBox &box) { return box.Key == SHOP; });
  checkHoldsOnly(world, before, expected);
  CHECK(world.nextKey() == before.nextKey());
}

template <typename Command>
void checkRefusalChangesNothing(const World &before, const Command &command) {
  World world = copyWorld(before);
  REQUIRE_FALSE(isAccepted(world, command));
  applyCommand(world, command);
  CHECK(worldsEqual(world, before));
  CHECK(world.nextKey() == before.nextKey());
}

TEST_CASE("A refused command given to applyCommand leaves the world equal, its next key included") {
  const World before = editedPark();
  checkRefusalChangesNothing(before, AddPath{PathKind::Guest, {{127.9, -10.0}, {127.9, 10.0}}});
  checkRefusalChangesNothing(before, AddBox{BoxKind::Shop, facingSouth(42.0, 0.0)});
  checkRefusalChangesNothing(before, MoveBox{SHOP, facingSouth(-40.0, 0.0)});
  checkRefusalChangesNothing(before, DeletePath{SHOP});
  checkRefusalChangesNothing(before, DeleteBox{EntityKey{99}});
}

// Two copies of one box, then a delete of the key the first will take. Each is judged on the world
// as the commands before it left it: the first is applied, its copy overlaps it and is refused,
// and the delete finds it. Judged on the world before them all, both copies would be applied and
// the delete refused.
CommandQueue editsJudgedInTurn(const World &world) {
  const AddBox place{BoxKind::Shop, facingSouth(80.0, -80.0)};
  const DeleteBox removeFirst{EntityKey{world.nextKey()}};
  REQUIRE(isAccepted(world, place));
  REQUIRE_FALSE(isAccepted(world, removeFirst));

  CommandQueue queue;
  queue.push(place);
  queue.push(place);
  queue.push(removeFirst);
  return queue;
}

void checkJudgedInTurn(const World &after, const World &before) {
  CHECK(sameIntent(after, intentOf(before)));
  CHECK(after.nextKey() == before.nextKey() + 1);
}

TEST_CASE("Commands queued for a cycle are each applied exactly when isAccepted is true on the "
          "world as it is when they apply") {
  World world = editedPark();
  resolveWorld(world);
  const World before = copyWorld(world);
  CommandQueue queue = editsJudgedInTurn(world);

  stepWorld(world, queue);

  checkJudgedInTurn(world, before);
}

TEST_CASE("A command naming no intent of the kind it acts on is refused, without throwing") {
  World world = editedPark();
  const Pose free = facingSouth(60.0, 70.0);
  // A box and a path given keys by the counter, then deleted.
  const EntityKey deletedBox{world.nextKey()};
  const EntityKey deletedPath{world.nextKey() + 1};
  const AddBox placeBox{BoxKind::Shop, facingSouth(60.0, -60.0)};
  const AddPath drawPath{PathKind::Backstage, {{100.0, -80.0}, {100.0, -60.0}}};
  REQUIRE(isAccepted(world, placeBox));
  applyCommand(world, placeBox);
  REQUIRE(isAccepted(world, drawPath));
  applyCommand(world, drawPath);
  REQUIRE(isAccepted(world, DeleteBox{deletedBox}));
  applyCommand(world, DeleteBox{deletedBox});
  REQUIRE(isAccepted(world, DeletePath{deletedPath}));
  applyCommand(world, DeletePath{deletedPath});

  // The same commands naming what they act on are accepted.
  REQUIRE(isAccepted(world, MoveBox{SHOP, free}));
  REQUIRE(isAccepted(world, DeleteBox{SHOP}));
  REQUIRE(isAccepted(world, DeletePath{BACKSTAGE}));

  const EntityKey missing{99};
  for (const EntityKey key :
       {NULL_KEY, missing, deletedBox, deletedPath, ENTRANCE, TEMPLATE_PATH}) {
    INFO("key " << static_cast<uint64_t>(key));
    CHECK_FALSE(acceptedWithoutThrowing(world, MoveBox{key, free}));
    CHECK_FALSE(acceptedWithoutThrowing(world, DeleteBox{key}));
  }
  for (const EntityKey key : {NULL_KEY, missing, deletedPath, deletedBox, ENTRANCE, SHOP}) {
    INFO("key " << static_cast<uint64_t>(key));
    CHECK_FALSE(acceptedWithoutThrowing(world, DeletePath{key}));
  }
}

// Such intent can come only from a hand-written save.
TEST_CASE("Intent with no footprint or ground line blocks nothing, and isAccepted does not throw "
          "on a world holding it") {
  constexpr EntityKey UNFACING_DEPOT{5};
  const World world =
      worldOf(ParkIntent{{{ENTRANCE, Pose{0.0, 0.0, 0.0, 0.0}}},
                         {{EntityKey{2}, PathKind::Guest, {{20.0, 0.0}}},
                          {EntityKey{3}, PathKind::Backstage, {}},
                          {EntityKey{4}, PathKind::Guest, {{-20.0, 0.0}, {-20.0, 0.0}}}},
                         {{UNFACING_DEPOT, BoxKind::Depot, Pose{0.0, 20.0, 0.0, 0.0}}}});

  CHECK(acceptedWithoutThrowing(world, AddBox{BoxKind::Shop, facingSouth(0.0, 0.0)}));
  CHECK(acceptedWithoutThrowing(world, AddBox{BoxKind::Shop, facingSouth(20.0, 0.0)}));
  CHECK(acceptedWithoutThrowing(world, AddBox{BoxKind::Shop, facingSouth(-20.0, 0.0)}));
  CHECK(acceptedWithoutThrowing(world, AddBox{BoxKind::Shop, facingSouth(0.0, 20.0)}));
  CHECK(acceptedWithoutThrowing(world, AddPath{PathKind::Guest, {{0.0, -10.0}, {0.0, 30.0}}}));
  CHECK(acceptedWithoutThrowing(world, MoveBox{UNFACING_DEPOT, facingSouth(0.0, 0.0)}));
  CHECK(acceptedWithoutThrowing(world, DeleteBox{UNFACING_DEPOT}));
  CHECK(acceptedWithoutThrowing(world, DeletePath{EntityKey{2}}));
}

// The world each command describes, written out as intent so that its validity is judged apart
// from the command.
World withBox(const World &world, BoxKind kind, const Pose &at) {
  ParkIntent intent = intentOf(world);
  intent.Boxes.push_back({EntityKey{world.nextKey()}, kind, at});
  return worldOf(intent);
}

World withPath(const World &world, PathKind kind, const std::vector<ParkPoint> &points) {
  ParkIntent intent = intentOf(world);
  intent.Paths.push_back({EntityKey{world.nextKey()}, kind, keptPoints(points)});
  return worldOf(intent);
}

World withMove(const World &world, EntityKey key, const Pose &at) {
  ParkIntent intent = intentOf(world);
  for (ParkBox &box : intent.Boxes) {
    if (box.Key == key) {
      box.At = at;
    }
  }
  return worldOf(intent);
}

World without(const World &world, EntityKey key) {
  ParkIntent intent = intentOf(world);
  std::erase_if(intent.Paths, [key](const ParkPath &path) { return path.Key == key; });
  std::erase_if(intent.Boxes, [key](const ParkBox &box) { return box.Key == key; });
  return worldOf(intent);
}

TEST_CASE("On a physically valid world, a command naming what it acts on is accepted exactly when "
          "the world it describes is physically valid") {
  const World world = editedPark();
  REQUIRE(isPhysicallyValid(world));

  const auto checkBox = [&](BoxKind kind, const Pose &at, bool valid) {
    INFO("box at " << at.X << ", " << at.Z);
    CHECK(isPhysicallyValid(withBox(world, kind, at)) == valid);
    CHECK(isAccepted(world, AddBox{kind, at}) == valid);
  };
  const auto checkPath = [&](PathKind kind, const std::vector<ParkPoint> &points, bool valid) {
    INFO("path from " << points.front().X << ", " << points.front().Z);
    CHECK(isPhysicallyValid(withPath(world, kind, points)) == valid);
    CHECK(isAccepted(world, AddPath{kind, points}) == valid);
  };
  const auto checkMove = [&](EntityKey key, const Pose &at, bool valid) {
    INFO("move to " << at.X << ", " << at.Z);
    CHECK(isPhysicallyValid(withMove(world, key, at)) == valid);
    CHECK(isAccepted(world, MoveBox{key, at}) == valid);
  };

  SECTION("AddBox") {
    // Far from any path, turned to an unnormalized facing.
    checkBox(BoxKind::Shop, Pose{80.0, -80.0, -2.0, 0.5}, true);
    // Flush with the backstage path's side at x = -39.
    checkBox(BoxKind::Shop, facingSouth(-35.0, 0.0), true);
    checkBox(BoxKind::Shop, facingSouth(-35.5, 0.0), false);
    checkBox(BoxKind::Shop, facingSouth(44.0, 0.0), false);
    // Reaching 1 m into the entrance's side, clear of the template path.
    checkBox(BoxKind::Shop, facingSouth(8.0, 124.0), false);
    checkBox(BoxKind::Depot, facingSouth(125.0, 0.0), false);
    checkBox(BoxKind::Shop, Pose{80.0, -80.0, 0.0, 0.0}, false);
  }

  SECTION("AddPath") {
    checkPath(PathKind::Guest, {{-10.0, 113.0}, {10.0, 113.0}}, true);
    checkPath(PathKind::Backstage, {{-60.0, 0.0}, {-20.0, 0.0}}, true);
    checkPath(PathKind::Guest, {{40.0, -20.0}, {40.0, 20.0}}, false);
    // 1 m from the entrance's front, less than a guest path's half width.
    checkPath(PathKind::Guest, {{-20.0, 124.0}, {20.0, 124.0}}, false);
    checkPath(PathKind::Guest, {{127.0, -10.0}, {127.0, 10.0}}, false);
    checkPath(PathKind::Guest, {{60.0, 60.0}, {60.0, 60.005}}, false);
  }

  SECTION("MoveBox") {
    checkMove(SHOP, facingSouth(40.0, 0.0), true);
    // Onto part of its own old footprint, which the moved box no longer holds.
    checkMove(SHOP, facingSouth(42.0, 0.0), true);
    checkMove(SHOP, Pose{-80.0, -80.0, 1.0, 3.0}, true);
    checkMove(SHOP, facingSouth(-40.0, 0.0), false);
    checkMove(SHOP, facingSouth(80.0, 36.0), false);
    // A shop would fit here, x from 119 to 127, but the depot reaches x = 129.
    checkMove(DEPOT, facingSouth(123.0, 0.0), false);
  }

  SECTION("DeletePath and DeleteBox") {
    CHECK(isPhysicallyValid(without(world, TEMPLATE_PATH)));
    CHECK(isAccepted(world, DeletePath{TEMPLATE_PATH}));
    CHECK(isPhysicallyValid(without(world, BACKSTAGE)));
    CHECK(isAccepted(world, DeletePath{BACKSTAGE}));
    CHECK(isPhysicallyValid(without(world, DEPOT)));
    CHECK(isAccepted(world, DeleteBox{DEPOT}));
  }
}

} // namespace
} // namespace tpj
