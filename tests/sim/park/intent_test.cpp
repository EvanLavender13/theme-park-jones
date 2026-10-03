#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstddef>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

bool sameBits(double left, double right) {
  return std::bit_cast<uint64_t>(left) == std::bit_cast<uint64_t>(right);
}

bool samePose(const Pose &left, const Pose &right) {
  return sameBits(left.X, right.X) && sameBits(left.Z, right.Z) &&
         sameBits(left.FacingX, right.FacingX) && sameBits(left.FacingZ, right.FacingZ);
}

bool samePoints(const std::vector<ParkPoint> &left, const std::vector<ParkPoint> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (!sameBits(left[index].X, right[index].X) || !sameBits(left[index].Z, right[index].Z)) {
      return false;
    }
  }
  return true;
}

bool sameEntrances(const std::vector<ParkEntrance> &left, const std::vector<ParkEntrance> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].Key != right[index].Key || !samePose(left[index].At, right[index].At)) {
      return false;
    }
  }
  return true;
}

bool samePaths(const std::vector<ParkPath> &left, const std::vector<ParkPath> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].Key != right[index].Key || left[index].Kind != right[index].Kind ||
        !samePoints(left[index].Points, right[index].Points)) {
      return false;
    }
  }
  return true;
}

bool sameBoxes(const std::vector<ParkBox> &left, const std::vector<ParkBox> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (left[index].Key != right[index].Key || left[index].Kind != right[index].Kind ||
        !samePose(left[index].At, right[index].At)) {
      return false;
    }
  }
  return true;
}

// Intent of every kind, both path kinds and both box kinds, holding finite values a save spells
// unusually (negative zero, the smallest subnormal, the largest double) and degenerate intent: a
// path of repeated points leaving the park, a path of one point, a path of none, a box and an
// entrance with a zero facing, and positions outside the park.
constexpr std::string_view INTENT_SAVE = "tpj-park 1\n"
                                         "seed 99\n"
                                         "tick 12\n"
                                         "next-key 9\n"
                                         "\n"
                                         "[entrance]\n"
                                         "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
                                         "8 x=-0 z=-129 facing-x=0 facing-z=0\n"
                                         "\n"
                                         "[path]\n"
                                         "2 kind=guest points=[{x=0.1 z=-3.25} {x=17 z=4} "
                                         "{x=17 z=4} {x=300 z=-128}]\n"
                                         "3 kind=backstage points=[{x=5e-324 z=-0}]\n"
                                         "4 kind=guest points=[]\n"
                                         "\n"
                                         "[box]\n"
                                         "5 kind=shop x=-20.5 z=60 facing-x=3 facing-z=4\n"
                                         "6 kind=depot x=1e+300 z=-128 facing-x=0 facing-z=0\n"
                                         "7 kind=shop x=-0 z=0 facing-x=-1.7976931348623157e+308 "
                                         "facing-z=1e-310\n";

TEST_CASE("A save of finite intent of every kind loads, saves to identical text, and loads equal") {
  const auto schema = makeParkSchema();
  const World loaded = loadWorld(schema, INTENT_SAVE);
  const std::string saved = saveWorld(loaded);
  CHECK(saved == INTENT_SAVE);

  const World reloaded = loadWorld(schema, saved);
  CHECK(worldsEqual(reloaded, loaded));
}

TEST_CASE("A world of degenerate finite intent copies and hashes as a legitimate world") {
  const World loaded = loadWorld(makeParkSchema(), INTENT_SAVE);
  CHECK_NOTHROW(validateWorld(loaded));

  const World copy = copyWorld(loaded);
  CHECK(worldsEqual(copy, loaded));
  CHECK(hashWorld(copy) == hashWorld(loaded));
}

// Three entities of each kind, so that removing the first reorders EnTT's storage, which swaps the
// last element into a removed one's place: the queries must follow keys, not storage.
constexpr std::string_view QUERY_SAVE = "tpj-park 1\n"
                                        "seed 3\n"
                                        "tick 0\n"
                                        "next-key 10\n"
                                        "\n"
                                        "[entrance]\n"
                                        "1 x=1 z=2 facing-x=0 facing-z=-1\n"
                                        "2 x=-0 z=0.1 facing-x=1e-300 facing-z=0\n"
                                        "3 x=-128 z=128 facing-x=-3 facing-z=4\n"
                                        "\n"
                                        "[path]\n"
                                        "4 kind=guest points=[{x=0 z=0} {x=10 z=0}]\n"
                                        "5 kind=backstage points=[{x=0.1 z=-0} {x=5e-324 z=200}]\n"
                                        "6 kind=guest points=[{x=-7.25 z=3}]\n"
                                        "\n"
                                        "[box]\n"
                                        "7 kind=depot x=2 z=3 facing-x=1 facing-z=0\n"
                                        "8 kind=shop x=-0 z=0.30000000000000004 facing-x=0 "
                                        "facing-z=0\n"
                                        "9 kind=depot x=129 z=-5 facing-x=-2 facing-z=-2\n";

TEST_CASE("The intent queries give every holder in ascending key order with its values exactly") {
  World world = loadWorld(makeParkSchema(), QUERY_SAVE);

  const ParkEntrance entrance1{EntityKey{1}, Pose{1.0, 2.0, 0.0, -1.0}};
  const ParkEntrance entrance2{EntityKey{2}, Pose{-0.0, 0.1, 1e-300, 0.0}};
  const ParkEntrance entrance3{EntityKey{3}, Pose{-128.0, 128.0, -3.0, 4.0}};
  const ParkPath path4{EntityKey{4}, PathKind::Guest, {{0.0, 0.0}, {10.0, 0.0}}};
  const ParkPath path5{EntityKey{5}, PathKind::Backstage, {{0.1, -0.0}, {5e-324, 200.0}}};
  const ParkPath path6{EntityKey{6}, PathKind::Guest, {{-7.25, 3.0}}};
  const ParkBox box7{EntityKey{7}, BoxKind::Depot, Pose{2.0, 3.0, 1.0, 0.0}};
  const ParkBox box8{EntityKey{8}, BoxKind::Shop, Pose{-0.0, 0.30000000000000004, 0.0, 0.0}};
  const ParkBox box9{EntityKey{9}, BoxKind::Depot, Pose{129.0, -5.0, -2.0, -2.0}};

  CHECK(sameEntrances(parkEntrances(world), {entrance1, entrance2, entrance3}));
  CHECK(samePaths(parkPaths(world), {path4, path5, path6}));
  CHECK(sameBoxes(parkBoxes(world), {box7, box8, box9}));

  REQUIRE(world.destroyEntity(EntityKey{1}));
  REQUIRE(world.destroyEntity(EntityKey{4}));
  REQUIRE(world.destroyEntity(EntityKey{7}));

  CHECK(sameEntrances(parkEntrances(world), {entrance2, entrance3}));
  CHECK(samePaths(parkPaths(world), {path5, path6}));
  CHECK(sameBoxes(parkBoxes(world), {box8, box9}));
}

} // namespace
} // namespace tpj
