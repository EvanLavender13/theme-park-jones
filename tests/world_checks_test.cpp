#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"
#include "synthetic_world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <entt/entity/registry.hpp>

#include <bit>
#include <functional>
#include <limits>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;

struct Spot {
  double Height = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Spot &spot) {
  visitor.field("height", spot.Height);
}

struct Notch {
  int32_t Rank = 0;
  double Depth = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Notch &notch) {
  visitor.field("rank", notch.Rank);
  visitor.field("depth", notch.Depth);
}

struct Gauge {
  double Level = 0.0;
  std::vector<double> Readings;
  Spot Anchor;
  std::vector<Notch> Notches;
};

template <typename Visitor> void visitFields(Visitor &visitor, Gauge &gauge) {
  visitor.field("level", gauge.Level);
  visitor.field("readings", gauge.Readings);
  visitor.field("anchor", gauge.Anchor);
  visitor.field("notches", gauge.Notches);
}

struct Pin {};

// Never registered.
struct UnregisteredProbe {
  int32_t Value = 0;
};

// Never registered, and a tag.
struct UnregisteredMarkProbe {};

std::shared_ptr<const WorldSchema> makeCheckSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Gauge>("gauge", DataKind::State);
  schema->addComponent<Pin>("pin", DataKind::Intent);
  return schema;
}

// Keys 1 to 30 and one derived key, with valid Gauges on keys 1 and 23 and a Pin on key 2.
World buildValidWorld() {
  World world(makeCheckSchema(), 11);
  for (int i = 0; i < 30; ++i) {
    world.createEntity();
  }
  world.createDerivedEntity(EntityKey{1}, hashName("gauge"), 5);
  Gauge gauge;
  gauge.Readings = {1.0, 2.0, 3.0};
  gauge.Notches = {Notch{1, 0.5}, Notch{2, 0.25}};
  world.Registry.emplace<Gauge>(synthetic::requireEntity(world, EntityKey{1}), gauge);
  world.Registry.emplace<Gauge>(synthetic::requireEntity(world, EntityKey{23}), gauge);
  world.Registry.emplace<Pin>(synthetic::requireEntity(world, EntityKey{2}));
  return world;
}

// Runs the call and returns WorldInvariantError's message, or fails the test if it did not throw.
std::string invariantMessage(const std::function<void()> &call) {
  try {
    call();
  } catch (const WorldInvariantError &error) {
    return error.what();
  }
  FAIL("expected WorldInvariantError, but nothing was thrown");
  return {};
}

// Every public walk that must refuse an invalid world when WORLD_CHECKS is true, and
// validateWorld itself, which refuses it in any build.
enum class Walk { Validate, Copy, EqualLeft, EqualRight, Hash };

void runWalk(Walk walk, const World &invalid) {
  switch (walk) {
  case Walk::Validate:
    validateWorld(invalid);
    break;
  case Walk::Copy:
    static_cast<void>(copyWorld(invalid));
    break;
  case Walk::EqualLeft:
    static_cast<void>(worldsEqual(invalid, buildValidWorld()));
    break;
  case Walk::EqualRight:
    static_cast<void>(worldsEqual(buildValidWorld(), invalid));
    break;
  case Walk::Hash:
    static_cast<void>(hashWorld(invalid));
    break;
  }
}

Walk generateWalk() {
  if constexpr (WORLD_CHECKS) {
    return GENERATE(Walk::Validate, Walk::Copy, Walk::EqualLeft, Walk::EqualRight, Walk::Hash);
  } else {
    return Walk::Validate;
  }
}

#ifdef NDEBUG
constexpr bool NDEBUG_DEFINED = true;
#else
constexpr bool NDEBUG_DEFINED = false;
#endif

TEST_CASE("WORLD_CHECKS is true in builds without NDEBUG") {
  STATIC_REQUIRE((NDEBUG_DEFINED || WORLD_CHECKS));
}

// A valid world passes, so the failures below come from the planted fault.
TEST_CASE("validateWorld accepts a valid world") {
  const World world = buildValidWorld();
  CHECK_NOTHROW(validateWorld(world));
  CHECK_NOTHROW(hashWorld(world));
  CHECK_NOTHROW(copyWorld(world));
}

TEST_CASE("a component of an unregistered type is refused with the type's name") {
  const Walk walk = generateWalk();
  CAPTURE(walk);
  World world = buildValidWorld();
  world.Registry.emplace<UnregisteredProbe>(synthetic::requireEntity(world, EntityKey{4}),
                                            UnregisteredProbe{9});
  CHECK_THAT(invariantMessage([&] { runWalk(walk, world); }),
             ContainsSubstring("UnregisteredProbe"));
}

TEST_CASE("an unregistered tag component is refused with the type's name") {
  const Walk walk = generateWalk();
  CAPTURE(walk);
  World world = buildValidWorld();
  world.Registry.emplace<UnregisteredMarkProbe>(synthetic::requireEntity(world, EntityKey{1}));
  CHECK_THAT(invariantMessage([&] { runWalk(walk, world); }),
             ContainsSubstring("UnregisteredMarkProbe"));
}

