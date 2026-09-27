#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"
#include "synthetic_world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <entt/entity/registry.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <vector>

namespace tpj {
namespace {

using synthetic::Deep;
using synthetic::forEachSyntheticType;
using synthetic::Grade;
using synthetic::Link;
using synthetic::Marker;
using synthetic::Mood;
using synthetic::Numbers;
using synthetic::Point;
using synthetic::Series;
using synthetic::snapshotWorld;
using synthetic::WorldSnapshot;

std::shared_ptr<const WorldSchema> sharedSchema() {
  static const std::shared_ptr<const WorldSchema> SCHEMA = synthetic::makeSyntheticSchema();
  return SCHEMA;
}

constexpr EntityKey keyOfValue(uint64_t value) { return EntityKey{value}; }

// A small hand-built world: keys 1 to 4 and one derived key. Key 1 holds Numbers, Series, and
// Deep, key 2 holds Marker, key 3 holds a Link to key 1, and key 4 and the derived key hold
// nothing.
World buildBaseWorld(const std::shared_ptr<const WorldSchema> &schema) {
  World world(schema, 7);
  world.Tick = 3;
  const EntityKey one = world.createEntity();
  const EntityKey two = world.createEntity();
  const EntityKey three = world.createEntity();
  world.createEntity();
  world.createDerivedEntity(one, hashName("base"), 0);

  Numbers numbers;
  numbers.IntSigned = 5;
  numbers.Real = 0.0;
  numbers.LongLongUnsigned = 123456789;
  world.Registry.emplace<Numbers>(synthetic::requireEntity(world, one), numbers);

  Series series;
  series.Samples = {1.0, 2.5};
  series.Targets = {three};
  series.Path = {Point{1.0, 2.0, 3.0}};
  world.Registry.emplace<Series>(synthetic::requireEntity(world, one), series);

  Deep deep;
  deep.Depth = 2;
  deep.Body.Head.At = Point{4.0, 5.0, 6.0};
  deep.Body.Rest.resize(2);
  deep.Body.Rest[1].Weights = {0.25};
  world.Registry.emplace<Deep>(synthetic::requireEntity(world, one), deep);

  world.Registry.emplace<Marker>(synthetic::requireEntity(world, two));
  world.Registry.emplace<Link>(synthetic::requireEntity(world, three),
                               Link{one, Mood::Happy, Grade::High, Point{}});
  return world;
}

entt::entity entityOf(const World &world, uint64_t key) {
  const entt::entity entity = world.findEntity(keyOfValue(key));
  REQUIRE(entity != entt::null);
  return entity;
}

// The component T on a live key, failing the test instead of letting EnTT abort when it is missing.
template <typename T> T &componentOf(World &world, uint64_t key) {
  T *component = world.Registry.try_get<T>(entityOf(world, key));
  REQUIRE(component != nullptr);
  return *component;
}

TEST_CASE("a copy of a randomized world equals the original and has the same hash") {
  const auto seed = GENERATE(range<uint64_t>(1, 21));
  CAPTURE(seed);
  const World original = synthetic::buildRandomWorld(sharedSchema(), seed);
  const World copy = copyWorld(original);
  CHECK(worldsEqual(copy, original));
  CHECK(worldsEqual(original, copy));
  CHECK(hashWorld(copy) == hashWorld(original));
}

TEST_CASE("a copy has the same tick, seed, next key, keys, and components") {
  const auto seed = GENERATE(range<uint64_t>(1, 21));
  CAPTURE(seed);
  const World original = synthetic::buildRandomWorld(sharedSchema(), seed);
  const World copy = copyWorld(original);
  CHECK(copy.Tick == original.Tick);
  CHECK(copy.Seed == original.Seed);
  CHECK(copy.nextKey() == original.nextKey());
  CHECK(copy.keys() == original.keys());
  CHECK(snapshotWorld(copy) == snapshotWorld(original));
}

TEST_CASE("an EntityKey field in a copy finds the entity with that key through the copy") {
  size_t referencesChecked = 0;
  for (uint64_t seed = 1; seed <= 20; ++seed) {
    CAPTURE(seed);
    const World original = synthetic::buildRandomWorld(sharedSchema(), seed);
    const World copy = copyWorld(original);
    for (const EntityKey key : copy.keys()) {
      const Link *link = copy.Registry.try_get<Link>(synthetic::requireEntity(copy, key));
      if (link == nullptr || link->Target == NULL_KEY) {
        continue;
      }
      const entt::entity target = copy.findEntity(link->Target);
      REQUIRE(target != entt::null);
      CHECK(copy.Registry.valid(target));
      CHECK(copy.keyOf(target) == link->Target);
      ++referencesChecked;
    }
  }
  CHECK(referencesChecked > 0);
}

enum class Change {
  ScalarField,
  VectorField,
  NestedField,
  EntityKeyField,
  AddComponent,
  RemoveComponent,
  AddTag,
  RemoveTag,
  CreateEntity,
  DestroyEntity,
  ChangeTick,
  ChangeSeed,
};

void applyChange(World &world, Change change) {
  switch (change) {
  case Change::ScalarField:
    componentOf<Numbers>(world, 1).Real = 9.5;
    break;
  case Change::VectorField:
    componentOf<Series>(world, 1).Samples.push_back(4.0);
    componentOf<Series>(world, 1).Samples.at(0) = -1.0;
    break;
  case Change::NestedField:
    componentOf<Deep>(world, 1).Body.Rest.at(1).Weights.at(0) = 0.75;
    break;
  case Change::EntityKeyField:
    componentOf<Link>(world, 3).Target = keyOfValue(2);
    break;
  case Change::AddComponent:
    world.Registry.emplace<Numbers>(entityOf(world, 4));
    break;
  case Change::RemoveComponent:
    world.Registry.remove<Numbers>(entityOf(world, 1));
    break;
  case Change::AddTag:
    world.Registry.emplace<Marker>(entityOf(world, 3));
    break;
  case Change::RemoveTag:
    world.Registry.remove<Marker>(entityOf(world, 2));
    break;
  case Change::CreateEntity:
    world.createEntity();
    break;
  case Change::DestroyEntity:
    world.destroyEntity(keyOfValue(3));
    break;
  case Change::ChangeTick:
    world.Tick += 1;
    break;
  case Change::ChangeSeed:
    world.Seed += 1;
    break;
  }
}

TEST_CASE("changing a copy leaves the original unchanged") {
  const Change change = GENERATE(
      Change::ScalarField, Change::VectorField, Change::NestedField, Change::EntityKeyField,
      Change::AddComponent, Change::RemoveComponent, Change::AddTag, Change::RemoveTag,
      Change::CreateEntity, Change::DestroyEntity, Change::ChangeTick, Change::ChangeSeed);
  CAPTURE(change);
  const World original = buildBaseWorld(sharedSchema());
  const uint64_t hashBefore = hashWorld(original);
  const WorldSnapshot before = snapshotWorld(original);

  World copy = copyWorld(original);
  applyChange(copy, change);

  CHECK(hashWorld(original) == hashBefore);
  CHECK(snapshotWorld(original) == before);
  CHECK(snapshotWorld(copy) != before);
}

TEST_CASE("changing the original leaves a copy unchanged") {
  const Change change = GENERATE(
      Change::ScalarField, Change::VectorField, Change::NestedField, Change::EntityKeyField,
      Change::AddComponent, Change::RemoveComponent, Change::AddTag, Change::RemoveTag,
      Change::CreateEntity, Change::DestroyEntity, Change::ChangeTick, Change::ChangeSeed);
  CAPTURE(change);
  World original = buildBaseWorld(sharedSchema());
  const World copy = copyWorld(original);
  const uint64_t hashBefore = hashWorld(copy);
  const WorldSnapshot before = snapshotWorld(copy);

  applyChange(original, change);

  CHECK(hashWorld(copy) == hashBefore);
  CHECK(snapshotWorld(copy) == before);
  CHECK(snapshotWorld(original) != before);
}

enum class Difference {
  Tick,
  Seed,
  NextKey,
  LiveKeysDestroyed,
  LiveKeysDerived,
  ComponentHolder,
  TagHolder,
  FieldValue,
  NegativeZero,
  EntityKeyField,
};

void applyDifference(World &world, Difference difference) {
  switch (difference) {
  case Difference::Tick:
    world.Tick = 4;
    break;
  case Difference::Seed:
    world.Seed = 8;
    break;
  case Difference::NextKey:
    world.destroyEntity(world.createEntity());
    break;
  case Difference::LiveKeysDestroyed:
    world.destroyEntity(keyOfValue(4));
    break;
  case Difference::LiveKeysDerived:
    world.createDerivedEntity(keyOfValue(2), hashName("other"), 1);
    break;
  case Difference::ComponentHolder: {
    const Link link = componentOf<Link>(world, 3);
    world.Registry.remove<Link>(entityOf(world, 3));
    world.Registry.emplace<Link>(entityOf(world, 4), link);
    break;
  }
  case Difference::TagHolder:
    world.Registry.remove<Marker>(entityOf(world, 2));
    world.Registry.emplace<Marker>(entityOf(world, 4));
    break;
  case Difference::FieldValue:
    componentOf<Numbers>(world, 1).IntSigned = 6;
    break;
  case Difference::NegativeZero:
    componentOf<Numbers>(world, 1).Real = -0.0;
    break;
  case Difference::EntityKeyField:
    componentOf<Link>(world, 3).Target = keyOfValue(2);
    break;
  }
}

TEST_CASE("two worlds built by the same calls are equal") {
  const World built = buildBaseWorld(sharedSchema());
  const World rebuilt = buildBaseWorld(sharedSchema());
  CHECK(worldsEqual(built, rebuilt));
  CHECK(worldsEqual(rebuilt, built));
  CHECK(worldsEqual(built, built));
  CHECK(hashWorld(built) == hashWorld(rebuilt));
}

TEST_CASE("worldsEqual is false when two worlds differ in any one thing") {
  const Difference difference = GENERATE(
      Difference::Tick, Difference::Seed, Difference::NextKey, Difference::LiveKeysDestroyed,
      Difference::LiveKeysDerived, Difference::ComponentHolder, Difference::TagHolder,
      Difference::FieldValue, Difference::NegativeZero, Difference::EntityKeyField);
  CAPTURE(difference);
  const World built = buildBaseWorld(sharedSchema());
  World rebuilt = buildBaseWorld(sharedSchema());
  applyDifference(rebuilt, difference);
  CHECK_FALSE(worldsEqual(built, rebuilt));
  CHECK_FALSE(worldsEqual(rebuilt, built));
  CHECK(hashWorld(built) != hashWorld(rebuilt));
}

// Doubles compare by their bits, so the sign of zero counts.
TEST_CASE("worldsEqual tells 0.0 from -0.0") {
  World positive(sharedSchema(), 0);
  World negative(sharedSchema(), 0);
  Numbers numbers;
  numbers.Real = 0.0;
  positive.Registry.emplace<Numbers>(synthetic::requireEntity(positive, positive.createEntity()),
                                     numbers);
  numbers.Real = -0.0;
  negative.Registry.emplace<Numbers>(synthetic::requireEntity(negative, negative.createEntity()),
                                     numbers);
  CHECK_FALSE(worldsEqual(positive, negative));
  CHECK(hashWorld(positive) != hashWorld(negative));
}

TEST_CASE("worlds on separately built but identical schemas are equal") {
  const auto seed = GENERATE(range<uint64_t>(1, 11));
  CAPTURE(seed);
  const World built = synthetic::buildRandomWorld(synthetic::makeSyntheticSchema(), seed);
  const World rebuilt = synthetic::buildRandomWorld(synthetic::makeSyntheticSchema(), seed);
  CHECK(worldsEqual(built, rebuilt));
  CHECK(hashWorld(built) == hashWorld(rebuilt));
}

struct Alpha {
  int32_t A = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Alpha &alpha) {
  visitor.field("a", alpha.A);
}

struct Beta {
  int32_t B = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Beta &beta) {
  visitor.field("b", beta.B);
}

// The same layout and field names as Beta, but a different type.
struct BetaTwin {
  int32_t B = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, BetaTwin &beta) {
  visitor.field("b", beta.B);
}

struct Gamma {};

enum class SchemaVariant { Base, Renamed, Rekinded, Reordered, Missing, Extra, OtherType };

std::shared_ptr<const WorldSchema> makeVariantSchema(SchemaVariant variant) {
  auto schema = std::make_shared<WorldSchema>();
  switch (variant) {
  case SchemaVariant::Base:
    schema->addComponent<Alpha>("alpha", DataKind::State);
    schema->addComponent<Beta>("beta", DataKind::State);
    break;
  case SchemaVariant::Renamed:
    schema->addComponent<Alpha>("alpha-2", DataKind::State);
    schema->addComponent<Beta>("beta", DataKind::State);
    break;
  case SchemaVariant::Rekinded:
    schema->addComponent<Alpha>("alpha", DataKind::State);
    schema->addComponent<Beta>("beta", DataKind::Intent);
    break;
  case SchemaVariant::Reordered:
    schema->addComponent<Beta>("beta", DataKind::State);
    schema->addComponent<Alpha>("alpha", DataKind::State);
    break;
  case SchemaVariant::Missing:
    schema->addComponent<Alpha>("alpha", DataKind::State);
    break;
  case SchemaVariant::Extra:
    schema->addComponent<Alpha>("alpha", DataKind::State);
    schema->addComponent<Beta>("beta", DataKind::State);
    schema->addComponent<Gamma>("gamma", DataKind::State);
    break;
  case SchemaVariant::OtherType:
    schema->addComponent<Alpha>("alpha", DataKind::State);
    schema->addComponent<BetaTwin>("beta", DataKind::State);
    break;
  }
  return schema;
}

TEST_CASE("worlds whose schemas differ are never equal") {
  const SchemaVariant variant =
      GENERATE(SchemaVariant::Renamed, SchemaVariant::Rekinded, SchemaVariant::Reordered,
               SchemaVariant::Missing, SchemaVariant::Extra, SchemaVariant::OtherType);
  CAPTURE(variant);
  World base(makeVariantSchema(SchemaVariant::Base), 1);
  World other(makeVariantSchema(variant), 1);
  base.createEntity();
  other.createEntity();
  const World sameAsBase = [] {
    World world(makeVariantSchema(SchemaVariant::Base), 1);
    world.createEntity();
    return world;
  }();
  REQUIRE(worldsEqual(base, sameAsBase));
  CHECK_FALSE(worldsEqual(base, other));
  CHECK_FALSE(worldsEqual(other, base));
}

// A description of each single-value change that failed to change the hash or equality.
using Failures = std::vector<std::string>;

template <typename T>
void mutateEverySite(const std::shared_ptr<const WorldSchema> &schema, uint64_t seed, EntityKey key,
                     const World &reference, Failures &failures, size_t &sitesChecked,
                     bool &zeroSignSeen) {
  const uint64_t referenceHash = hashWorld(reference);
  for (size_t site = 0;; ++site) {
    World mutated = synthetic::buildRandomWorld(schema, seed);
    synthetic::FieldMutator mutator(site);
    visitFields(mutator, mutated.Registry.get<T>(synthetic::requireEntity(mutated, key)));
    if (!mutator.mutated()) {
      return;
    }
    ++sitesChecked;
    zeroSignSeen = zeroSignSeen || mutator.mutatedZeroSign();
    if (hashWorld(mutated) == referenceHash || worldsEqual(mutated, reference)) {
      failures.push_back("seed " + std::to_string(seed) + " key " +
                         std::to_string(static_cast<uint64_t>(key)) + " type " +
                         std::string(entt::type_id<T>().name()) + " site " + std::to_string(site));
    }
  }
}

TEST_CASE("changing any single registered value changes the hash") {
  const auto schema = sharedSchema();
  Failures failures;
  size_t sitesChecked = 0;
  bool zeroSignSeen = false;
  std::vector<size_t> typesMutated(synthetic::SYNTHETIC_NAMES.size(), 0);
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    const World reference = synthetic::buildRandomWorld(schema, seed);
    for (const EntityKey key : reference.keys()) {
      const entt::entity entity = synthetic::requireEntity(reference, key);
      size_t typeIndex = 0;
      forEachSyntheticType([&](auto type) {
        using T = typename decltype(type)::type;
        if constexpr (!std::is_empty_v<T>) {
          if (reference.Registry.all_of<T>(entity)) {
            const size_t before = sitesChecked;
            mutateEverySite<T>(schema, seed, key, reference, failures, sitesChecked, zeroSignSeen);
            typesMutated[typeIndex] += sitesChecked - before;
          }
        }
        ++typeIndex;
      });
    }
  }
  CAPTURE(failures);
  CHECK(failures.empty());
  CHECK(zeroSignSeen);
  for (size_t i = 0; i + 1 < typesMutated.size(); ++i) {
    CAPTURE(synthetic::SYNTHETIC_NAMES[i]);
    CHECK(typesMutated[i] > 0);
  }
}

TEST_CASE("adding or removing a component changes the hash") {
  const auto schema = sharedSchema();
  Failures failures;
  size_t togglesChecked = 0;
  for (uint64_t seed = 1; seed <= 8; ++seed) {
    const World reference = synthetic::buildRandomWorld(schema, seed);
    const uint64_t referenceHash = hashWorld(reference);
    for (const EntityKey key : reference.keys()) {
      forEachSyntheticType([&](auto type) {
        using T = typename decltype(type)::type;
        World changed = synthetic::buildRandomWorld(schema, seed);
        const entt::entity entity = synthetic::requireEntity(changed, key);
        if (changed.Registry.all_of<T>(entity)) {
          changed.Registry.remove<T>(entity);
        } else {
          changed.Registry.emplace<T>(entity);
        }
        ++togglesChecked;
        if (hashWorld(changed) == referenceHash || worldsEqual(changed, reference)) {
          failures.push_back("seed " + std::to_string(seed) + " key " +
                             std::to_string(static_cast<uint64_t>(key)) + " type " +
                             std::string(entt::type_id<T>().name()));
        }
      });
    }
  }
  CAPTURE(failures);
  CHECK(failures.empty());
  CHECK(togglesChecked > 0);
}

TEST_CASE("changing tick, seed, or next key changes the hash") {
  const auto seed = GENERATE(range<uint64_t>(1, 9));
  CAPTURE(seed);
  const World reference = synthetic::buildRandomWorld(sharedSchema(), seed);
  const uint64_t referenceHash = hashWorld(reference);

  World tick = synthetic::buildRandomWorld(sharedSchema(), seed);
  tick.Tick += 1;
  CHECK(hashWorld(tick) != referenceHash);

  World worldSeed = synthetic::buildRandomWorld(sharedSchema(), seed);
  worldSeed.Seed ^= 1U;
  CHECK(hashWorld(worldSeed) != referenceHash);

  World nextKey = synthetic::buildRandomWorld(sharedSchema(), seed);
  nextKey.destroyEntity(nextKey.createEntity());
  REQUIRE(nextKey.keys() == reference.keys());
  CHECK(hashWorld(nextKey) != referenceHash);
}

TEST_CASE("changing a double by its sign of zero or by one ulp changes the hash") {
  World world(sharedSchema(), 0);
  const entt::entity entity = synthetic::requireEntity(world, world.createEntity());
  world.Registry.emplace<Numbers>(entity);
  const uint64_t zeroHash = hashWorld(world);
  world.Registry.get<Numbers>(entity).Real = -0.0;
  const uint64_t negativeZeroHash = hashWorld(world);
  world.Registry.get<Numbers>(entity).Real = std::nextafter(0.0, 1.0);
  const uint64_t smallestHash = hashWorld(world);
  CHECK(zeroHash != negativeZeroHash);
  CHECK(zeroHash != smallestHash);
  CHECK(negativeZeroHash != smallestHash);
}

TEST_CASE("equality and hash ignore the order components were added in") {
  World forward(sharedSchema(), 5);
  World backward(sharedSchema(), 5);
  for (int i = 0; i < 4; ++i) {
    forward.createEntity();
    backward.createEntity();
  }
  const auto numbersFor = [](uint64_t key) {
    Numbers numbers;
    numbers.IntSigned = static_cast<int>(key) * 10;
    numbers.Real = static_cast<double>(key) / 4.0;
    return numbers;
  };
  for (const uint64_t key : {1, 2, 3}) {
    forward.Registry.emplace<Numbers>(entityOf(forward, key), numbersFor(key));
  }
  for (const uint64_t key : {1, 3}) {
    forward.Registry.emplace<Marker>(entityOf(forward, key));
  }
  for (const uint64_t key : {3, 1}) {
    backward.Registry.emplace<Marker>(entityOf(backward, key));
  }
  for (const uint64_t key : {3, 1, 2}) {
    backward.Registry.emplace<Numbers>(entityOf(backward, key), numbersFor(key));
  }
  CHECK(worldsEqual(forward, backward));
  CHECK(hashWorld(forward) == hashWorld(backward));

  // Removing and re-adding moves an entity to the end of EnTT's storage.
  forward.Registry.remove<Numbers>(entityOf(forward, 1));
  forward.Registry.emplace<Numbers>(entityOf(forward, 1), numbersFor(1));
  forward.Registry.remove<Marker>(entityOf(forward, 1));
  forward.Registry.emplace<Marker>(entityOf(forward, 1));
  CHECK(worldsEqual(forward, backward));
  CHECK(hashWorld(forward) == hashWorld(backward));

  // Sorting a storage reorders it without changing any value.
  backward.Registry.sort<Numbers>(
      [](const Numbers &lhs, const Numbers &rhs) { return lhs.IntSigned > rhs.IntSigned; });
  CHECK(worldsEqual(forward, backward));
  CHECK(hashWorld(forward) == hashWorld(backward));
}

// EnTT recycles its identifiers, so the walk identifies entities by key, never by entt::entity.
TEST_CASE("equality and hash ignore which EnTT entities hold the keys") {
  World plain(sharedSchema(), 2);
  World shuffled(sharedSchema(), 2);
  const EntityKey one = plain.createEntity();
  const EntityKey two = plain.createEntity();
  const EntityKey derived = plain.createDerivedEntity(one, hashName("p"), 0);

  // In shuffled, the derived entity is created before key 2, so the two take each other's EnTT
  // identifiers, and it is then destroyed and recreated so its identifier's version differs too.
  REQUIRE(shuffled.createEntity() == one);
  REQUIRE(shuffled.createDerivedEntity(one, hashName("p"), 0) == derived);
  REQUIRE(shuffled.createEntity() == two);
  REQUIRE(shuffled.destroyEntity(derived));
  REQUIRE(shuffled.createDerivedEntity(one, hashName("p"), 0) == derived);
  REQUIRE(plain.findEntity(two) != shuffled.findEntity(two));
  REQUIRE(plain.findEntity(derived) != shuffled.findEntity(derived));

  for (World *world : {&plain, &shuffled}) {
    world->Registry.emplace<Link>(synthetic::requireEntity(*world, two),
                                  Link{derived, Mood::Cross, Grade::Low, Point{1.0, 0.0, 0.0}});
    Series series;
    series.Targets = {one, derived, two};
    world->Registry.emplace<Series>(synthetic::requireEntity(*world, derived), series);
    world->Registry.emplace<Marker>(synthetic::requireEntity(*world, one));
  }
  CHECK(worldsEqual(plain, shuffled));
  CHECK(hashWorld(plain) == hashWorld(shuffled));
}

TEST_CASE("stepWorld advances tick by one per call") {
  World world = synthetic::buildRandomWorld(sharedSchema(), 3);
  const uint64_t start = world.Tick;
  for (uint64_t step = 1; step <= 10; ++step) {
    stepWorld(world);
    CHECK(world.Tick == start + step);
  }
  World empty;
  stepWorld(empty);
  CHECK(empty.Tick == 1);
}

TEST_CASE("two worlds built by the same calls hash equal after every one of 100 steps") {
  const auto seed = GENERATE(range<uint64_t>(1, 6));
  CAPTURE(seed);
  World built = synthetic::buildRandomWorld(sharedSchema(), seed);
  World rebuilt = synthetic::buildRandomWorld(synthetic::makeSyntheticSchema(), seed);
  size_t mismatches = 0;
  for (int step = 0; step < 100; ++step) {
    stepWorld(built);
    stepWorld(rebuilt);
    if (hashWorld(built) != hashWorld(rebuilt) || !worldsEqual(built, rebuilt)) {
      ++mismatches;
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE("a copy and its original hash equal after every one of 100 steps") {
  const auto seed = GENERATE(range<uint64_t>(1, 6));
  CAPTURE(seed);
  World original = synthetic::buildRandomWorld(sharedSchema(), seed);
  World copy = copyWorld(original);
  size_t mismatches = 0;
  for (int step = 0; step < 100; ++step) {
    stepWorld(original);
    stepWorld(copy);
    if (hashWorld(original) != hashWorld(copy) || !worldsEqual(original, copy)) {
      ++mismatches;
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE("an empty world is valid and copies, compares, and hashes") {
  const World empty(sharedSchema(), 9);
  CHECK_NOTHROW(validateWorld(empty));
  const World copy = copyWorld(empty);
  CHECK(worldsEqual(copy, empty));
  CHECK(hashWorld(copy) == hashWorld(empty));
  CHECK(copy.keys().empty());
  CHECK(copy.Seed == 9);

  const World bare;
  CHECK_NOTHROW(validateWorld(bare));
  const World bareCopy = copyWorld(bare);
  CHECK(worldsEqual(bareCopy, bare));
  CHECK(hashWorld(bareCopy) == hashWorld(bare));
}

TEST_CASE("entities without components are valid and copy, compare, and hash") {
  World world(sharedSchema(), 4);
  const EntityKey first = world.createEntity();
  world.createEntity();
  const EntityKey derived = world.createDerivedEntity(first, hashName("empty"), 2);
  CHECK_NOTHROW(validateWorld(world));

  const World copy = copyWorld(world);
  CHECK(copy.keys() == world.keys());
  CHECK(copy.findEntity(derived) != entt::null);
  CHECK(worldsEqual(copy, world));
  CHECK(hashWorld(copy) == hashWorld(world));

  World fewer = copyWorld(world);
  REQUIRE(fewer.destroyEntity(derived));
  CHECK_FALSE(worldsEqual(fewer, world));
  CHECK(hashWorld(fewer) != hashWorld(world));
}

// Derived data is never saved, but it is still part of the world's value.
TEST_CASE("a derived component is copied, compared, and hashed") {
  const std::shared_ptr<const WorldSchema> schema = sharedSchema();
  const ComponentType *series = schema->findComponent(entt::type_id<Series>().hash());
  REQUIRE(series != nullptr);
  REQUIRE(series->Kind == DataKind::Derived);

  const World original = buildBaseWorld(schema);
  World copy = copyWorld(original);
  const Series &copied = componentOf<Series>(copy, 1);
  CHECK(copied.Samples == std::vector<double>{1.0, 2.5});
  CHECK(copied.Targets == std::vector<EntityKey>{keyOfValue(3)});

  componentOf<Series>(copy, 1).Samples.at(1) = 2.75;
  CHECK_FALSE(worldsEqual(copy, original));
  CHECK(hashWorld(copy) != hashWorld(original));
}

} // namespace
} // namespace tpj
