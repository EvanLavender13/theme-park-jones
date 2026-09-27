#include "sim/world.h"

#include "core/profile.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace tpj {

World::World() : World(std::make_shared<const WorldSchema>(), 0) {}

World::World(std::shared_ptr<const WorldSchema> schema, uint64_t seed)
    : Seed(seed), Schema(std::move(schema)) {
  if (!Schema) {
    throw std::invalid_argument("a world needs a schema");
  }
}

entt::entity World::addEntity(EntityKey key) {
  const entt::entity entity = Registry.create();
  ByKey[key] = EntityRecord{entity, std::nullopt};
  KeyByEntity[entity] = key;
  return entity;
}

EntityKey World::createEntity() {
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

void stepWorld(World &world) {
  TPJ_PROFILE_ZONE();
  ++world.Tick;
}

} // namespace tpj
