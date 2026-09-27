#include "support/synthetic_types.h"

#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::Cached;
using test::makeSchema;
using test::Probe;
using test::SLOT_PURPOSE;
using test::Tag;

const entt::entity NULL_ENTITY = entt::null;

TEST_CASE("counter keys ascend from 1, and nextKey is the key the next call returns") {
  World world(makeSchema(), 0);
  REQUIRE(world.nextKey() == 1);
  for (uint64_t expected = 1; expected <= 3; ++expected) {
    const uint64_t predicted = world.nextKey();
    const EntityKey key = world.createEntity();
    REQUIRE(static_cast<uint64_t>(key) == predicted);
    REQUIRE(static_cast<uint64_t>(key) == expected);
  }
}

TEST_CASE("a destroyed entity's counter key never comes back") {
  World world(makeSchema(), 0);
  world.createEntity();
  const EntityKey second = world.createEntity();
  const EntityKey third = world.createEntity();

  // Destroying the newest key is where a counter that stepped back would reissue it.
  REQUIRE(world.destroyEntity(third));
  REQUIRE(world.destroyEntity(second));
  REQUIRE(world.nextKey() == 4);
  REQUIRE(world.createEntity() == EntityKey{4});
  REQUIRE(world.nextKey() == 5);
}

TEST_CASE("derived keys are a pure function of owner, purpose, and index") {
  // Worlds with different histories derive the same key from the same arguments.
  World sparse(makeSchema(), 1);
  World busy(makeSchema(), 2);
  const EntityKey owner = sparse.createEntity();
  REQUIRE(busy.createEntity() == owner);
  busy.createEntity();
  busy.createEntity();

  const EntityKey fromSparse = sparse.createDerivedEntity(owner, SLOT_PURPOSE, 0);
  const EntityKey fromBusy = busy.createDerivedEntity(owner, SLOT_PURPOSE, 0);
  REQUIRE(fromSparse == fromBusy);
  REQUIRE(fromSparse == deriveKey(owner, SLOT_PURPOSE, 0));

  // Each argument takes part: otherwise one owner could not derive entities for several purposes
  // or several of one purpose.
  REQUIRE(deriveKey(EntityKey{2}, SLOT_PURPOSE, 0) != fromSparse);
  REQUIRE(deriveKey(owner, hashName("other"), 0) != fromSparse);
  REQUIRE(deriveKey(owner, SLOT_PURPOSE, 1) != fromSparse);
}

TEST_CASE("derived keys lie outside the counter's range, and NULL_KEY is neither") {
  REQUIRE_FALSE(isDerivedKey(NULL_KEY));
  REQUIRE_FALSE(isDerivedKey(EntityKey{1}));
  REQUIRE_FALSE(isDerivedKey(EntityKey{DERIVED_KEY_BIT - 1}));
  REQUIRE(isDerivedKey(EntityKey{DERIVED_KEY_BIT}));

  World world(makeSchema(), 0);
  const EntityKey owner = world.createEntity();
  const EntityKey derived = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
  REQUIRE_FALSE(isDerivedKey(owner));
  REQUIRE(isDerivedKey(derived));
  REQUIRE(derived != NULL_KEY);
}

TEST_CASE("createDerivedEntity is idempotent and never moves the counter") {
  World world(makeSchema(), 0);
  const EntityKey owner = world.createEntity();
  const uint64_t counter = world.nextKey();

  const EntityKey derived = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
  const entt::entity entity = world.findEntity(derived);
  world.Registry.emplace<Cached>(entity, Cached{.Total = 42});
  REQUIRE(world.nextKey() == counter);
  const uint64_t hashBefore = hashWorld(world);

  const EntityKey again = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
  REQUIRE(again == derived);
  REQUIRE(world.findEntity(again) == entity);
  REQUIRE(world.Registry.get<Cached>(entity).Total == 42);
  REQUIRE(world.nextKey() == counter);
  REQUIRE(world.keys().size() == 2);
  REQUIRE(hashWorld(world) == hashBefore);
}