TEST_CASE("an unregistered type whose storage exists but is empty is not an error") {
  World world = buildValidWorld();
  const entt::entity entity = synthetic::requireEntity(world, EntityKey{4});
  world.Registry.emplace<UnregisteredProbe>(entity, UnregisteredProbe{9});
  world.Registry.remove<UnregisteredProbe>(entity);
  static_cast<void>(world.Registry.view<UnregisteredMarkProbe>());
  static_cast<void>(world.Registry.storage<UnregisteredMarkProbe>());

  CHECK_NOTHROW(validateWorld(world));
  CHECK_NOTHROW(hashWorld(world));
  const World copy = copyWorld(world);
  CHECK(worldsEqual(copy, world));
  CHECK(worldsEqual(world, buildValidWorld()));
  CHECK(hashWorld(world) == hashWorld(buildValidWorld()));
}

TEST_CASE("an entity the world did not create is refused") {
  const Walk walk = generateWalk();
  CAPTURE(walk);
  const bool withComponent = GENERATE(false, true);
  CAPTURE(withComponent);
  World world = buildValidWorld();
  const entt::entity stranger = world.Registry.create();
  if (withComponent) {
    world.Registry.emplace<Pin>(stranger);
  }
  CHECK_THROWS_AS(runWalk(walk, world), WorldInvariantError);
}

TEST_CASE("a live key whose entity was destroyed through the registry is refused") {
  const Walk walk = generateWalk();
  CAPTURE(walk);
  const bool derived = GENERATE(false, true);
  CAPTURE(derived);
  World world = buildValidWorld();
  const EntityKey key = derived ? deriveKey(EntityKey{1}, hashName("gauge"), 5) : EntityKey{7};
  world.Registry.destroy(synthetic::requireEntity(world, key));
  CHECK_THROWS_AS(runWalk(walk, world), WorldInvariantError);
}

enum class NanPlace { Scalar, VectorElement, NestedStruct, StructInVector };

const char *fieldNameAt(NanPlace place) {
  switch (place) {
  case NanPlace::Scalar:
    return "level";
  case NanPlace::VectorElement:
    return "readings";
  case NanPlace::NestedStruct:
    return "height";
  case NanPlace::StructInVector:
    return "depth";
  }
  return "";
}

void plantNan(Gauge &gauge, NanPlace place, double nan) {
  switch (place) {
  case NanPlace::Scalar:
    gauge.Level = nan;
    break;
  case NanPlace::VectorElement:
    gauge.Readings[1] = nan;
    break;
  case NanPlace::NestedStruct:
    gauge.Anchor.Height = nan;
    break;
  case NanPlace::StructInVector:
    gauge.Notches[1].Depth = nan;
    break;
  }
}

TEST_CASE("NaN in a registered double is refused with component, field, and key") {
  const Walk walk = generateWalk();
  CAPTURE(walk);
  const NanPlace place = GENERATE(NanPlace::Scalar, NanPlace::VectorElement, NanPlace::NestedStruct,
                                  NanPlace::StructInVector);
  CAPTURE(place);
  const double nan =
      GENERATE(std::numeric_limits<double>::quiet_NaN(), -std::numeric_limits<double>::quiet_NaN(),
               std::bit_cast<double>(uint64_t{0x7ff0000000000001ULL}));
  World world = buildValidWorld();
  plantNan(world.Registry.get<Gauge>(synthetic::requireEntity(world, EntityKey{23})), place, nan);

  const std::string message = invariantMessage([&] { runWalk(walk, world); });
  CHECK_THAT(message, ContainsSubstring("gauge"));
  CHECK_THAT(message, ContainsSubstring(fieldNameAt(place)));
  CHECK_THAT(message, ContainsSubstring("23"));
}

TEST_CASE("NaN on a derived entity is refused with its key in decimal") {
  World world = buildValidWorld();
  const EntityKey derived = deriveKey(EntityKey{1}, hashName("gauge"), 5);
  Gauge gauge;
  gauge.Readings = {std::numeric_limits<double>::quiet_NaN()};
  world.Registry.emplace<Gauge>(synthetic::requireEntity(world, derived), gauge);

  const std::string message = invariantMessage([&] { validateWorld(world); });
  CHECK_THAT(message, ContainsSubstring("gauge"));
  CHECK_THAT(message, ContainsSubstring("readings"));
  CHECK_THAT(message, ContainsSubstring(std::to_string(static_cast<uint64_t>(derived))));
}

// Only NaN is refused: infinities are legitimate values.
TEST_CASE("infinite doubles are not refused") {
  World world = buildValidWorld();
  Gauge &gauge = world.Registry.get<Gauge>(synthetic::requireEntity(world, EntityKey{23}));
  gauge.Level = std::numeric_limits<double>::infinity();
  gauge.Readings[0] = -std::numeric_limits<double>::infinity();
  CHECK_NOTHROW(validateWorld(world));
  CHECK_NOTHROW(hashWorld(world));
}

} // namespace
} // namespace tpj
