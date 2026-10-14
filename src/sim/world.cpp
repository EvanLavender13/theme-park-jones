#include "sim/world.h"

#include "sim/unit_space.h"

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

World::World(World &&) noexcept = default;
World &World::operator=(World &&) noexcept = default;
World::~World() = default;

World::World(std::shared_ptr<const WorldSchema> schema, uint64_t seed)
    : Seed(seed), Schema(std::move(schema)) {
  if (!Schema) {
    throw std::invalid_argument("a world needs a schema");
  }
  Stores.reserve(Schema->components().size());
  for (const ComponentType &type : Schema->components()) {
    Stores.push_back(type.MakeStore());
  }
  Units = std::make_unique<UnitSpace>(*Schema);
  if (WORLD_CHECKS && !isDefaultFloatEnvironment()) {
    throw WorldInvariantError(
        "the floating-point environment is not the default: round-to-nearest, "
        "with subnormals neither flushed to zero nor read as zero");
  }
}

void World::addEntity(EntityKey key) {
  if (SpareRecords.empty() || ByKey.contains(key)) {
    ByKey[key] = EntityRecord{std::nullopt};
    return;
  }
  auto node = std::move(SpareRecords.back());
  SpareRecords.pop_back();
  node.key() = key;
  node.mapped() = EntityRecord{std::nullopt};
  ByKey.insert(std::move(node));
}

EntityKey World::createEntity() {
  if (WORLD_CHECKS && (Resolving || Finishing)) {
    throw WorldInvariantError(
        "createEntity was called during resolution, which takes derived keys");
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
  // As a store's spare nodes do, a full list makes room for every record, so it rarely grows.
  if (SpareRecords.size() == SpareRecords.capacity()) {
    SpareRecords.reserve(ByKey.size() + SpareRecords.size());
  }
  SpareRecords.push_back(ByKey.extract(found));
  for (size_t index = 0; index < Stores.size(); ++index) {
    if (Units->isLogged(index) && Stores[index]->has(key)) {
      Units->noteChange(index, key);
    }
    Stores[index]->erase(key);
  }
  return true;
}

void World::noteChange(size_t store, EntityKey key) { Units->noteChange(store, key); }

UnitSpace &unitSpaceOf(World &world) { return *world.Units; }

const UnitSpace &unitSpaceOf(const World &world) { return *world.Units; }

std::vector<EntityKey> World::keys() const {
  std::vector<EntityKey> result;
  result.reserve(ByKey.size());
  for (const auto &entry : ByKey) {
    result.push_back(entry.first);
  }
  return result;
}

bool World::isLive(EntityKey key) const { return ByKey.contains(key); }

bool World::holds(const ComponentType &type, EntityKey key) const {
  const std::optional<size_t> index = Schema->indexOf(type.Id);
  return index && Stores[*index]->has(key);
}

} // namespace tpj
