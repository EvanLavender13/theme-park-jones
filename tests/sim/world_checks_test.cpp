#include "support/synthetic_types.h"

#include "sim/entity_key.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <stdint.h>

namespace tpj {
namespace {

using test::buildWorld;
using test::makeSchema;

struct UnregisteredGadget {
  int Value = 0;
};

// A key that buildWorld gives a probe.
constexpr EntityKey FIRST{1};

// An unregistered component is invisible to the walk, so if the world kept it, a copy would drop
// it silently and the world would compare and hash equal to one without it.
TEST_CASE(
    "adding a component of an unregistered type throws and leaves the world's hash unchanged") {
  World world = buildWorld(makeSchema());
  const uint64_t before = hashWorld(world);

  REQUIRE_THROWS_AS(world.emplace<UnregisteredGadget>(FIRST, UnregisteredGadget{.Value = 1}),
                    WorldInvariantError);
  CHECK(hashWorld(world) == before);
}

} // namespace
} // namespace tpj
