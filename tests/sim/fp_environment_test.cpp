#include "support/synthetic_types.h"

#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cfenv>
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>

#if defined(__SSE2__)
#include <pmmintrin.h>
#include <xmmintrin.h>
#endif

namespace tpj {
namespace {

using test::buildWorld;
using test::makeSchema;

constexpr uint64_t SEED = 12345;

// Ways the thread's floating-point environment can differ from the default.
enum class Deviation {
  RoundUpward,
  RoundDownward,
  RoundTowardZero,
  FlushToZero,
  DenormalsAreZero,
};

// Puts back the thread's floating-point environment, including the SSE flush-to-zero and
// denormals-are-zero controls, when it goes out of scope.
class EnvironmentGuard {
public:
  EnvironmentGuard() {
    std::fegetenv(&Saved);
#if defined(__SSE2__)
    SavedControl = _mm_getcsr();
#endif
  }
  EnvironmentGuard(const EnvironmentGuard &) = delete;
  EnvironmentGuard &operator=(const EnvironmentGuard &) = delete;
  EnvironmentGuard(EnvironmentGuard &&) = delete;
  EnvironmentGuard &operator=(EnvironmentGuard &&) = delete;
  ~EnvironmentGuard() {
    std::fesetenv(&Saved);
#if defined(__SSE2__)
    _mm_setcsr(SavedControl);
#endif
  }

private:
  std::fenv_t Saved{};
#if defined(__SSE2__)
  unsigned int SavedControl = 0;
#endif
};

// Flush-to-zero and denormals-are-zero are controls outside <cfenv>, which the test sets only
// where it knows them.
#if defined(__SSE2__)
constexpr bool HAS_SUBNORMAL_CONTROLS = true;
#else
constexpr bool HAS_SUBNORMAL_CONTROLS = false;
#endif

void apply(Deviation deviation) {
  switch (deviation) {
  case Deviation::RoundUpward:
    REQUIRE(std::fesetround(FE_UPWARD) == 0);
    break;
  case Deviation::RoundDownward:
    REQUIRE(std::fesetround(FE_DOWNWARD) == 0);
    break;
  case Deviation::RoundTowardZero:
    REQUIRE(std::fesetround(FE_TOWARDZERO) == 0);
    break;
  case Deviation::FlushToZero:
#if defined(__SSE2__)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
    break;
  case Deviation::DenormalsAreZero:
#if defined(__SSE2__)
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    break;
  }
}

// True if the call throws WorldInvariantError while the environment deviates. The environment is
// restored before this returns, whatever the call does.
bool refusedUnder(Deviation deviation, const std::function<void()> &call) {
  const EnvironmentGuard guard;
  apply(deviation);
  try {
    call();
  } catch (const WorldInvariantError &) {
    return true;
  }
  return false;
}

void requireWorldChecks() {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
}

void requireSubnormalControls() {
  if (!HAS_SUBNORMAL_CONTROLS) {
    SKIP("this target has no flush-to-zero or denormals-are-zero control the test can set");
  }
}

void constructDirectly() { const World world(makeSchema(), SEED); }

TEST_CASE("constructing a World succeeds in the default floating-point environment") {
  SECTION("with the default constructor") { REQUIRE_NOTHROW(World()); }
  SECTION("with a schema and a seed") { REQUIRE_NOTHROW(constructDirectly()); }
  SECTION("by copyWorld") {
    const World original = buildWorld(makeSchema());
    REQUIRE_NOTHROW(copyWorld(original));
  }
  SECTION("by loadWorld") {
    const std::string text = saveWorld(buildWorld(makeSchema()));
    REQUIRE_NOTHROW(loadWorld(makeSchema(), text));
  }
}

// Each direction is its own case because an arithmetic probe can tell one direction from
// to-nearest and still miss another.
TEST_CASE("in debug builds, constructing a World throws unless rounding is to nearest") {
  requireWorldChecks();
  Deviation deviation = Deviation::RoundUpward;
  SECTION("upward") { deviation = Deviation::RoundUpward; }
  SECTION("downward") { deviation = Deviation::RoundDownward; }
  SECTION("toward zero") { deviation = Deviation::RoundTowardZero; }
  REQUIRE(refusedUnder(deviation, constructDirectly));
}

TEST_CASE("in debug builds, constructing a World throws when subnormal results flush to zero") {
  requireWorldChecks();
  requireSubnormalControls();
  REQUIRE(refusedUnder(Deviation::FlushToZero, constructDirectly));
}

TEST_CASE("in debug builds, constructing a World throws when subnormal operands read as zero") {
  requireWorldChecks();
  requireSubnormalControls();
  REQUIRE(refusedUnder(Deviation::DenormalsAreZero, constructDirectly));
}

TEST_CASE("in debug builds, every way of constructing a World checks the environment") {
  requireWorldChecks();
  const std::shared_ptr<const WorldSchema> schema = makeSchema();
  const World original = buildWorld(schema);
  const std::string text = saveWorld(original);

  std::function<void()> construct;
  SECTION("the default constructor") {
    construct = [] { const World world; };
  }
  SECTION("copyWorld") {
    construct = [&] { static_cast<void>(copyWorld(original)); };
  }
  SECTION("loadWorld") {
    construct = [&] { static_cast<void>(loadWorld(schema, text)); };
  }
  REQUIRE(refusedUnder(Deviation::RoundUpward, construct));
}

} // namespace
} // namespace tpj
