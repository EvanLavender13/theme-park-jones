#include "support/synthetic_types.h"

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::makeSchema;

// The statistical tests' sample, from the acceptance of keyed draws.
constexpr uint64_t SAMPLE = 1'000'000;

constexpr uint64_t WANDER = hashName("wander");

DrawKey sampleKey(uint64_t purpose, uint64_t index) {
  return DrawKey{
      .Seed = 2024, .Entity = EntityKey{42}, .Purpose = purpose, .Tick = 9000, .Index = index};
}

size_t pickInteger(const DrawKey &key, const std::vector<uint64_t> &weights) {
  return drawPick(key, std::span<const uint64_t>(weights));
}

size_t pickDouble(const DrawKey &key, const std::vector<double> &weights) {
  return drawPick(key, std::span<const double>(weights));
}

void requireSameKey(const DrawKey &actual, const DrawKey &expected) {
  REQUIRE(actual.Seed == expected.Seed);
  REQUIRE(actual.Entity == expected.Entity);
  REQUIRE(actual.Purpose == expected.Purpose);
  REQUIRE(actual.Tick == expected.Tick);
  REQUIRE(actual.Index == expected.Index);
}

// Each count is within five standard deviations, sqrt(n p (1 - p)), of n p.
void requireProportional(const std::vector<uint64_t> &counts, const std::vector<double> &shares) {
  const auto n = static_cast<double>(SAMPLE);
  for (size_t i = 0; i < counts.size(); ++i) {
    CAPTURE(i, counts[i], shares[i]);
    const double expected = n * shares[i];
    const double deviation = std::sqrt(n * shares[i] * (1.0 - shares[i]));
    REQUIRE(std::abs(static_cast<double>(counts[i]) - expected) <= 5.0 * deviation);
  }
}

TEST_CASE("a draw depends on its key alone, whatever order and other draws surround it") {
  struct Draw {
    uint64_t Bits = 0;
    double Uniform = 0.0;
    size_t IntegerPick = 0;
    size_t DoublePick = 0;
  };
  const std::vector<uint64_t> integerWeights = {4, 1, 6};
  const std::vector<double> doubleWeights = {0.3, 1.2, 0.5};
  const auto drawAll = [&](const DrawKey &key) {
    return Draw{.Bits = drawBits(key),
                .Uniform = drawUniform(key),
                .IntegerPick = pickInteger(key, integerWeights),
                .DoublePick = pickDouble(key, doubleWeights)};
  };

  // Keys differing in every word but the seed, so that no single word's order explains a match.
  std::vector<DrawKey> keys;
  for (uint64_t i = 0; i < 64; ++i) {
    keys.push_back(DrawKey{.Seed = 5,
                           .Entity = EntityKey{1 + (i % 4)},
                           .Purpose = hashName(i % 2 == 0 ? "wander" : "eat"),
                           .Tick = 100 + (i % 3),
                           .Index = i});
  }
  std::vector<Draw> inOrder;
  inOrder.reserve(keys.size());
  for (const DrawKey &key : keys) {
    inOrder.push_back(drawAll(key));
  }

  const auto requireSame = [&](size_t i, const Draw &draw) {
    CAPTURE(i);
    REQUIRE(draw.Bits == inOrder[i].Bits);
    REQUIRE(draw.Uniform == inOrder[i].Uniform);
    REQUIRE(draw.IntegerPick == inOrder[i].IntegerPick);
    REQUIRE(draw.DoublePick == inOrder[i].DoublePick);
  };

  // Reversed, with an unrelated draw of each kind before every key's.
  for (size_t j = keys.size(); j-- > 0;) {
    static_cast<void>(drawAll(sampleKey(WANDER, j)));
    requireSame(j, drawAll(keys[j]));
  }
  // A stride coprime to the key count visits every key once, in a scattered order.
  for (size_t j = 0; j < keys.size(); ++j) {
    const size_t i = (j * 37) % keys.size();
    requireSame(i, drawAll(keys[i]));
  }
}

TEST_CASE("drawKey takes the world's seed and tick at the time of the call") {
  World world(makeSchema(), 777);
  world.Tick = 3000;
  const EntityKey entity{7};

  requireSameKey(
      drawKey(world, entity, WANDER, 2),
      DrawKey{.Seed = 777, .Entity = entity, .Purpose = WANDER, .Tick = 3000, .Index = 2});

  stepWorld(world);
  requireSameKey(
      drawKey(world, entity, WANDER, 2),
      DrawKey{.Seed = 777, .Entity = entity, .Purpose = WANDER, .Tick = 3001, .Index = 2});

  world.Seed = 778;
  requireSameKey(
      drawKey(world, entity, WANDER, 2),
      DrawKey{.Seed = 778, .Entity = entity, .Purpose = WANDER, .Tick = 3001, .Index = 2});
}

