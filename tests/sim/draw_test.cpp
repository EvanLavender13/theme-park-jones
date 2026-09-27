#include "support/expected_draws.h"
#include "support/synthetic_types.h"

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::buildWorld;
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

// The table in expected_draws.h is written by tests/sim/support/expected_draws.py, a separate
// implementation of the definitions in src/sim/SPEC.md.
// Draws that match it on every build are the same on every build.
TEST_CASE("draws match the table computed independently from the specification") {
  const std::span<const uint64_t> integerWeights(test::EXPECTED_DRAW_INTEGER_WEIGHTS);
  const std::span<const double> doubleWeights(test::EXPECTED_DRAW_DOUBLE_WEIGHTS);
  for (const test::ExpectedDraw &expected : test::EXPECTED_DRAWS) {
    const DrawKey &key = expected.Key;
    CAPTURE(key.Seed, key.Entity, key.Purpose, key.Tick, key.Index);
    REQUIRE(drawBits(key) == expected.Bits);
    REQUIRE(drawUniform(key) == expected.Uniform);
    REQUIRE(drawPick(key, integerWeights) == expected.IntegerPick);
    REQUIRE(drawPick(key, doubleWeights) == expected.DoublePick);
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

TEST_CASE("drawUniform is the draw's top 53 bits times 2^-53, in [0, 1)") {
  for (uint64_t index = 0; index < 1000; ++index) {
    const DrawKey key = sampleKey(WANDER, index);
    CAPTURE(index);
    const double uniform = drawUniform(key);
    REQUIRE(uniform == static_cast<double>(drawBits(key) >> 11U) * 0x1.0p-53);
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
  const DrawKey key = sampleKey(WANDER, 0);
  constexpr uint64_t MAX_TOTAL = std::numeric_limits<uint64_t>::max();

  SECTION("integer weights") {
    REQUIRE_THROWS_AS(pickInteger(key, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickInteger(key, {0, 0}), std::invalid_argument);
    // This total wraps to 1, so only a check for overflow refuses it.
    REQUIRE_THROWS_AS(pickInteger(key, {MAX_TOTAL, 2}), std::invalid_argument);

    // The largest total allowed, from one weight and from two.
    REQUIRE(pickInteger(key, {MAX_TOTAL}) == 0);
    REQUIRE_NOTHROW(pickInteger(key, {MAX_TOTAL - 1, 1}));
  }

  SECTION("double weights") {
    constexpr double SMALLEST_NORMAL = std::numeric_limits<double>::min();
    constexpr double LARGEST = std::numeric_limits<double>::max();
    constexpr double INF = std::numeric_limits<double>::infinity();
    constexpr double NAN_WEIGHT = std::numeric_limits<double>::quiet_NaN();

    REQUIRE_THROWS_AS(pickDouble(key, {}), std::invalid_argument);
    // The total is positive, so only a check of each weight refuses it.
    REQUIRE_THROWS_AS(pickDouble(key, {-1.0, 2.0}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickDouble(key, {NAN_WEIGHT, 1.0}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickDouble(key, {INF, 1.0}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickDouble(key, {0.0, 0.0}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickDouble(key, {SMALLEST_NORMAL / 2.0}), std::invalid_argument);
    REQUIRE_THROWS_AS(pickDouble(key, {LARGEST, LARGEST}), std::invalid_argument);

    // The smallest normal total, from one weight and from two subnormal weights, and the largest.
    REQUIRE(pickDouble(key, {SMALLEST_NORMAL}) == 0);
    REQUIRE_NOTHROW(pickDouble(key, {SMALLEST_NORMAL / 2.0, SMALLEST_NORMAL / 2.0}));
    REQUIRE(pickDouble(key, {LARGEST}) == 0);
  }
}

TEST_CASE("drawing leaves the world unchanged") {
  const World world = buildWorld(makeSchema());
  const World before = copyWorld(world);
  const uint64_t hashBefore = hashWorld(world);

  const std::vector<uint64_t> integerWeights = {2, 5};
  const std::vector<double> doubleWeights = {0.5, 0.25};
  for (const EntityKey entity : world.keys()) {
    for (uint64_t index = 0; index < 10; ++index) {
      const DrawKey key = drawKey(world, entity, WANDER, index);
      static_cast<void>(drawBits(key));
      static_cast<void>(drawUniform(key));
      static_cast<void>(pickInteger(key, integerWeights));
      static_cast<void>(pickDouble(key, doubleWeights));
    }
  }

  REQUIRE(hashWorld(world) == hashBefore);
  REQUIRE(worldsEqual(world, before));
}

TEST_CASE("a copy of a world draws the same values as the world") {
  World world = buildWorld(makeSchema());
  stepWorld(world);
  const World copy = copyWorld(world);
  for (const EntityKey entity : world.keys()) {
    for (uint64_t index = 0; index < 3; ++index) {
      CAPTURE(entity, index);
      REQUIRE(drawBits(drawKey(copy, entity, WANDER, index)) ==
              drawBits(drawKey(world, entity, WANDER, index)));
    }
  }
}

TEST_CASE("a candidate draws the same values as the world its commands would give") {
  // Two copies of one world: one finishes a cycle and previews, the other commits that cycle.
  World previewed = buildWorld(makeSchema());
  World committed = copyWorld(previewed);
  CommandQueue commands;
  stepWorld(previewed);
  const World candidate = makeCandidate(previewed, commands);
  stepWorld(committed, commands);

  for (const EntityKey entity : committed.keys()) {
    for (uint64_t index = 0; index < 3; ++index) {
      CAPTURE(entity, index);
      REQUIRE(drawBits(drawKey(candidate, entity, WANDER, index)) ==
              drawBits(drawKey(committed, entity, WANDER, index)));
    }
  }
}

} // namespace
} // namespace tpj
