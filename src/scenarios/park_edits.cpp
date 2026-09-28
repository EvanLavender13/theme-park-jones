#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/world.h"

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace tpj {
namespace {

constexpr uint64_t EDIT_PURPOSE = hashName("park-edits");
constexpr uint64_t EDIT_INTERVAL = 10;
// Coordinates reach 8 m past the park's edge, so some edits leave it.
constexpr double REACH = PARK_SIZE / 2.0 + 8.0;
// A path's next point lies at most this far from its last along each axis.
constexpr double PATH_STEP = 40.0;
// Weights of AddPath, AddBox, MoveBox, DeletePath, and DeleteBox.
constexpr std::array<uint64_t, 5> EDIT_WEIGHTS{3, 4, 3, 1, 1};

// The cycle's next draw, in [0, 1).
double drawEdit(const World &world, uint64_t &index) {
  return drawUniform(drawKey(world, NULL_KEY, EDIT_PURPOSE, index++));
}

// A draw from -reach to reach.
double drawAround(const World &world, uint64_t &index, double reach) {
  return (2.0 * drawEdit(world, index) - 1.0) * reach;
}

// Any key the counter has given, live or not.
EntityKey drawGivenKey(const World &world, uint64_t &index) {
  const double given = static_cast<double>(world.nextKey() - 1);
  return static_cast<EntityKey>(1 + static_cast<uint64_t>(drawEdit(world, index) * given));
}

// A braced list evaluates its elements in order, so the draws are made in a fixed order.
Pose drawPose(const World &world, uint64_t &index) {
  return Pose{drawAround(world, index, REACH), drawAround(world, index, REACH),
              drawAround(world, index, 1.0), drawAround(world, index, 1.0)};
}

AddPath drawPath(const World &world, uint64_t &index) {
  AddPath path{drawEdit(world, index) < 0.5 ? PathKind::Guest : PathKind::Backstage, {}};
  const size_t count = 2 + static_cast<size_t>(drawEdit(world, index) * 4.0);
  ParkPoint point{drawAround(world, index, REACH), drawAround(world, index, REACH)};
  path.Points.push_back(point);
  while (path.Points.size() < count) {
    // One point in eight repeats the last, which the command drops.
    if (drawEdit(world, index) >= 0.125) {
      point = ParkPoint{point.X + drawAround(world, index, PATH_STEP),
                        point.Z + drawAround(world, index, PATH_STEP)};
    }
    path.Points.push_back(point);
  }
  return path;
}

// Before every tenth cycle, one drawn edit of any kind.
void queueEdit(const World &world, CommandQueue &commands) {
  if (world.Tick % EDIT_INTERVAL != 0) {
    return;
  }
  uint64_t index = 0;
  const size_t kind = drawPick(drawKey(world, NULL_KEY, EDIT_PURPOSE, index++), EDIT_WEIGHTS);
  if (kind == 0) {
    commands.push(drawPath(world, index));
  } else if (kind == 1) {
    const BoxKind box = drawEdit(world, index) < 0.75 ? BoxKind::Shop : BoxKind::Depot;
    commands.push(AddBox{box, drawPose(world, index)});
  } else if (kind == 2) {
    const EntityKey box = drawGivenKey(world, index);
    commands.push(MoveBox{box, drawPose(world, index)});
  } else if (kind == 3) {
    commands.push(DeletePath{drawGivenKey(world, index)});
  } else {
    commands.push(DeleteBox{drawGivenKey(world, index)});
  }
}

} // namespace

Scenario parkEditsScenario() {
  return Scenario{
      .Name = "park-edits",
      .Seed = 4004,
      .MakeSchema = makeParkSchema,
      .Populate = [](World &world) { world = makeNewPark(world.Seed); },
      .QueueCommands = queueEdit,
  };
}

} // namespace tpj
