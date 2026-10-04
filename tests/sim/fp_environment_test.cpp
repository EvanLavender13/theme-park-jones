#include "support/synthetic_types.h"

#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cfenv>
#include <string>
#include <vector>

#if defined(__SSE2__)
#include <pmmintrin.h>
#include <xmmintrin.h>
#endif

namespace tpj {
namespace {

using test::buildWorld;
using test::makeSchema;

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

// The deviations this target can set. Flush-to-zero and denormals-are-zero are controls outside
// <cfenv>, set only where the test knows them.
std::vector<Deviation> settableDeviations() {
  std::vector<Deviation> deviations = {Deviation::RoundUpward, Deviation::RoundDownward,
                                       Deviation::RoundTowardZero};
#if defined(__SSE2__)
  deviations.push_back(Deviation::FlushToZero);
  deviations.push_back(Deviation::DenormalsAreZero);
#endif
  return deviations;
}

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

// The check works by arithmetic, and a probe that tells one rounding direction from to-nearest can
// still miss another, so each rounding direction and each subnormal control is its own input.
TEST_CASE("in debug builds, constructing a World by any route refuses a non-default "
          "floating-point environment") {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
  const auto schema = makeSchema();
  const std::string text = saveWorld(buildWorld(schema));

  // A load builds its world from a file rather than from a seed.
  for (const Deviation deviation : settableDeviations()) {
    CAPTURE(deviation);
    const EnvironmentGuard guard;
    apply(deviation);
    CHECK_THROWS_AS(loadWorld(schema, text), WorldInvariantError);
  }
}

} // namespace
} // namespace tpj
