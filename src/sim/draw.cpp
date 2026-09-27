#include "sim/draw.h"

#include "sim/world.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace tpj {

namespace {

// The high 64 bits of the 128-bit product, from four 32-bit partial products, in standard C++.
uint64_t multiplyHigh(uint64_t left, uint64_t right) {
  const uint64_t leftLow = left & 0xffffffffU;
  const uint64_t leftHigh = left >> 32U;
  const uint64_t rightLow = right & 0xffffffffU;
  const uint64_t rightHigh = right >> 32U;
  const uint64_t lowLow = leftLow * rightLow;
  const uint64_t highLow = leftHigh * rightLow;
  const uint64_t lowHigh = leftLow * rightHigh;
  const uint64_t highHigh = leftHigh * rightHigh;
  // At most 2^64 - 1, so it cannot overflow.
  const uint64_t middle = (lowLow >> 32U) + (highLow & 0xffffffffU) + lowHigh;
  return highHigh + (highLow >> 32U) + (middle >> 32U);
}

} // namespace

DrawKey drawKey(const World &world, EntityKey entity, uint64_t purpose, uint64_t index) {
  return DrawKey{world.Seed, entity, purpose, world.Tick, index};
}

size_t drawPick(const DrawKey &key, std::span<const uint64_t> weights) {
  uint64_t total = 0;
  for (const uint64_t weight : weights) {
    if (weight > std::numeric_limits<uint64_t>::max() - total) {
      throw std::invalid_argument("draw weights total more than 2^64 - 1");
    }
    total += weight;
  }
  if (total == 0) {
    throw std::invalid_argument("draw weights are empty or all zero");
  }
  // Lemire's multiply-shift, without rejection: its bias is below total / 2^64.
  const uint64_t target = multiplyHigh(drawBits(key), total);
  uint64_t running = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
    running += weights[i];
    if (running > target) {
      return i;
    }
  }
  // Not reached: target is below total, and the running sum ends at total.
  return weights.size() - 1;
}

size_t drawPick(const DrawKey &key, std::span<const double> weights) {
  double total = 0.0;
  for (const double weight : weights) {
    if (!std::isfinite(weight) || weight < 0.0) {
      throw std::invalid_argument("a draw weight is negative, NaN, or infinite");
    }
    total += weight;
  }
  if (std::isinf(total)) {
    throw std::invalid_argument("draw weights total more than the largest double");
  }
  if (total < std::numeric_limits<double>::min()) {
    throw std::invalid_argument("draw weights total less than the smallest normal double");
  }
  // drawUniform is at most 1 - 2^-53, so for a normal total the product rounds below total.
  const double target = drawUniform(key) * total;
  // The running sum repeats the total's additions in the same order, so it ends exactly at total.
  // A zero weight leaves it unchanged, so a zero-weight index is never the first to exceed target.
  double running = 0.0;
  size_t lastPositive = 0;
  for (size_t i = 0; i < weights.size(); ++i) {
    running += weights[i];
    if (weights[i] > 0.0) {
      lastPositive = i;
    }
    if (running > target) {
      return i;
    }
  }
  // Not reached: target is below total, and the running sum ends at total.
  return lastPositive;
}

} // namespace tpj
