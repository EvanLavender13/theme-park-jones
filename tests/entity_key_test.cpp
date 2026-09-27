#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"
#include "synthetic_world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <entt/entity/registry.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <random>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {
namespace {

struct Payload {
  int64_t Value = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Payload &payload) {
  visitor.field("value", payload.Value);
}

struct Flag {};

std::shared_ptr<const WorldSchema> makeKeySchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Payload>("payload", DataKind::State);
  schema->addComponent<Flag>("flag", DataKind::State);
  return schema;
}

constexpr EntityKey keyOfValue(uint64_t value) { return EntityKey{value}; }

constexpr uint64_t PURPOSE_A = hashName("a");
constexpr uint64_t PURPOSE_B = hashName("b");

TEST_CASE("a default world has tick 0, seed 0, next key 1, and no entities") {
  const World world;
  CHECK(world.Tick == 0);
  CHECK(world.Seed == 0);
  CHECK(world.nextKey() == 1);
  CHECK(world.keys().empty());
}

TEST_CASE("a world made with a schema and seed 42 has seed 42") {
  const World world(makeKeySchema(), 42);
  CHECK(world.Seed == 42);
  CHECK(world.Tick == 0);
  CHECK(world.nextKey() == 1);
  CHECK(world.keys().empty());
}

TEST_CASE("createEntity returns keys 1, 2, 3 in call order") {
  World world(makeKeySchema(), 0);
  CHECK(world.createEntity() == keyOfValue(1));
  CHECK(world.createEntity() == keyOfValue(2));
  CHECK(world.createEntity() == keyOfValue(3));
  CHECK(world.nextKey() == 4);
}

TEST_CASE("after key 2 is destroyed the next key is 4 and nextKey is 5") {
  World world(makeKeySchema(), 0);
  world.createEntity();
  world.createEntity();
  world.createEntity();
  REQUIRE(world.destroyEntity(keyOfValue(2)));
  CHECK(world.createEntity() == keyOfValue(4));
  CHECK(world.nextKey() == 5);
}

TEST_CASE("no sequence of creates and destroys returns a key twice") {
  const auto seed = GENERATE(range<uint64_t>(1, 21));
  std::mt19937_64 rng(seed);
  World world(makeKeySchema(), seed);
  std::set<EntityKey> returned;
  std::vector<EntityKey> live;
  bool repeated = false;
  bool counterMismatch = false;
  for (int step = 0; step < 400; ++step) {
    const uint64_t choice = rng() % 3;
    if (choice == 0 && !live.empty()) {
      const size_t at = rng() % live.size();
      world.destroyEntity(live[at]);
      live.erase(live.begin() + static_cast<std::ptrdiff_t>(at));
    } else if (choice == 1 && !live.empty()) {
      world.createDerivedEntity(live[rng() % live.size()], PURPOSE_A, rng() % 8);
    } else {
      const EntityKey key = world.createEntity();
      repeated = repeated || !returned.insert(key).second;
      counterMismatch = counterMismatch || world.nextKey() != static_cast<uint64_t>(key) + 1;
      live.push_back(key);
    }
  }
  CAPTURE(seed);
  CHECK_FALSE(repeated);
  CHECK_FALSE(counterMismatch);
}

TEST_CASE("findEntity and keyOf map a live key to its entity and back") {
  World world(makeKeySchema(), 0);
  world.createEntity();
  const EntityKey key = world.createEntity();
  const entt::entity entity = world.findEntity(key);
  REQUIRE(entity != entt::null);
  CHECK(world.Registry.valid(entity));
  CHECK(world.keyOf(entity) == key);

  const EntityKey derived = world.createDerivedEntity(key, PURPOSE_A, 3);
  const entt::entity derivedEntity = world.findEntity(derived);
  REQUIRE(derivedEntity != entt::null);
  CHECK(derivedEntity != entity);
  CHECK(world.keyOf(derivedEntity) == derived);
}

