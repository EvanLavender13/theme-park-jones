#ifndef TPJ_SIM_DRAW_H
#define TPJ_SIM_DRAW_H

#include "sim/entity_key.h"
#include "sim/mix.h"

#include <span>
#include <stddef.h>
#include <stdint.h>

namespace tpj {

class World;

// Everything a draw depends on. Purpose is usually a hashName, as for deriveKey.
struct DrawKey {
  uint64_t Seed = 0;
  EntityKey Entity = NULL_KEY;
  uint64_t Purpose = 0;
  uint64_t Tick = 0;
  uint64_t Index = 0;
};

// The key for a draw made now: the world's seed and current tick, with the entity, purpose, and
// index given.
DrawKey drawKey(const World &world, EntityKey entity, uint64_t purpose, uint64_t index);

// 64 bits that depend on the key alone: its five words folded, in order, by Hasher.
constexpr uint64_t drawBits(const DrawKey &key) {
  Hasher hasher;
  hasher.add(key.Seed);
  hasher.add(static_cast<uint64_t>(key.Entity));
  hasher.add(key.Purpose);
  hasher.add(key.Tick);
  hasher.add(key.Index);
  return hasher.value();
}

// The draw's top 53 bits as a double in [0, 1).
constexpr double drawUniform(const DrawKey &key) {
  return static_cast<double>(drawBits(key) >> 11U) * 0x1.0p-53;
}

// An index into weights, picked with probability proportional to its weight, from one draw.
// Throws std::invalid_argument for an empty list, a zero total, or a total above 2^64 - 1.
size_t drawPick(const DrawKey &key, std::span<const uint64_t> weights);
// Throws std::invalid_argument for an empty list, a negative, NaN, or infinite weight, or a total
// that overflows or is below the smallest normal double.
size_t drawPick(const DrawKey &key, std::span<const double> weights);

} // namespace tpj

#endif