TEST_CASE("findEntity and keyOf are inverses for live entities") {
  World world(makeSchema(), 0);
  const EntityKey owner = world.createEntity();
  world.createEntity();
  world.createDerivedEntity(owner, SLOT_PURPOSE, 0);

  for (const EntityKey key : world.keys()) {
    const entt::entity entity = world.findEntity(key);
    REQUIRE(world.Registry.valid(entity));
    REQUIRE(world.keyOf(entity) == key);
    REQUIRE(world.findEntity(world.keyOf(entity)) == entity);
  }
}

TEST_CASE("lookups of anything not live give entt::null and NULL_KEY") {
  World world(makeSchema(), 0);
  const EntityKey owner = world.createEntity();
  const EntityKey doomed = world.createEntity();
  const entt::entity doomedEntity = world.findEntity(doomed);
  REQUIRE(world.destroyEntity(doomed));

  REQUIRE(world.findEntity(NULL_KEY) == NULL_ENTITY);
  REQUIRE(world.findEntity(doomed) == NULL_ENTITY);
  REQUIRE(world.findEntity(EntityKey{99}) == NULL_ENTITY);
  REQUIRE(world.findEntity(deriveKey(owner, SLOT_PURPOSE, 0)) == NULL_ENTITY);
  REQUIRE(world.keyOf(doomedEntity) == NULL_KEY);
  REQUIRE(world.keyOf(NULL_ENTITY) == NULL_KEY);
}

TEST_CASE("destroyEntity removes the entity and its components") {
  World world(makeSchema(), 0);
  const EntityKey kept = world.createEntity();
  const EntityKey doomed = world.createEntity();
  const entt::entity entity = world.findEntity(doomed);
  world.Registry.emplace<Probe>(entity);
  world.Registry.emplace<Tag>(entity);

  REQUIRE(world.destroyEntity(doomed));
  REQUIRE_FALSE(world.Registry.valid(entity));
  REQUIRE(world.Registry.storage<Probe>().empty());
  REQUIRE(world.Registry.storage<Tag>().empty());
  REQUIRE(world.keys() == std::vector<EntityKey>{kept});
}

TEST_CASE("destroyEntity of a key that is not live returns false and changes nothing") {
  World world = test::buildWorld(makeSchema());
  const EntityKey destroyed = world.createEntity();
  REQUIRE(world.destroyEntity(destroyed));
  const uint64_t hashBefore = hashWorld(world);
  const std::vector<EntityKey> keysBefore = world.keys();
  const uint64_t counterBefore = world.nextKey();

  const std::vector<EntityKey> notLive = {NULL_KEY, destroyed, EntityKey{99},
                                          deriveKey(EntityKey{1}, SLOT_PURPOSE, 7)};
  for (const EntityKey key : notLive) {
    CAPTURE(key);
    REQUIRE_FALSE(world.destroyEntity(key));
    REQUIRE(hashWorld(world) == hashBefore);
    REQUIRE(world.keys() == keysBefore);
    REQUIRE(world.nextKey() == counterBefore);
  }
}

TEST_CASE("keys() lists the live keys in ascending order") {
  World world(makeSchema(), 0);
  const EntityKey first = world.createEntity();
  // Derived entities made before later counter entities, and in descending index order, so that
  // creation order is not key order.
  const EntityKey derivedOne = world.createDerivedEntity(first, SLOT_PURPOSE, 1);
  const EntityKey derivedZero = world.createDerivedEntity(first, SLOT_PURPOSE, 0);
  const EntityKey second = world.createEntity();
  const EntityKey third = world.createEntity();
  const EntityKey fourth = world.createEntity();
  REQUIRE(world.destroyEntity(second));

  const std::vector<EntityKey> expected = {first, third, fourth, std::min(derivedOne, derivedZero),
                                           std::max(derivedOne, derivedZero)};
  REQUIRE(world.keys() == expected);
}

} // namespace
} // namespace tpj