TEST_CASE("findEntity returns null for a key never created") {
  World world(makeKeySchema(), 0);
  const EntityKey owner = world.createEntity();
  CHECK(world.findEntity(NULL_KEY) == entt::null);
  CHECK(world.findEntity(keyOfValue(2)) == entt::null);
  CHECK(world.findEntity(keyOfValue(99)) == entt::null);
  CHECK(world.findEntity(deriveKey(owner, PURPOSE_A, 0)) == entt::null);
}

TEST_CASE("findEntity returns null for a destroyed key") {
  World world(makeKeySchema(), 0);
  const EntityKey key = world.createEntity();
  const EntityKey derived = world.createDerivedEntity(key, PURPOSE_B, 1);
  REQUIRE(world.destroyEntity(key));
  REQUIRE(world.destroyEntity(derived));
  CHECK(world.findEntity(key) == entt::null);
  CHECK(world.findEntity(derived) == entt::null);
}

TEST_CASE("keyOf returns NULL_KEY for entt::null and for an entity the world did not create") {
  World world(makeKeySchema(), 0);
  world.createEntity();
  CHECK(world.keyOf(entt::null) == NULL_KEY);
  const entt::entity stranger = world.Registry.create();
  CHECK(world.keyOf(stranger) == NULL_KEY);
}

TEST_CASE("destroyEntity removes a live entity with all its components") {
  World world(makeKeySchema(), 0);
  const EntityKey kept = world.createEntity();
  const EntityKey doomed = world.createEntity();
  const entt::entity keptEntity = synthetic::requireEntity(world, kept);
  const entt::entity doomedEntity = synthetic::requireEntity(world, doomed);
  world.Registry.emplace<Payload>(keptEntity, Payload{11});
  world.Registry.emplace<Payload>(doomedEntity, Payload{22});
  world.Registry.emplace<Flag>(doomedEntity);

  CHECK(world.destroyEntity(doomed));
  CHECK_FALSE(world.Registry.valid(doomedEntity));
  CHECK(world.Registry.storage<Payload>().size() == 1);
  CHECK(world.Registry.storage<Flag>().empty());
  CHECK(world.keys() == std::vector<EntityKey>{kept});
  CHECK(world.Registry.get<Payload>(keptEntity).Value == 11);
}

TEST_CASE("destroyEntity returns false and changes nothing when the key is not live") {
  World world(makeKeySchema(), 0);
  const EntityKey first = world.createEntity();
  const EntityKey second = world.createEntity();
  world.Registry.emplace<Payload>(synthetic::requireEntity(world, first), Payload{5});
  REQUIRE(world.destroyEntity(second));
  const std::vector<EntityKey> keysBefore = world.keys();
  const uint64_t nextBefore = world.nextKey();

  const EntityKey notLive = GENERATE(NULL_KEY, keyOfValue(2), keyOfValue(3), keyOfValue(1000),
                                     deriveKey(keyOfValue(1), hashName("a"), 0));
  CAPTURE(notLive);
  CHECK_FALSE(world.destroyEntity(notLive));
  CHECK(world.keys() == keysBefore);
  CHECK(world.nextKey() == nextBefore);
  REQUIRE(world.findEntity(first) != entt::null);
  CHECK(world.Registry.get<Payload>(synthetic::requireEntity(world, first)).Value == 5);
  CHECK(world.Registry.storage<Payload>().size() == 1);
}

TEST_CASE("keys lists live keys in ascending order with derived keys after counter keys") {
  World world(makeKeySchema(), 0);
  const EntityKey one = world.createEntity();
  const EntityKey derivedEarly = world.createDerivedEntity(one, PURPOSE_A, 0);
  world.createEntity();
  const EntityKey three = world.createEntity();
  const EntityKey derivedLate = world.createDerivedEntity(three, PURPOSE_B, 7);
  const EntityKey four = world.createEntity();
  world.destroyEntity(keyOfValue(2));
  const EntityKey five = world.createEntity();

  std::vector<EntityKey> expectedDerived{derivedEarly, derivedLate};
  std::sort(expectedDerived.begin(), expectedDerived.end());
  const std::vector<EntityKey> expected{
      one, three, four, five, expectedDerived[0], expectedDerived[1]};
  CHECK(world.keys() == expected);
}

