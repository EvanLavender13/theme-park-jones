#include "support/synthetic_types.h"

#include "sim/entity_key.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace tpj {
namespace {

using test::buildWorld;
using test::componentOf;
using test::makeSchema;
using test::Probe;

struct UnregisteredGadget {
  int Value = 0;
};

// A key that buildWorld gives a probe.
constexpr EntityKey FIRST{1};

// An unregistered component is invisible to the walk, so without the check a copy would drop it
// silently and the world would compare and hash equal to one without it.
TEST_CASE("in debug builds, the walk refuses a world holding state it cannot cover, and accepts "
          "every world it can") {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
  World world = buildWorld(makeSchema());
  // Only a NaN is refused, so an infinity is an ordinary double, and an unregistered type whose
  // storage is empty holds nothing the walk misses.
  componentOf<Probe>(world, FIRST).Level = std::numeric_limits<double>::infinity();
  world.emplace<UnregisteredGadget>(FIRST);
  world.remove<UnregisteredGadget>(FIRST);
  REQUIRE_NOTHROW(hashWorld(world));

  world.emplace<UnregisteredGadget>(FIRST, UnregisteredGadget{.Value = 1});
  REQUIRE_THROWS_AS(hashWorld(world), WorldInvariantError);
}

} // namespace
} // namespace tpj
