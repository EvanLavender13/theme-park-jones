#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

// Placeholder that proves the test harness builds and runs. Real tests are derived from the
// principles and module specs in a separate pass from the implementation.
TEST_CASE("placeholder: a new world starts at tick zero") {
  const tpj::World world;
  REQUIRE(world.Tick == 0);
}
