#include "support/scenario_world.h"

#include "scenarios/scenarios.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// The runner's default run.
constexpr int CYCLES = 3000;
constexpr double EDGE = PARK_SIZE / 2.0;

const Scenario *findParkEdits() {
  const auto scenarios = registeredScenarios();
  const auto found = std::ranges::find_if(
      scenarios, [](const Scenario &scenario) { return scenario.Name == "park-edits"; });
  return found == scenarios.end() ? nullptr : &*found;
}

enum Kind : std::size_t { ADD_PATH, ADD_BOX, MOVE_BOX, DELETE_PATH, DELETE_BOX, KINDS };

// What park-edits queued over its run, each command judged on the world it was queued for.
struct Run {
  std::vector<int> CommandCycles;
  bool QueuedMoreThanOne = false;
  bool QueuedUnknown = false;
  std::array<int, KINDS> Accepted{};
  std::array<int, KINDS> Refused{};
  std::vector<int> InvalidCycles;

  std::vector<std::vector<ParkPoint>> PathPoints;
  std::vector<Pose> Poses;
  // Each key a move or delete named, with the counter's next key when it was queued.
  std::vector<std::pair<EntityKey, uint64_t>> NamedKeys;
};

template <typename Command>
bool judge(Run &run, const World &world, const std::any &value, Kind kind) {
  const auto *command = std::any_cast<Command>(&value);
  if (command == nullptr) {
    return false;
  }
  (isAccepted(world, *command) ? run.Accepted : run.Refused).at(kind) += 1;
  return true;
}

void record(Run &run, const World &world, const std::any &value) {
  const bool known = judge<AddPath>(run, world, value, ADD_PATH) ||
                     judge<AddBox>(run, world, value, ADD_BOX) ||
                     judge<MoveBox>(run, world, value, MOVE_BOX) ||
                     judge<DeletePath>(run, world, value, DELETE_PATH) ||
                     judge<DeleteBox>(run, world, value, DELETE_BOX);
  run.QueuedUnknown = run.QueuedUnknown || !known;

  if (const auto *path = std::any_cast<AddPath>(&value)) {
    run.PathPoints.push_back(path->Points);
  }
  if (const auto *box = std::any_cast<AddBox>(&value)) {
    run.Poses.push_back(box->At);
  }
  if (const auto *move = std::any_cast<MoveBox>(&value)) {
    run.Poses.push_back(move->At);
    run.NamedKeys.emplace_back(move->Box, world.nextKey());
  }
  if (const auto *deletion = std::any_cast<DeletePath>(&value)) {
    run.NamedKeys.emplace_back(deletion->Path, world.nextKey());
  }
  if (const auto *deletion = std::any_cast<DeleteBox>(&value)) {
    run.NamedKeys.emplace_back(deletion->Box, world.nextKey());
  }
}

// Intent changes only through commands, so validity is checked after every cycle that applied
// one, and holds unchanged through the cycles between.
Run runParkEdits(const Scenario &scenario) {
  Run run;
  World world = test::startScenarioWorld(scenario, scenario.Seed);
  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    CommandQueue commands;
    scenario.QueueCommands(world, commands);
    for (const QueuedCommand &queued : commands.commands()) {
      record(run, world, queued.Value);
    }
    if (!commands.empty()) {
      run.CommandCycles.push_back(cycle);
      run.QueuedMoreThanOne = run.QueuedMoreThanOne || commands.commands().size() > 1;
    }
    const bool checked = cycle == 0 || !commands.empty();
    stepWorld(world, commands);
    if (checked && !isPhysicallyValid(world)) {
      run.InvalidCycles.push_back(cycle);
    }
  }
  return run;
}

const Run &parkEditsRun() {
  static const Run RUN = [] {
    const Scenario *scenario = findParkEdits();
    return scenario == nullptr || scenario->QueueCommands == nullptr ? Run{}
                                                                     : runParkEdits(*scenario);
  }();
  return RUN;
}

