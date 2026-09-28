#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();
// Kinds with no enumerator, which a command can still hold.
// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
constexpr auto NO_PATH_KIND = static_cast<PathKind>(2);
// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
constexpr auto NO_BOX_KIND = static_cast<BoxKind>(2);

// Keyed draws, so a failing sequence is the same on every build and in every run.
class Draws {
public:
  double uniform() {
    return drawUniform(DrawKey{SEED, NULL_KEY, hashName("edit-sequences"), 0, Index++});
  }
  double between(double low, double high) { return low + (high - low) * uniform(); }
  uint64_t below(uint64_t count) {
    return static_cast<uint64_t>(uniform() * static_cast<double>(count));
  }
  bool oneIn(uint64_t count) { return below(count) == 0; }

private:
  static constexpr uint64_t SEED = 11;
  uint64_t Index = 0;
};

// Coordinates reach past the park's edge, and now and then are not finite.
double coordinate(Draws &draws, double center, double reach) {
  if (draws.oneIn(24)) {
    return draws.oneIn(2) ? NOT_A_NUMBER : -INFINITE;
  }
  return center + draws.between(-reach, reach);
}

// Facings of every direction, now and then of zero length.
Pose pose(Draws &draws) {
  const double x = coordinate(draws, 0.0, 136.0);
  const double z = coordinate(draws, 0.0, 136.0);
  if (draws.oneIn(12)) {
    return Pose{x, z, 0.0, 0.0};
  }
  return Pose{x, z, draws.between(-1.0, 1.0), draws.between(-1.0, 1.0)};
}

// Keys from 0 past the counter's next, so they name missing, deleted, and wrong-kind entities as
// well as the right kind.
EntityKey key(Draws &draws, const World &world) {
  return EntityKey{draws.below(world.nextKey() + 2)};
}

AddPath addPath(Draws &draws) {
  AddPath command;
  command.Kind = draws.oneIn(10) ? NO_PATH_KIND : static_cast<PathKind>(draws.below(2));
  const std::size_t count = draws.below(6);
  double x = draws.between(-128.0, 128.0);
  double z = draws.between(-128.0, 128.0);
  for (std::size_t index = 0; index < count; ++index) {
    if (index == 0 || !draws.oneIn(4)) {
      x = coordinate(draws, x, 20.0);
      z = coordinate(draws, z, 20.0);
    }
    command.Points.push_back({x, z});
  }
  return command;
}

// Which command each kind's draws gave, and whether it was accepted where it applied.
struct Tally {
  std::array<int, 5> Accepted{};
  std::array<int, 5> Refused{};
};

template <typename Command>
void queueJudged(CommandQueue &queue, World &preview, Tally &tally, std::size_t kind,
                 const Command &command) {
  const bool accepted = isAccepted(preview, command);
  (accepted ? tally.Accepted : tally.Refused).at(kind) += 1;
  applyCommand(preview, command);
  queue.push(command);
}

TEST_CASE("Random command sequences from the new park leave a physically valid world after every "
          "cycle, whose copy is equal and whose save loads back equal and saves identically") {
  constexpr int CYCLES = 150;
  Draws draws;
  World world = makeNewPark(3);
  Tally tally;

  for (int cycle = 0; cycle < CYCLES; ++cycle) {
    INFO("cycle " << cycle);
    CommandQueue queue;
    // Only to tally what the sequence exercises: the commands are judged in turn as they apply.
    World preview = copyWorld(world);
    const uint64_t count = 1 + draws.below(3);
    for (uint64_t index = 0; index < count; ++index) {
      switch (draws.below(5)) {
      case 0:
        queueJudged(queue, preview, tally, 0, addPath(draws));
        break;
      case 1: {
        const auto kind = draws.oneIn(10) ? NO_BOX_KIND : static_cast<BoxKind>(draws.below(2));
        queueJudged(queue, preview, tally, 1, AddBox{kind, pose(draws)});
        break;
      }
      case 2:
        queueJudged(queue, preview, tally, 2, MoveBox{key(draws, preview), pose(draws)});
        break;
      case 3:
        queueJudged(queue, preview, tally, 3, DeletePath{key(draws, preview)});
        break;
      default:
        queueJudged(queue, preview, tally, 4, DeleteBox{key(draws, preview)});
        break;
      }
    }

    stepWorld(world, queue);

    REQUIRE(isPhysicallyValid(world));
    const World copy = copyWorld(world);
    REQUIRE(worldsEqual(copy, world));
    const std::string saved = saveWorld(world);
    World loaded = loadWorld(makeParkSchema(), saved);
    resolveWorld(loaded);
    REQUIRE(worldsEqual(loaded, world));
    REQUIRE(saveWorld(loaded) == saved);
  }

  // The sequence exercised every kind of command both ways.
  for (std::size_t kind = 0; kind < 5; ++kind) {
    INFO("command kind " << kind);
    CHECK(tally.Accepted.at(kind) > 0);
    CHECK(tally.Refused.at(kind) > 0);
  }
}

} // namespace
} // namespace tpj
