#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

namespace tpj {
namespace {

using test::ParkIntent;
using test::worldOf;

constexpr EntityKey ENTRANCE{1};
constexpr EntityKey TEMPLATE_PATH{2};
constexpr EntityKey BACKSTAGE{3};
constexpr EntityKey SHOP{4};

constexpr Pose facingSouth(double x, double z) { return Pose{x, z, 0.0, -1.0}; }

// The template's entrance and guest path, a backstage path along x = -40 from z = -50 to 50, and a
// shop covering x from 36 to 44 and z from -3 to 3.
World editablePark() {
  return worldOf(ParkIntent{{{ENTRANCE, Pose{0.0, 126.5, 0.0, -1.0}}},
                            {{TEMPLATE_PATH, PathKind::Guest, {{0.0, 123.0}, {0.0, 103.0}}},
                             {BACKSTAGE, PathKind::Backstage, {{-40.0, -50.0}, {-40.0, 50.0}}}},
                            {{SHOP, BoxKind::Shop, facingSouth(40.0, 0.0)}}});
}

// Each command is checked where isAccepted is true and where it is false, so the edit's answer
// must follow the command's rather than be fixed.
template <typename Command>
void checkSameAnswer(const World &world, const Command &accepted, const Command &refused) {
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));
  CHECK(isAccepted(world, ParkEdit{accepted}));
  CHECK_FALSE(isAccepted(world, ParkEdit{refused}));
}

TEST_CASE("isAccepted on a ParkEdit equals isAccepted on the command it holds") {
  const World world = editablePark();
  checkSameAnswer(world, AddPath{PathKind::Guest, {{60.0, 60.0}, {60.0, 80.0}}},
                  AddPath{PathKind::Guest, {{40.0, -20.0}, {40.0, 20.0}}});
  checkSameAnswer(world, AddBox{BoxKind::Depot, facingSouth(80.0, -80.0)},
                  AddBox{BoxKind::Shop, facingSouth(42.0, 0.0)});
  checkSameAnswer(world, MoveBox{SHOP, facingSouth(60.0, 60.0)},
                  MoveBox{SHOP, facingSouth(-40.0, 0.0)});
  checkSameAnswer(world, DeletePath{BACKSTAGE}, DeletePath{SHOP});
  checkSameAnswer(world, DeleteBox{SHOP}, DeleteBox{BACKSTAGE});
}

template <typename Command> void checkSameCycle(const World &resolved, const Command &command) {
  REQUIRE(isAccepted(resolved, command));
  World direct = copyWorld(resolved);
  World edited = copyWorld(resolved);
  CommandQueue directQueue;
  directQueue.push(command);
  CommandQueue editQueue;
  queueEdit(editQueue, ParkEdit{command});

  stepWorld(direct, directQueue);
  stepWorld(edited, editQueue);

  CHECK(worldsEqual(edited, direct));
  CHECK(edited.nextKey() == direct.nextKey());
}

TEST_CASE("A cycle with queueEdit's queue gives the world a cycle with the command pushed directly "
          "gives") {
  World world = editablePark();
  resolveWorld(world);
  checkSameCycle(world, AddPath{PathKind::Backstage, {{60.0, 60.0}, {60.0, 80.0}, {70.0, 90.0}}});
  checkSameCycle(world, AddBox{BoxKind::Depot, Pose{80.0, -80.0, 3.0, -4.0}});
  checkSameCycle(world, MoveBox{SHOP, Pose{60.0, 60.0, -1.0, 2.0}});
  checkSameCycle(world, DeletePath{BACKSTAGE});
  checkSameCycle(world, DeleteBox{SHOP});
}

} // namespace
} // namespace tpj