TEST_CASE("tpj_scenarios registers a scenario named park-edits that queues commands") {
  const Scenario *scenario = findParkEdits();
  REQUIRE(scenario != nullptr);
  CHECK(scenario->QueueCommands != nullptr);
}

TEST_CASE("park-edits starts from makeNewPark's template with makeParkSchema's schema") {
  const Scenario *scenario = findParkEdits();
  REQUIRE(scenario != nullptr);
  const World world = test::startScenarioWorld(*scenario, scenario->Seed);
  World park = makeNewPark(scenario->Seed);
  resolveWorld(park);

  CHECK(world.schema().sameComponents(*makeParkSchema()));
  CHECK(worldsEqual(world, park));
}

TEST_CASE(
    "park-edits queues one park command before every tenth cycle and none before the others") {
  REQUIRE(findParkEdits() != nullptr);
  const Run &run = parkEditsRun();

  CHECK_FALSE(run.QueuedUnknown);
  CHECK_FALSE(run.QueuedMoreThanOne);
  CHECK(run.CommandCycles.size() == CYCLES / 10);
  for (std::size_t index = 1; index < run.CommandCycles.size(); ++index) {
    CHECK(run.CommandCycles[index] - run.CommandCycles[index - 1] == 10);
  }
}

TEST_CASE("Across its run, park-edits queues every kind of command, and some of its adds and moves "
          "are accepted and some refused") {
  REQUIRE(findParkEdits() != nullptr);
  const Run &run = parkEditsRun();

  for (std::size_t kind = 0; kind < KINDS; ++kind) {
    INFO("command kind " << kind);
    CHECK(run.Accepted.at(kind) + run.Refused.at(kind) > 0);
  }
  for (const Kind kind : {ADD_PATH, ADD_BOX, MOVE_BOX}) {
    INFO("command kind " << static_cast<std::size_t>(kind));
    CHECK(run.Accepted.at(kind) > 0);
    CHECK(run.Refused.at(kind) > 0);
  }
}

TEST_CASE("park-edits' world is physically valid after every cycle of its run") {
  REQUIRE(findParkEdits() != nullptr);
  CHECK(parkEditsRun().InvalidCycles.empty());
}

TEST_CASE(
    "park-edits draws paths of two to five points, some repeated, poses reaching 8 m past the "
    "park's edge with facings of every direction, and keys the counter has given") {
  REQUIRE(findParkEdits() != nullptr);
  const Run &run = parkEditsRun();
  REQUIRE_FALSE(run.PathPoints.empty());
  REQUIRE_FALSE(run.Poses.empty());
  REQUIRE_FALSE(run.NamedKeys.empty());

  bool anyRepeated = false;
  for (const std::vector<ParkPoint> &points : run.PathPoints) {
    CHECK(points.size() >= 2);
    CHECK(points.size() <= 5);
    for (std::size_t index = 1; index < points.size(); ++index) {
      anyRepeated = anyRepeated || std::ranges::find(points.begin(), points.begin() + index,
                                                     points[index]) != points.begin() + index;
    }
  }
  CHECK(anyRepeated);

  bool anyBeyondEdge = false;
  std::array<bool, 4> quadrants{};
  for (const Pose &pose : run.Poses) {
    CHECK(std::abs(pose.X) <= EDGE + 8.0);
    CHECK(std::abs(pose.Z) <= EDGE + 8.0);
    anyBeyondEdge = anyBeyondEdge || std::abs(pose.X) > EDGE || std::abs(pose.Z) > EDGE;
    const std::size_t quadrant = (pose.FacingX < 0.0 ? 1U : 0U) + (pose.FacingZ < 0.0 ? 2U : 0U);
    quadrants.at(quadrant) = true;
  }
  CHECK(anyBeyondEdge);
  CHECK(std::ranges::all_of(quadrants, [](bool seen) { return seen; }));

  for (const auto &[key, next] : run.NamedKeys) {
    CHECK(static_cast<uint64_t>(key) >= 1);
    CHECK(static_cast<uint64_t>(key) < next);
  }
}

} // namespace
} // namespace tpj
