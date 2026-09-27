#include "synthetic_types.h"

#include "sim/entity_key.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <functional>
#include <limits>
#include <optional>
#include <stdint.h>
#include <string>

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;
using test::buildWorld;
using test::componentOf;
using test::makeSchema;
using test::Probe;
using test::SLOT_PURPOSE;

struct UnregisteredGadget {
  int Value = 0;
};

// Keys that buildWorld gives its entities.
constexpr EntityKey FIRST{1};
constexpr EntityKey SECOND{2};
constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();

// The message of the WorldInvariantError the call throws, or nothing if it throws none.
std::optional<std::string> refusal(const std::function<void()> &call) {
  try {
    call();
  } catch (const WorldInvariantError &error) {
    return std::string(error.what());
  }
  return std::nullopt;
}

std::string requireRefused(const World &world) {
  const std::optional<std::string> message = refusal([&] { validateWorld(world); });
  REQUIRE(message.has_value());
  return message.value_or("");
}

std::string decimal(EntityKey key) { return std::to_string(static_cast<uint64_t>(key)); }

// An entity with a derived key, whose decimal form cannot turn up in a message by accident,
// holding a probe whose fields are all numbers.
EntityKey addDerivedProbe(World &world, uint64_t index) {
  const EntityKey key = world.createDerivedEntity(FIRST, SLOT_PURPOSE, index);
  Probe probe;
  probe.Samples = {0.5, 2.0};
  world.Registry.emplace<Probe>(world.findEntity(key), probe);
  return key;
}

// buildWorld's world with a component the schema does not register.
World worldWithUnregisteredComponent() {
  World world = buildWorld(makeSchema());
  world.Registry.emplace<UnregisteredGadget>(world.findEntity(FIRST),
                                             UnregisteredGadget{.Value = 1});
  return world;
}

TEST_CASE("validateWorld refuses a component of an unregistered type, naming the type") {
  const World world = worldWithUnregisteredComponent();
  CHECK_THAT(requireRefused(world), ContainsSubstring("UnregisteredGadget"));
}

TEST_CASE("validateWorld accepts an empty storage of an unregistered type") {
  World world = buildWorld(makeSchema());
  const entt::entity entity = world.findEntity(FIRST);
  world.Registry.emplace<UnregisteredGadget>(entity);
  world.Registry.remove<UnregisteredGadget>(entity);
  REQUIRE_NOTHROW(validateWorld(world));
}

TEST_CASE("validateWorld refuses an entity not created by the world") {
  World world = buildWorld(makeSchema());
  static_cast<void>(world.Registry.create());
  requireRefused(world);
}

TEST_CASE("validateWorld refuses a live key whose entity was destroyed on the registry") {
  World world = buildWorld(makeSchema());
  world.Registry.destroy(world.findEntity(SECOND));
  requireRefused(world);
}

// A registered double is reached directly, as a vector element, or through a nested struct, and a
// NaN hides in any of them.
TEST_CASE("validateWorld refuses a NaN in a registered double, naming the component, field, and "
          "key") {
  World world = buildWorld(makeSchema());

  SECTION("a top-level field") {
    const EntityKey key = addDerivedProbe(world, 3);
    componentOf<Probe>(world, key).Level = NOT_A_NUMBER;
    const std::string message = requireRefused(world);
    CHECK_THAT(message, ContainsSubstring("probe"));
    CHECK_THAT(message, ContainsSubstring("level"));
    CHECK_THAT(message, ContainsSubstring(decimal(key)));
  }
  SECTION("a vector element") {
    const EntityKey key = addDerivedProbe(world, 4);
    componentOf<Probe>(world, key).Samples[1] = NOT_A_NUMBER;
    const std::string message = requireRefused(world);
    CHECK_THAT(message, ContainsSubstring("probe"));
    CHECK_THAT(message, ContainsSubstring("samples"));
    CHECK_THAT(message, ContainsSubstring(decimal(key)));
  }
  SECTION("a nested struct's field") {
    const EntityKey key = addDerivedProbe(world, 5);
    componentOf<Probe>(world, key).Place.Z = NOT_A_NUMBER;
    const std::string message = requireRefused(world);
    CHECK_THAT(message, ContainsSubstring("probe"));
    CHECK_THAT(message, ContainsSubstring(decimal(key)));
  }
}

// Only NaN is refused: infinities and negative zero are ordinary doubles.
TEST_CASE("validateWorld accepts every world the walk can cover") {
  World world = buildWorld(makeSchema());

  SECTION("a world of registered components and keyed entities") {
    REQUIRE_NOTHROW(validateWorld(world));
  }
  SECTION("doubles that are infinite or negative zero") {
    componentOf<Probe>(world, FIRST).Level = std::numeric_limits<double>::infinity();
    componentOf<Probe>(world, SECOND).Level = -std::numeric_limits<double>::infinity();
    componentOf<Probe>(world, FIRST).Place.X = -0.0;
    REQUIRE_NOTHROW(validateWorld(world));
  }
}

// An unregistered component is invisible to the walk, so without the check a copy would drop it
// silently and the world would compare and hash equal to one without it.
TEST_CASE("in debug builds, copyWorld, worldsEqual, and hashWorld run validateWorld first") {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
  const World bad = worldWithUnregisteredComponent();
  const World good = buildWorld(makeSchema());
  const std::string expected = requireRefused(bad);

  std::function<void()> walk;
  SECTION("copyWorld") {
    walk = [&] { static_cast<void>(copyWorld(bad)); };
  }
  SECTION("hashWorld") {
    walk = [&] { static_cast<void>(hashWorld(bad)); };
  }
  SECTION("worldsEqual, invalid on the left") {
    walk = [&] { static_cast<void>(worldsEqual(bad, good)); };
  }
  SECTION("worldsEqual, invalid on the right") {
    walk = [&] { static_cast<void>(worldsEqual(good, bad)); };
  }
  REQUIRE(refusal(walk) == expected);
}

} // namespace
} // namespace tpj
