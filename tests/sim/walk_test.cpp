#include "support/synthetic_types.h"

#include "sim/entity_key.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::buildWorld;
using test::Cached;
using test::componentOf;
using test::makeSchema;
using test::Mood;
using test::Probe;
using test::SLOT_PURPOSE;
using test::Tag;

// Keys that buildWorld gives its entities.
constexpr EntityKey FIRST{1};
constexpr EntityKey SECOND{2};
constexpr EntityKey BARE{3};
const EntityKey DERIVED = deriveKey(FIRST, SLOT_PURPOSE, 0);

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

TEST_CASE("a copy equals its original, hashes the same, and keeps its keys") {
  const World original = buildWorld(makeSchema());
  const World copy = copyWorld(original);
  requireSameValue(copy, original);
  REQUIRE(copy.keys() == original.keys());
  REQUIRE(copy.nextKey() == original.nextKey());
}

TEST_CASE("a copy and its original are independent") {
  const auto schema = makeSchema();
  const World reference = buildWorld(schema);
  World original = buildWorld(schema);
  World copy = copyWorld(original);

  // Each change reaches into state the copy duplicated: a vector field, the key counter, and the
  // live keys.
  auto change = [](World &world) {
    componentOf<Probe>(world, FIRST).Samples[0] = 8.0;
    world.createEntity();
    world.destroyEntity(SECOND);
  };

  SECTION("changing the copy leaves the original unchanged") {
    const uint64_t originalHash = hashWorld(original);
    change(copy);
    requireSameValue(original, reference);
    REQUIRE(hashWorld(original) == originalHash);
    REQUIRE_FALSE(worldsEqual(copy, original));
  }
  SECTION("changing the original leaves the copy unchanged") {
    const uint64_t copyHash = hashWorld(copy);
    change(original);
    requireSameValue(copy, reference);
    REQUIRE(hashWorld(copy) == copyHash);
    REQUIRE_FALSE(worldsEqual(copy, original));
  }
}

TEST_CASE("worlds built by the same calls are equal and hash equal, even on separate schemas") {
  const auto schema = makeSchema();
  requireSameValue(buildWorld(schema), buildWorld(schema));
  requireSameValue(buildWorld(makeSchema()), buildWorld(makeSchema()));
}

TEST_CASE("a change to any one thing the walk covers makes worlds unequal and changes the hash") {
  const std::vector<std::pair<std::string, std::function<void(World &)>>> changes = {
      {"tick", [](World &world) { world.Tick += 1; }},
      {"seed", [](World &world) { world.Seed += 1; }},
      {"key counter", [](World &world) { world.destroyEntity(world.createEntity()); }},
      {"a live key without components removed", [](World &world) { world.destroyEntity(BARE); }},
      {"a live derived key added",
       [](World &world) { world.createDerivedEntity(FIRST, SLOT_PURPOSE, 1); }},
      {"an empty component added",
       [](World &world) { world.Registry.emplace<Tag>(world.findEntity(BARE)); }},
      {"an empty component removed",
       [](World &world) { world.Registry.remove<Tag>(world.findEntity(SECOND)); }},
      {"a derived component removed",
       [](World &world) { world.Registry.remove<Cached>(world.findEntity(DERIVED)); }},
      {"bool field", [](World &world) { componentOf<Probe>(world, FIRST).Flag = false; }},
      {"signed field", [](World &world) { componentOf<Probe>(world, FIRST).Count = 7; }},
      {"unsigned field", [](World &world) { componentOf<Probe>(world, FIRST).Small = 10; }},
      {"double field", [](World &world) { componentOf<Probe>(world, FIRST).Level = 1.25; }},
      // Equal as doubles, different as bits.
      {"double field, signed zero",
       [](World &world) { componentOf<Probe>(world, SECOND).Level = -0.0; }},
      {"enum field", [](World &world) { componentOf<Probe>(world, FIRST).Feeling = Mood::Calm; }},
      {"key field", [](World &world) { componentOf<Probe>(world, FIRST).Target = BARE; }},
      {"vector element", [](World &world) { componentOf<Probe>(world, FIRST).Samples[1] = 3.0; }},
      {"vector length",
       [](World &world) { componentOf<Probe>(world, SECOND).Samples.push_back(0.0); }},
      {"nested struct field", [](World &world) { componentOf<Probe>(world, FIRST).Place.Z = 2.0; }},
      {"derived component field",
       [](World &world) { componentOf<Cached>(world, DERIVED).Total = 43; }},
  };

  const auto schema = makeSchema();
  const World reference = buildWorld(schema);
  const uint64_t referenceHash = hashWorld(reference);
  for (const auto &[label, apply] : changes) {
    CAPTURE(label);
    World changed = buildWorld(schema);
    apply(changed);
    CHECK_FALSE(worldsEqual(changed, reference));
    CHECK_FALSE(worldsEqual(reference, changed));
    CHECK(hashWorld(changed) != referenceHash);
  }
}

TEST_CASE("worlds on different schemas are never equal") {
  auto reordered = std::make_shared<WorldSchema>();
  reordered->addComponent<Tag>("tag", DataKind::Intent);
  reordered->addComponent<Probe>("probe", DataKind::State);
  reordered->addComponent<Cached>("cached", DataKind::Derived);

  auto rekinded = std::make_shared<WorldSchema>();
  rekinded->addComponent<Probe>("probe", DataKind::State);
  rekinded->addComponent<Tag>("tag", DataKind::State);
  rekinded->addComponent<Cached>("cached", DataKind::Derived);

  auto renamed = std::make_shared<WorldSchema>();
  renamed->addComponent<Probe>("probe", DataKind::State);
  renamed->addComponent<Tag>("marker", DataKind::Intent);
  renamed->addComponent<Cached>("cached", DataKind::Derived);

  // Empty worlds, so that nothing but the schema differs.
  const World base(makeSchema(), 5);
  for (const std::shared_ptr<const WorldSchema> &other :
       std::vector<std::shared_ptr<const WorldSchema>>{reordered, rekinded, renamed}) {
    const World different(other, 5);
    CHECK_FALSE(worldsEqual(base, different));
    CHECK_FALSE(worldsEqual(different, base));
  }
}

} // namespace
} // namespace tpj
