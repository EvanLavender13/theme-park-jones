#include "support/sim_math_reference.h"

#include "sim/sim_math.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>
#include <span>
#include <stdint.h>
#include <string>

namespace tpj {
namespace {

using test::EXP_REFERENCE;
using test::LOG_REFERENCE;
using test::SimMathReference;

constexpr double INF = std::numeric_limits<double>::infinity();
constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double LARGEST = std::numeric_limits<double>::max();
constexpr double SMALLEST_SUBNORMAL = std::numeric_limits<double>::denorm_min();

// A double's bits as an integer in the same order as the doubles, so that adjacent doubles differ
// by one and both zeros map to zero.
int64_t orderedBits(double value) {
  const auto bits = std::bit_cast<int64_t>(value);
  return bits < 0 ? std::numeric_limits<int64_t>::min() - bits : bits;
}

// True when actual is expected or a double adjacent to it.
bool withinOneUlp(double actual, double expected) {
  if (std::isnan(actual)) {
    return false;
  }
  const auto difference =
      static_cast<uint64_t>(orderedBits(actual)) - static_cast<uint64_t>(orderedBits(expected));
  return difference <= 1 || difference == ~uint64_t{0};
}

std::string hex(double value) {
  std::array<char, 64> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::hex);
  return {buffer.data(), result.ptr};
}

void checkAgainst(std::span<const SimMathReference> table, double (*function)(double)) {
  for (const SimMathReference &entry : table) {
    const double actual = function(entry.Argument);
    INFO("argument " << hex(entry.Argument) << ", expected " << hex(entry.Expected) << ", got "
                     << hex(actual));
    CHECK(withinOneUlp(actual, entry.Expected));
  }
}

bool isPositiveZero(double value) { return value == 0.0 && !std::signbit(value); }

TEST_CASE("simExp is within one ULP of the correctly rounded e^x") {
  checkAgainst(EXP_REFERENCE, simExp);
}

TEST_CASE("simLog is within one ULP of the correctly rounded ln x") {
  checkAgainst(LOG_REFERENCE, simLog);
}

TEST_CASE("simExp returns exactly 1 for zero of either sign") {
  CHECK(simExp(0.0) == 1.0);
  CHECK(simExp(-0.0) == 1.0);
}

TEST_CASE("simExp returns +infinity for +infinity and for any argument of 710 or more") {
  CHECK(simExp(710.0) == INF);
  CHECK(simExp(LARGEST) == INF);
  CHECK(simExp(INF) == INF);
}

TEST_CASE("simExp returns +0 for -infinity and for any argument of -746 or less") {
  CHECK(isPositiveZero(simExp(-746.0)));
  CHECK(isPositiveZero(simExp(-LARGEST)));
  CHECK(isPositiveZero(simExp(-INF)));
}

TEST_CASE("simExp returns a NaN for a NaN") { CHECK(std::isnan(simExp(NOT_A_NUMBER))); }

TEST_CASE("simLog returns -infinity for zero of either sign") {
  CHECK(simLog(0.0) == -INF);
  CHECK(simLog(-0.0) == -INF);
}

TEST_CASE("simLog returns +infinity for +infinity") { CHECK(simLog(INF) == INF); }

TEST_CASE("simLog returns +0 for 1") { CHECK(isPositiveZero(simLog(1.0))); }

// The negative of the smallest subnormal is the argument nearest zero from below.
TEST_CASE("simLog returns a NaN for any argument below zero") {
  CHECK(std::isnan(simLog(-SMALLEST_SUBNORMAL)));
  CHECK(std::isnan(simLog(-1.0)));
  CHECK(std::isnan(simLog(-INF)));
}

TEST_CASE("simLog returns a NaN for a NaN") { CHECK(std::isnan(simLog(NOT_A_NUMBER))); }

} // namespace
} // namespace tpj