TEST_CASE("keys stays ascending over random creates and destroys") {
  const auto seed = GENERATE(range<uint64_t>(1, 11));
  std::mt19937_64 rng(seed);
  World world(makeKeySchema(), seed);
  std::set<EntityKey> expected;
  for (int step = 0; step < 300; ++step) {
    const uint64_t choice = rng() % 4;
    if (choice == 0 && !expected.empty()) {
      auto it = expected.begin();
      std::advance(it, static_cast<std::ptrdiff_t>(rng() % expected.size()));
      world.destroyEntity(*it);
      expected.erase(it);
    } else if (choice == 1 && !expected.empty()) {
      expected.insert(world.createDerivedEntity(*expected.begin(), PURPOSE_B, rng() % 16));
    } else {
      expected.insert(world.createEntity());
    }
  }
  const std::vector<EntityKey> listed = world.keys();
  CAPTURE(seed);
  CHECK(listed == std::vector<EntityKey>(expected.begin(), expected.end()));
  CHECK(std::is_sorted(listed.begin(), listed.end()));
  const auto firstDerived = std::find_if(listed.begin(), listed.end(), isDerivedKey);
  CHECK(std::none_of(firstDerived, listed.end(), [](EntityKey key) { return !isDerivedKey(key); }));
}

TEST_CASE("deriveKey is constexpr and depends only on its arguments") {
  constexpr EntityKey derived = deriveKey(keyOfValue(7), hashName("a"), 3);
  STATIC_REQUIRE(derived == deriveKey(keyOfValue(7), hashName("a"), 3));
  STATIC_REQUIRE(isDerivedKey(derived));

  World first(makeKeySchema(), 1);
  World second(makeKeySchema(), 999);
  second.createEntity();
  second.createEntity();
  CHECK(first.createDerivedEntity(keyOfValue(7), hashName("a"), 3) == derived);
  CHECK(second.createDerivedEntity(keyOfValue(7), hashName("a"), 3) == derived);
  CHECK(deriveKey(keyOfValue(7), hashName("a"), 3) == derived);
}

TEST_CASE("isDerivedKey is true for derived keys and false for counter keys and NULL_KEY") {
  STATIC_REQUIRE_FALSE(isDerivedKey(NULL_KEY));
  STATIC_REQUIRE_FALSE(isDerivedKey(keyOfValue(1)));
  STATIC_REQUIRE_FALSE(isDerivedKey(keyOfValue(DERIVED_KEY_BIT - 1)));

  World world(makeKeySchema(), 0);
  bool counterKeyDerived = false;
  bool derivedKeyNotDerived = false;
  for (int i = 0; i < 1000; ++i) {
    const EntityKey key = world.createEntity();
    counterKeyDerived = counterKeyDerived || isDerivedKey(key);
    derivedKeyNotDerived =
        derivedKeyNotDerived || !isDerivedKey(deriveKey(key, PURPOSE_A, static_cast<uint64_t>(i)));
    derivedKeyNotDerived = derivedKeyNotDerived || !isDerivedKey(deriveKey(NULL_KEY, 0, 0));
  }
  CHECK_FALSE(counterKeyDerived);
  CHECK_FALSE(derivedKeyNotDerived);
}

TEST_CASE("100,000 derived keys over owners, purposes, and indexes are all distinct") {
  std::vector<EntityKey> derived;
  derived.reserve(100000);
  for (uint64_t owner = 1; owner <= 100; ++owner) {
    for (const uint64_t purpose : {PURPOSE_A, PURPOSE_B}) {
      for (uint64_t index = 0; index < 500; ++index) {
        derived.push_back(deriveKey(keyOfValue(owner), purpose, index));
      }
    }
  }
  REQUIRE(derived.size() == 100000);
  std::sort(derived.begin(), derived.end());
  CHECK(std::adjacent_find(derived.begin(), derived.end()) == derived.end());
  CHECK(std::all_of(derived.begin(), derived.end(), isDerivedKey));
}

