#include "bench/stages.h"

#include "bench/timing.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "support/bench_parks.h"
#include "views/food_overlay.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::MessageMatches;

// Few enough to keep the run short, and other than REPETITIONS, so the two counts tell apart.
constexpr uint64_t TICKS = 3;

std::vector<std::string> namesOf(const std::vector<StageResult> &stages) {
  std::vector<std::string> names;
  names.reserve(stages.size());
  for (const StageResult &stage : stages) {
    names.push_back(stage.Name);
  }
  return names;
}

TEST_CASE("benchPark gives resolution, preview, food-overlay, park-mesh, guest-mesh, and ticks in "
          "order, with preview exactly when the resolved park holds a box") {
  SECTION("warm.park holds a shop and a depot") {
    const World loaded = test::loadPark(test::parkText("warm.park"));
    CHECK(namesOf(benchPark(loaded, TICKS)) == std::vector<std::string>{"resolution", "preview",
                                                                        "food-overlay", "park-mesh",
                                                                        "guest-mesh", "ticks"});
  }
  SECTION("new.park holds no box") {
    const World loaded = test::loadPark(test::parkText("new.park"));
    CHECK(namesOf(benchPark(loaded, TICKS)) == std::vector<std::string>{"resolution",
                                                                        "food-overlay", "park-mesh",
                                                                        "guest-mesh", "ticks"});
  }
}

TEST_CASE("benchPark counts REPETITIONS calls of every stage but ticks, and one call per tick of "
          "ticks") {
  const World loaded = test::loadPark(test::parkText("warm.park"));
  const std::vector<StageResult> stages = benchPark(loaded, TICKS);
  REQUIRE_FALSE(stages.empty());
  for (const StageResult &stage : stages) {
    INFO(stage.Name);
    CHECK(stage.Times.Count == (stage.Name == "ticks" ? TICKS : REPETITIONS));
  }
}

TEST_CASE("benchPark's median of every stage lies between its least and its greatest") {
  const World loaded = test::loadPark(test::parkText("warm.park"));
  const std::vector<StageResult> stages = benchPark(loaded, TICKS);
  REQUIRE_FALSE(stages.empty());
  for (const StageResult &stage : stages) {
    INFO(stage.Name);
    CHECK(stage.Times.Least <= stage.Times.Median);
    CHECK(stage.Times.Median <= stage.Times.Greatest);
  }
}

// The results are what the work gives untimed, so reading the clock changes nothing the work does,
// and a build that timed less work would show a different result.
TEST_CASE("benchPark's result of each stage is what its work gives with nothing timed") {
  // warm.park holds guests, so the guest mesh is not empty, and a stocked shop, so ticks change it.
  const World loaded = test::loadPark(test::parkText("warm.park"));

  World resolved = copyWorld(loaded);
  resolveWorld(resolved);
  const std::vector<ParkBox> boxes = parkBoxes(resolved);
  REQUIRE_FALSE(boxes.empty());
  CommandQueue ownPose;
  ownPose.push(MoveBox{boxes.front().Key, boxes.front().At});
  World stepped = copyWorld(resolved);
  for (uint64_t tick = 0; tick < TICKS; ++tick) {
    stepWorld(stepped);
  }
  const uint64_t guestVertices = buildGuestMesh(resolved).Vertices.size();
  REQUIRE(guestVertices > 0);

  struct Expected {
    std::string Name;
    ResultKind Kind = ResultKind::Hash;
    uint64_t Result = 0;
  };
  const std::vector<Expected> expected = {
      {"resolution", ResultKind::Hash, hashWorld(resolved)},
      {"preview", ResultKind::Hash, hashWorld(makeCandidate(resolved, ownPose))},
      {"food-overlay", ResultKind::Vertices,
       buildFoodAvailabilityOverlay(resolved).Vertices.size()},
      {"park-mesh", ResultKind::Vertices, buildParkMesh(resolved).Vertices.size()},
      {"guest-mesh", ResultKind::Vertices, guestVertices},
      {"ticks", ResultKind::Hash, hashWorld(stepped)},
  };

  const std::vector<StageResult> stages = benchPark(loaded, TICKS);

  REQUIRE(stages.size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    INFO(expected[i].Name);
    CHECK(stages[i].Name == expected[i].Name);
    CHECK(stages[i].Kind == expected[i].Kind);
    CHECK(stages[i].Result == expected[i].Result);
  }
}

TEST_CASE("benchPark leaves the loaded world unchanged") {
  const World loaded = test::loadPark(test::parkText("warm.park"));
  const World before = copyWorld(loaded);
  REQUIRE(loaded.isResolvePending());

  (void)benchPark(loaded, TICKS);

  CHECK(worldsEqual(loaded, before));
  CHECK(loaded.isResolvePending());
  CHECK(loaded.Tick == before.Tick);
}

TEST_CASE("benchPark throws std::runtime_error naming the lowest-keyed box when moving it to its "
          "own pose gives no candidate") {
  const World loaded = test::loadPark(test::OVERLAPPING_SHOPS);
  CHECK_THROWS_MATCHES(
      benchPark(loaded, TICKS), std::runtime_error,
      MessageMatches(ContainsSubstring(std::string(test::OVERLAPPING_LOWER_SHOP))));
}

TEST_CASE("benchPark throws std::invalid_argument when ticks is 0") {
  const World loaded = test::loadPark(test::parkText("new.park"));
  CHECK_THROWS_AS(benchPark(loaded, 0), std::invalid_argument);
}

TEST_CASE("stageLine writes a hash as 16 lowercase hexadecimal digits, a vertex count in decimal, "
          "and the times in decimal") {
  struct Case {
    std::string Name;
    StageResult Stage;
    std::string Expected;
  };
  const std::vector<Case> cases = {
      // Leading zeros are written, so every hash has 16 digits.
      {"a hash with leading zeros",
       {"resolution", {11, 1500, 1200, 9000}, ResultKind::Hash, 0xabULL},
       "stage resolution count 11 median 1500 least 1200 greatest 9000 hash 00000000000000ab"},
      // A hash above INT64_MAX, which written as signed would be negative.
      {"a hash with its top bit set",
       {"ticks", {300, 42, 7, 5'000'000'000LL}, ResultKind::Hash, 0xfedcba9876543210ULL},
       "stage ticks count 300 median 42 least 7 greatest 5000000000 hash fedcba9876543210"},
      // 4096 is 1000 in hexadecimal, so a count written in the hash's base would show.
      {"a vertex count",
       {"park-mesh", {11, 20, 10, 30}, ResultKind::Vertices, 4096},
       "stage park-mesh count 11 median 20 least 10 greatest 30 vertices 4096"},
  };
  for (const Case &example : cases) {
    INFO(example.Name);
    CHECK(stageLine(example.Stage) == example.Expected);
  }
}

TEST_CASE("parkLine writes the path as given, the ticks, WARM_UPS, and REPETITIONS") {
  CHECK(parkLine("stress/winding-path.park", 300) ==
        "park stress/winding-path.park ticks 300 warm-ups " + std::to_string(WARM_UPS) +
            " repetitions " + std::to_string(REPETITIONS));
}

} // namespace
} // namespace tpj
