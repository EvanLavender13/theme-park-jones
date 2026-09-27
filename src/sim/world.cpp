#include "sim/world.h"

#include <bit>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>

namespace tpj {

namespace {

// True when this thread's floating-point environment is the one the simulation's results assume:
// round-to-nearest, with subnormal results kept and subnormal operands read as themselves. It is
// measured by arithmetic, so it needs no platform-specific code. The operands are volatile so the
// compiler cannot fold the operations, and results are compared by bits because comparing a
// subnormal as a double would read it as zero when denormals-are-zero is set.
bool isDefaultFloatEnvironment() {
  const volatile double one = 1.0;
  const volatile double beyondHalfUlp = 0x1.8p-53;
  const volatile double smallestNormal = std::numeric_limits<double>::min();
  const volatile double smallestSubnormal = std::numeric_limits<double>::denorm_min();
  // Only round-to-nearest rounds both of these away from 1.
  const double above = one + beyondHalfUlp;
  const double below = -one - beyondHalfUlp;
  // Flush-to-zero would make this 0.
  const double halvedNormal = smallestNormal * 0.5;
  // Denormals-are-zero would make this 0.
  const double doubledSubnormal = smallestSubnormal * 2.0;
  return std::bit_cast<uint64_t>(above) == std::bit_cast<uint64_t>(1.0 + 0x1p-52) &&
         std::bit_cast<uint64_t>(below) == std::bit_cast<uint64_t>(-1.0 - 0x1p-52) &&
         std::bit_cast<uint64_t>(halvedNormal) == std::bit_cast<uint64_t>(0x1p-1023) &&
         std::bit_cast<uint64_t>(doubledSubnormal) == std::bit_cast<uint64_t>(0x1p-1073);
}

} // namespace

World::World() : World(std::make_shared<const WorldSchema>(), 0) {}

World::World(std::shared_ptr<const WorldSchema> schema, uint64_t seed)
    : Seed(seed), Schema(std::move(schema)) {
  if (!Schema) {
    throw std::invalid_argument("a world needs a schema");
  }
  if (WORLD_CHECKS && !isDefaultFloatEnvironment()) {
    throw WorldInvariantError(
        "the floating-point environment is not the default: round-to-nearest, "
        "with subnormals neither flushed to zero nor read as zero");
  }
}

entt::entity World::addEntity(EntityKey key) {
  const entt::entity entity = Registry.create();
  ByKey[key] = EntityRecord{entity, std::nullopt};
  KeyByEntity[entity] = key;
  return entity;
}

EntityKey World::createEntity() {
  if (WORLD_CHECKS && Resolving) {
    throw WorldInvariantError("a resolver called createEntity; resolvers take derived keys");
  }
  if (NextKey >= DERIVED_KEY_BIT) {
    throw WorldInvariantError("the entity key counter is exhausted");
  }
  const EntityKey key{NextKey};
  ++NextKey;
  addEntity(key);
  return key;
}

EntityKey World::createDerivedEntity(EntityKey owner, uint64_t purpose, uint64_t index) {
  const EntityKey key = deriveKey(owner, purpose, index);
  const DerivedOrigin origin{owner, purpose, index};
  const auto found = ByKey.find(key);
  if (found == ByKey.end()) {
    addEntity(key);
    ByKey.at(key).Origin = origin;
  } else if (!found->second.Origin) {
    // A loaded entity takes the origin it is resolved from.
    found->second.Origin = origin;
  } else if (WORLD_CHECKS && found->second.Origin != origin) {
    throw WorldInvariantError("derived key " + std::to_string(static_cast<uint64_t>(key)) +
                              " is shared by two origins");
  }
  return key;
}

bool World::destroyEntity(EntityKey key) {
  const auto found = ByKey.find(key);
  if (found == ByKey.end()) {
    return false;
  }
  const entt::entity entity = found->second.Entity;
  KeyByEntity.erase(entity);
  ByKey.erase(found);
  Registry.destroy(entity);
  return true;
}

entt::entity World::findEntity(EntityKey key) const {
  const auto found = ByKey.find(key);
  return found == ByKey.end() ? entt::null : found->second.Entity;
}

EntityKey World::keyOf(entt::entity entity) const {
  const auto found = KeyByEntity.find(entity);
  return found == KeyByEntity.end() ? NULL_KEY : found->second;
}

std::vector<EntityKey> World::keys() const {
  std::vector<EntityKey> result;
  result.reserve(ByKey.size());
  for (const auto &entry : ByKey) {
    result.push_back(entry.first);
  }
  return result;
}

} // namespace tpj