TEST_CASE("createDerivedEntity creates the entity keyed deriveKey on the first call") {
  World world(makeKeySchema(), 0);
  const EntityKey owner = world.createEntity();
  const uint64_t nextBefore = world.nextKey();
  const EntityKey derived = world.createDerivedEntity(owner, PURPOSE_A, 4);
  CHECK(derived == deriveKey(owner, PURPOSE_A, 4));
  CHECK(world.findEntity(derived) != entt::null);
  CHECK(world.keys() == std::vector<EntityKey>{owner, derived});
  CHECK(world.nextKey() == nextBefore);
}

TEST_CASE("createDerivedEntity called again returns the same entity and keeps its components") {
  World world(makeKeySchema(), 0);
  const EntityKey owner = world.createEntity();
  const EntityKey derived = world.createDerivedEntity(owner, PURPOSE_B, 9);
  const entt::entity entity = synthetic::requireEntity(world, derived);
  world.Registry.emplace<Payload>(entity, Payload{77});
  world.Registry.emplace<Flag>(entity);
  const uint64_t nextBefore = world.nextKey();

  CHECK(world.createDerivedEntity(owner, PURPOSE_B, 9) == derived);
  CHECK(world.createDerivedEntity(owner, PURPOSE_B, 9) == derived);
  CHECK(world.findEntity(derived) == entity);
  CHECK(world.keys().size() == 2);
  CHECK(world.nextKey() == nextBefore);
  REQUIRE(world.Registry.all_of<Payload, Flag>(entity));
  CHECK(world.Registry.get<Payload>(entity).Value == 77);
}

TEST_CASE("createDerivedEntity after a destroy creates the entity again under the same key") {
  World world(makeKeySchema(), 0);
  const EntityKey owner = world.createEntity();
  const EntityKey derived = world.createDerivedEntity(owner, PURPOSE_A, 0);
  world.Registry.emplace<Payload>(synthetic::requireEntity(world, derived), Payload{3});
  REQUIRE(world.destroyEntity(derived));
  REQUIRE(world.findEntity(derived) == entt::null);
  const uint64_t nextBefore = world.nextKey();

  CHECK(world.createDerivedEntity(owner, PURPOSE_A, 0) == derived);
  const entt::entity recreated = world.findEntity(derived);
  REQUIRE(recreated != entt::null);
  CHECK(world.keyOf(recreated) == derived);
  CHECK_FALSE(world.Registry.all_of<Payload>(recreated));
  CHECK(world.keys() == std::vector<EntityKey>{owner, derived});
  CHECK(world.nextKey() == nextBefore);
}

TEST_CASE("createDerivedEntity never changes nextKey") {
  const auto seed = GENERATE(range<uint64_t>(1, 6));
  std::mt19937_64 rng(seed);
  World world(makeKeySchema(), seed);
  const EntityKey owner = world.createEntity();
  const uint64_t nextBefore = world.nextKey();
  bool changed = false;
  for (int i = 0; i < 200; ++i) {
    const EntityKey derived = world.createDerivedEntity(owner, rng() % 3, rng() % 20);
    if ((rng() & 1U) != 0) {
      world.destroyEntity(derived);
    }
    changed = changed || world.nextKey() != nextBefore;
  }
  CHECK_FALSE(changed);
}

TEST_CASE("hashName is constexpr and equal for equal strings") {
  STATIC_REQUIRE(hashName("shop") == hashName("shop"));
  STATIC_REQUIRE(hashName("a") != hashName("b"));
  const std::string left = "box-2";
  const std::string right = std::string("box-") + "2";
  CHECK(hashName(left) == hashName(right));
}

TEST_CASE("hashName gives distinct values for name0 to name999") {
  std::set<uint64_t> hashes;
  for (int i = 0; i < 1000; ++i) {
    hashes.insert(hashName("name" + std::to_string(i)));
  }
  CHECK(hashes.size() == 1000);
}

} // namespace
} // namespace tpj
