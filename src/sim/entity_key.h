#ifndef TPJ_SIM_ENTITY_KEY_H
#define TPJ_SIM_ENTITY_KEY_H

#include "sim/mix.h"

#include <stdint.h>

namespace tpj {

// An entity's identity: stable across copies, saves, and loads, unlike EnTT's recycled handles.
// Components refer to other entities by key.
enum class EntityKey : uint64_t {};

constexpr EntityKey NULL_KEY{0};

// Keys from a world's counter lie below this bit. Keys derived by resolvers have it set.
constexpr uint64_t DERIVED_KEY_BIT = uint64_t{1} << 63U;

constexpr bool isDerivedKey(EntityKey key) {
  return (static_cast<uint64_t>(key) & DERIVED_KEY_BIT) != 0;
}

// The key a resolver gives an entity it derives from an owner, for a purpose (usually a hashName)
// and an index. It depends only on its arguments, so resolving the same intent again, in a copy or
// after a load, gives the same key.
constexpr EntityKey deriveKey(EntityKey owner, uint64_t purpose, uint64_t index) {
  Hasher hasher;
  hasher.add(static_cast<uint64_t>(owner));
  hasher.add(purpose);
  hasher.add(index);
  return EntityKey{DERIVED_KEY_BIT | (hasher.value() >> 1U)};
}

} // namespace tpj

#endif
