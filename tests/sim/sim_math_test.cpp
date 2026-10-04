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

TEST_CASE("simExp is within one ULP of the correctly rounded e^x") {
  checkAgainst(EXP_REFERENCE, simExp);
}

TEST_CASE("simLog is within one ULP of the correctly rounded ln x") {
  checkAgainst(LOG_REFERENCE, simLog);
}

// True when actual is expected, a NaN when expected is one, and a zero of expected's sign.
bool sameSpecialValue(double actual, double expected) {
  if (std::isnan(expected)) {
    return std::isnan(actual);
  }
  return actual == expected && std::signbit(actual) == std::signbit(expected);
}

struct SpecialValue {
  double (*Function)(double);
  const char *Name;
  double Argument;
  double Expected;
};

TEST_CASE("simExp and simLog return IEEE's special values") {
  // Zeros of both signs, the infinities, a NaN, for simExp arguments whose results overflow to
  // infinity and underflow to zero, and for simLog the argument nearest zero from below.
  const std::array<SpecialValue, 13> cases = {{
      {simExp, "simExp", 0.0, 1.0},
      {simExp, "simExp", -0.0, 1.0},
      {simExp, "simExp", 710.0, INF},
      {simExp, "simExp", INF, INF},
      {simExp, "simExp", -746.0, 0.0},
      {simExp, "simExp", -INF, 0.0},
      {simExp, "simExp", NOT_A_NUMBER, NOT_A_NUMBER},
      {simLog, "simLog", 0.0, -INF},
      {simLog, "simLog", -0.0, -INF},
      {simLog, "simLog", INF, INF},
      {simLog, "simLog", 1.0, 0.0},
      {simLog, "simLog", -SMALLEST_SUBNORMAL, NOT_A_NUMBER},
      {simLog, "simLog", NOT_A_NUMBER, NOT_A_NUMBER},
  }};
  for (const SpecialValue &special : cases) {
    const double actual = special.Function(special.Argument);
    INFO(special.Name << "(" << hex(special.Argument) << "), expected " << hex(special.Expected)
                      << ", got " << hex(actual));
    CHECK(sameSpecialValue(actual, special.Expected));
  }
}

} // namespace
} // namespace tpj