TEST_CASE("drawUniform lies in [0, 1)") {
  constexpr uint64_t MAX_WORD = std::numeric_limits<uint64_t>::max();
  // Every word zero and every word at its maximum, the keys at the ends of the key space, and a
  // typical key.
  const std::vector<DrawKey> keys = {
      DrawKey{},
      DrawKey{.Seed = MAX_WORD,
              .Entity = EntityKey{MAX_WORD},
              .Purpose = MAX_WORD,
              .Tick = MAX_WORD,
              .Index = MAX_WORD},
      sampleKey(WANDER, 0),
  };
  for (const DrawKey &key : keys) {
    const double uniform = drawUniform(key);
    CAPTURE(uniform);
    REQUIRE(uniform >= 0.0);
    REQUIRE(uniform < 1.0);
  }
}

TEST_CASE("drawUniform passes a chi-square test over 1000 equal bins") {
  constexpr size_t BINS = 1000;
  std::vector<uint64_t> counts(BINS, 0);
  const uint64_t purpose = hashName("chi-square");
  for (uint64_t index = 0; index < SAMPLE; ++index) {
    // The product can round up to BINS when the draw is just below 1.
    const auto bin = std::min(
        static_cast<size_t>(drawUniform(sampleKey(purpose, index)) * static_cast<double>(BINS)),
        BINS - 1);
    ++counts[bin];
  }

  const double expected = static_cast<double>(SAMPLE) / static_cast<double>(BINS);
  double statistic = 0.0;
  for (const uint64_t count : counts) {
    const double difference = static_cast<double>(count) - expected;
    statistic += difference * difference / expected;
  }
  // The lower and upper 0.001 critical values of chi-square with 999 degrees of freedom: too
  // uniform is as suspect as too lumpy.
  CAPTURE(statistic);
  REQUIRE(statistic > 866.55);
  REQUIRE(statistic < 1142.85);
}

TEST_CASE("drawPick picks each index in proportion to its weight") {
  SECTION("integer weights") {
    // Shares from a tenth of a percent up to most of the total.
    const std::vector<uint64_t> weights = {1, 10, 100, 889};
    std::vector<uint64_t> counts(weights.size(), 0);
    const uint64_t purpose = hashName("integer pick");
    for (uint64_t index = 0; index < SAMPLE; ++index) {
      ++counts[pickInteger(sampleKey(purpose, index), weights)];
    }
    std::vector<double> shares;
    shares.reserve(weights.size());
    for (const uint64_t weight : weights) {
      shares.push_back(static_cast<double>(weight) / 1000.0);
    }
    requireProportional(counts, shares);
  }

  SECTION("double weights") {
    const std::vector<double> weights = {0.1, 0.3, 2.5, 1.1};
    std::vector<uint64_t> counts(weights.size(), 0);
    const uint64_t purpose = hashName("double pick");
    for (uint64_t index = 0; index < SAMPLE; ++index) {
      ++counts[pickDouble(sampleKey(purpose, index), weights)];
    }
    const double total = 0.1 + 0.3 + 2.5 + 1.1;
    std::vector<double> shares;
    shares.reserve(weights.size());
    for (const double weight : weights) {
      shares.push_back(weight / total);
    }
    requireProportional(counts, shares);
  }
}

TEST_CASE("drawPick never picks an index whose weight is zero") {
  // Zero weights first, adjacent in the middle, and last: the first is where a pick of the draw's
  // lowest values would land if a zero running sum counted, and the others share a running sum
  // with the index before them.
  const std::vector<uint64_t> integerWeights = {0, 3, 0, 0, 5, 0};
  const std::vector<double> doubleWeights = {0.0, 1.5, 0.0, 0.0, 2.5, 0.0};
  const uint64_t purpose = hashName("zero weights");
  for (uint64_t index = 0; index < 1000; ++index) {
    const DrawKey key = sampleKey(purpose, index);
    CAPTURE(index);
    REQUIRE(integerWeights[pickInteger(key, integerWeights)] != 0);
    REQUIRE(doubleWeights[pickDouble(key, doubleWeights)] != 0.0);
  }
}

TEST_CASE("drawPick refuses weights it cannot pick from in proportion") {
  // This total wraps to 1, so only a check for overflow refuses it.
  const std::vector<uint64_t> weights = {std::numeric_limits<uint64_t>::max(), 2};
  REQUIRE_THROWS_AS(pickInteger(sampleKey(WANDER, 0), weights), std::invalid_argument);
}

} // namespace
} // namespace tpj
