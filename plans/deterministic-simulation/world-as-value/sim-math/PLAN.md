# Implementation Plan: Sim Math

## Goal

Give tpj_sim its own exp and log as ports of musl's, a CMake check that its archive calls no C runtime transcendental function, and a debug-build check at world construction that the floating-point environment is the default.

## Approach

simExp and simLog are musl v1.2.5's exp and log transcribed into C++, one file each with Arm's MIT notice and its tables. musl's helpers become std::bit_cast and plain expressions, and its exception-raising helpers become the values they return. The build takes musl's non-FMA path, which is the one its data file's second log table serves, and keeps musl's default WANT_ROUNDING branches, which change nothing in round-to-nearest (RESEARCH.md). The symbol check is a cmake -P script over the toolchain's nm, so it runs on both builds. The environment check is an arithmetic probe through volatile operands, compared by bits, because a comparison would itself read a subnormal operand as zero when denormals-are-zero is set.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Replace the final paragraph, which begins "Stepping is deterministic", with the three paragraphs in FEATURE.md's "Spec changes" section, exactly.

### Task 2: Declare the simulation's math

Files:
- Create: `src/sim/sim_math.h`

Step 1: Create the header.

```cpp
#ifndef TPJ_SIM_SIM_MATH_H
#define TPJ_SIM_SIM_MATH_H

namespace tpj {

// e^x, ported from musl's exp: within one ULP of the correctly rounded result, and the same bits
// on every build. Simulation code calls it, never std::exp.
double simExp(double x);

// The natural logarithm, ported from musl's log: within one ULP of the correctly rounded result,
// and the same bits on every build. Simulation code calls it, never std::log.
double simLog(double x);

} // namespace tpj

#endif
```

### Task 3: Stub the ports and the check, and build them

Files:
- Create: `src/sim/musl_exp.cpp`
- Create: `src/sim/musl_log.cpp`
- Create: `cmake/check_sim_symbols.cmake`
- Modify: `src/sim/CMakeLists.txt:3-9`

Step 1: Create `src/sim/musl_exp.cpp` as a stub, replaced in Task 5.

```cpp
#include "sim/sim_math.h"

namespace tpj {

double simExp(double /*x*/) { return 0.0; }

} // namespace tpj
```

Step 2: Create `src/sim/musl_log.cpp` as a stub, replaced in Task 6.

```cpp
#include "sim/sim_math.h"

namespace tpj {

double simLog(double /*x*/) { return 0.0; }

} // namespace tpj
```

Step 3: Create `cmake/check_sim_symbols.cmake` as a stub that accepts every archive, replaced in Task 8.

```cmake
# Fails when a static archive references a C runtime transcendental function. Stub until the
# check is implemented.
```

Step 4: Add the two sources to tpj_sim, keeping the list sorted.

```cmake
add_library(tpj_sim STATIC
    cycle.cpp
    draw.cpp
    musl_exp.cpp
    musl_log.cpp
    save.cpp
    schema.cpp
    walk.cpp
    world.cpp)
```

Step 5: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 74 existing tests pass.

### Task 4: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/SPEC.md, and docs/principles.md. It writes the reference table and its generating script under tests/sim/support/, the planted object and its static library, and the tests, and registers the two symbol-check tests in tests/sim/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 74 existing tests pass. The reference-table and special-value tests fail against the stubs, the environment tests fail because no World refuses a changed environment, and the planted-symbol test fails because the stub check accepts everything. The test that tpj_sim passes the check may pass against the stub.

### Task 5: Port exp

Files:
- Modify: `src/sim/musl_exp.cpp` (replace the stub)

Step 1: Fetch musl's exp data, and check that its table is the 128 lines this step expects, each with two entries.

Run: `curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/exp_data.c?h=v1.2.5' | sed -n '53,180p' | grep -c '^0x[0-9a-f]*, 0x[0-9a-f]*,$'`
Expected: `128`

Step 2: Replace the file with the port below. Where the table's comment marks the entries, paste the 128 lines printed by `curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/exp_data.c?h=v1.2.5' | sed -n '53,180p'`, unchanged and in order.

```cpp
// Ported from musl v1.2.5's src/math/exp.c and src/math/exp_data.c, which are Arm's optimized
// routines. Changes from musl: C++ in namespace tpj; asuint64 and asdouble are std::bit_cast; the
// helpers that raise floating-point exceptions return their results directly, and those that only
// force exceptions are dropped; the build's non-intrinsic, non-narrow rounding path is the only
// one kept; exp2's data is dropped.
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "sim/sim_math.h"

#include <array>
#include <bit>
#include <limits>
#include <stdint.h>

namespace tpj {

namespace {

constexpr uint64_t EXP_TABLE_BITS = 7;
constexpr uint64_t N = uint64_t{1} << EXP_TABLE_BITS;
constexpr double INV_LN2_N = 0x1.71547652b82fep0 * static_cast<double>(N);
constexpr double NEG_LN2_HI_N = -0x1.62e42fefa0000p-8;
constexpr double NEG_LN2_LO_N = -0x1.cf79abc9e3b3ap-47;
constexpr double SHIFT = 0x1.8p52;
// exp polynomial coefficients.
constexpr double C2 = 0x1.ffffffffffdbdp-2;
constexpr double C3 = 0x1.555555555543cp-3;
constexpr double C4 = 0x1.55555cf172b91p-5;
constexpr double C5 = 0x1.1111167a4d017p-7;

// 2^(k/N) ~= H[k]*(1 + T[k]) for integer k in [0, N): entry 2k holds T[k]'s bits, and entry 2k+1
// holds H[k]'s bits minus (k << 52)/N.
constexpr std::array<uint64_t, 2 * N> T = {
    // The 128 lines of musl's exp_data.c .tab, lines 53 to 180.
};

// The top 12 bits of a double: its sign and exponent.
constexpr uint32_t top12(double x) { return static_cast<uint32_t>(std::bit_cast<uint64_t>(x) >> 52U); }

// scale*(1 + tmp) where that may overflow or underflow, without intermediate rounding. sbits holds
// scale's bits, with a computed exponent that may have overflowed into the sign bit; ki's low 32
// bits are the reduction's k, positive when the result may overflow and negative when it may
// underflow.
double specialCase(double tmp, uint64_t sbits, uint64_t ki) {
  if ((ki & 0x80000000U) == 0) {
    // k > 0: the exponent of scale may have overflowed by up to 460.
    sbits -= 1009ULL << 52U;
    const double scale = std::bit_cast<double>(sbits);
    return 0x1p1009 * (scale + scale * tmp);
  }
  // k < 0: take care in the subnormal range.
  sbits += 1022ULL << 52U;
  const double scale = std::bit_cast<double>(sbits);
  double y = scale + scale * tmp;
  if (y < 1.0) {
    // Round y to the right precision before scaling it into the subnormal range, so it is rounded
    // once.
    double lo = scale - y + scale * tmp;
    const double hi = 1.0 + y;
    lo = 1.0 - hi + y + lo;
    y = (hi + lo) - 1.0;
    // musl's guard against -0 in downward rounding, kept as musl builds it.
    if (y == 0.0) {
      y = 0.0;
    }
  }
  return 0x1p-1022 * y;
}

} // namespace

double simExp(double x) {
  uint32_t abstop = top12(x) & 0x7ffU;
  if (abstop - top12(0x1p-54) >= top12(512.0) - top12(0x1p-54)) {
    if (abstop - top12(0x1p-54) >= 0x80000000U) {
      // Tiny x, including 0: e^x rounds to 1, and 1 + x avoids a spurious underflow.
      return 1.0 + x;
    }
    if (abstop >= top12(1024.0)) {
      if (std::bit_cast<uint64_t>(x) ==
          std::bit_cast<uint64_t>(-std::numeric_limits<double>::infinity())) {
        return 0.0;
      }
      if (abstop >= top12(std::numeric_limits<double>::infinity())) {
        // +infinity or NaN.
        return 1.0 + x;
      }
      if ((std::bit_cast<uint64_t>(x) >> 63U) != 0) {
        // musl's __math_uflow(0).
        return 0.0;
      }
      // musl's __math_oflow(0).
      return std::numeric_limits<double>::infinity();
    }
    // Large x is special cased below.
    abstop = 0;
  }

  // exp(x) = 2^(k/N) * exp(r), with exp(r) in [2^(-1/2N), 2^(1/2N)], where x = ln2/N*k + r with
  // integer k and r in [-ln2/2N, ln2/2N].
  const double z = INV_LN2_N * x;
  // Adding SHIFT rounds z to an integer held in the sum's low bits; z - kd is in [-1, 1] in
  // non-nearest rounding modes.
  double kd = z + SHIFT;
  const uint64_t ki = std::bit_cast<uint64_t>(kd);
  kd -= SHIFT;
  const double r = x + kd * NEG_LN2_HI_N + kd * NEG_LN2_LO_N;
  // 2^(k/N) ~= scale * (1 + tail).
  const uint64_t idx = 2 * (ki % N);
  const uint64_t top = ki << (52U - EXP_TABLE_BITS);
  const double tail = std::bit_cast<double>(T[idx]);
  // This is only a valid scale when -1023*N < k < 1024*N.
  const uint64_t sbits = T[idx + 1] + top;
  // exp(x) = 2^(k/N) * exp(r) ~= scale + scale * (tail + exp(r) - 1).
  const double r2 = r * r;
  // Without fma the worst case error is 0.25/N ulp larger.
  const double tmp = tail + r + r2 * (C2 + r * C3) + r2 * r2 * (C4 + r * C5);
  if (abstop == 0) {
    return specialCase(tmp, sbits, ki);
  }
  const double scale = std::bit_cast<double>(sbits);
  // tmp == 0 or |tmp| > 2^-200 and scale > 2^-739, so there is no spurious underflow here.
  return scale + scale * tmp;
}

} // namespace tpj
```

Step 3: Build and test. If clang-tidy asks for const on a local or another mechanical change, make it and note it here. Never reorder an arithmetic expression or split one into a different order of operations: the order is the result.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the reference-table and special-value tests pass for simExp. simLog's still fail.

### Task 6: Port log

Files:
- Modify: `src/sim/musl_log.cpp` (replace the stub)

Step 1: Fetch musl's log data, and check that both tables are the 128 lines this step expects, each one entry in braces.

Run: `curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/log_data.c?h=v1.2.5' | sed -n '67,194p' | grep -c '^{.*},$'; curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/log_data.c?h=v1.2.5' | sed -n '198,325p' | grep -c '^{.*},$'`
Expected: `128` twice.

Step 2: Replace the file with the port below. Where T's comment marks the entries, paste the 128 lines printed by `curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/log_data.c?h=v1.2.5' | sed -n '67,194p'`, and where T2's comment marks them, the 128 lines printed by `curl -sSf 'https://git.musl-libc.org/cgit/musl/plain/src/math/log_data.c?h=v1.2.5' | sed -n '198,325p'`, each unchanged and in order.

```cpp
// Ported from musl v1.2.5's src/math/log.c and src/math/log_data.c, which are Arm's optimized
// routines. Changes from musl: C++ in namespace tpj; asuint64 and asdouble are std::bit_cast; the
// helpers that raise floating-point exceptions return their results directly; the build's path
// without fused multiply-add is the only one kept.
//
// Copyright (c) 2018, Arm Limited.
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
// BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "sim/sim_math.h"

#include <array>
#include <bit>
#include <limits>
#include <stddef.h>
#include <stdint.h>

namespace tpj {

namespace {

constexpr uint64_t LOG_TABLE_BITS = 7;
constexpr uint64_t N = uint64_t{1} << LOG_TABLE_BITS;
constexpr uint64_t OFF = 0x3fe6000000000000;
constexpr double LN2_HI = 0x1.62e42fefa3800p-1;
constexpr double LN2_LO = 0x1.ef35793c76730p-45;

// log1p(r) - r ~= r^2*A[0] + ... for |r| < 1/2N; the first coefficient, 1, is implicit.
constexpr std::array<double, 5> A = {
    -0x1.0000000000001p-1, 0x1.555555551305bp-2, -0x1.fffffffeb459p-3,
    0x1.999b324f10111p-3,  -0x1.55575e506c89fp-3,
};

// log(1 + r) ~= r + r^2*B[0] + ... for x near 1; B[0] is -0.5.
constexpr std::array<double, 11> B = {
    -0x1p-1,
    0x1.5555555555577p-2,
    -0x1.ffffffffffdcbp-3,
    0x1.999999995dd0cp-3,
    -0x1.55555556745a7p-3,
    0x1.24924a344de3p-3,
    -0x1.fffffa4423d65p-4,
    0x1.c7184282ad6cap-4,
    -0x1.999eb43b068ffp-4,
    0x1.78182f7afd085p-4,
    -0x1.5521375d145cdp-4,
};

// For the subinterval of [OFF, 2*OFF) holding z, with c near its center: 1/c and log(c).
struct LogEntry {
  double InvC;
  double LogC;
};

// c split into a high part and the rounding error left over, so that z/c - 1 is computed with
// close to one rounding without fused multiply-add.
struct LogSplit {
  double CHigh;
  double CLow;
};

constexpr std::array<LogEntry, N> T = {{
    // The 128 lines of musl's log_data.c .tab, lines 67 to 194.
}};

constexpr std::array<LogSplit, N> T2 = {{
    // The 128 lines of musl's log_data.c .tab2, lines 198 to 325.
}};

// The top 16 bits of a double.
constexpr uint32_t top16(double x) { return static_cast<uint32_t>(std::bit_cast<uint64_t>(x) >> 48U); }

} // namespace

double simLog(double x) {
  uint64_t ix = std::bit_cast<uint64_t>(x);
  const uint32_t top = top16(x);
  constexpr uint64_t LO = std::bit_cast<uint64_t>(1.0 - 0x1p-4);
  constexpr uint64_t HI = std::bit_cast<uint64_t>(1.0 + 0x1.09p-4);
  if (ix - LO < HI - LO) {
    // Close to 1.0: handled separately.
    if (ix == std::bit_cast<uint64_t>(1.0)) {
      // musl's fix for the sign of zero in downward rounding, kept as musl builds it.
      return 0.0;
    }
    const double r = x - 1.0;
    const double r2 = r * r;
    const double r3 = r * r2;
    double y = r3 * (B[1] + r * B[2] + r2 * B[3] +
                     r3 * (B[4] + r * B[5] + r2 * B[6] + r3 * (B[7] + r * B[8] + r2 * B[9] + r3 * B[10])));
    // Worst-case error is around 0.507 ULP.
    double w = r * 0x1p27;
    const double rhi = r + w - w;
    const double rlo = r - rhi;
    w = rhi * rhi * B[0];
    const double hi = r + w;
    double lo = r - hi + w;
    lo += B[0] * rlo * (rhi + r);
    y += lo;
    y += hi;
    return y;
  }
  if (top - 0x0010U >= 0x7ff0U - 0x0010U) {
    // x < 0x1p-1022, or infinity, or NaN.
    if (ix * 2 == 0) {
      // musl's __math_divzero(1).
      return -std::numeric_limits<double>::infinity();
    }
    if (ix == std::bit_cast<uint64_t>(std::numeric_limits<double>::infinity())) {
      return x;
    }
    if ((top & 0x8000U) != 0 || (top & 0x7ff0U) == 0x7ff0U) {
      // musl's __math_invalid(x).
      return std::numeric_limits<double>::quiet_NaN();
    }
    // x is subnormal: normalize it.
    ix = std::bit_cast<uint64_t>(x * 0x1p52);
    ix -= 52ULL << 52U;
  }

  // x = 2^k z, where z is in [OFF, 2*OFF) and exact. The range is split into N subintervals, and
  // the ith one holds z, with c near its center.
  const uint64_t tmp = ix - OFF;
  const auto i = static_cast<size_t>((tmp >> (52U - LOG_TABLE_BITS)) % N);
  // An arithmetic shift, which C++20 defines for negative values.
  const int64_t k = static_cast<int64_t>(tmp) >> 52U;
  const uint64_t iz = ix - (tmp & (0xfffULL << 52U));
  const double invc = T[i].InvC;
  const double logc = T[i].LogC;
  const double z = std::bit_cast<double>(iz);

  // log(x) = log1p(z/c - 1) + log(c) + k*Ln2, with r ~= z/c - 1 and |r| < 1/2N. The rounding
  // error is 0x1p-55/N + 0x1p-66.
  const double r = (z - T2[i].CHigh - T2[i].CLow) * invc;
  const auto kd = static_cast<double>(k);

  // hi + lo = r + log(c) + k*Ln2.
  const double w = kd * LN2_HI + logc;
  const double hi = w + r;
  const double lo = w - hi + r + kd * LN2_LO;

  // log(x) = lo + (log1p(r) - r) + hi.
  const double r2 = r * r;
  return lo + r2 * A[0] + r * r2 * (A[1] + r * A[2] + r2 * (A[3] + r * A[4])) + hi;
}

} // namespace tpj
```

Step 3: Build and test, with the same rule as Task 5 for clang-tidy's requests.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the reference-table and special-value tests pass for both functions.

### Task 7: Check the floating-point environment at world construction

Files:
- Modify: `src/sim/world.cpp:1-16`
- Modify: `src/sim/world.h:32-33`

Step 1: In `src/sim/world.cpp`, add `#include <bit>`, `#include <limits>`, and `#include <stdint.h>` to the system includes, keeping them sorted, and add the probe in an anonymous namespace inside namespace tpj, before `World::World()`.

```cpp
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
```

Step 2: In the `World(std::shared_ptr<const WorldSchema> schema, uint64_t seed)` constructor, after the schema check, add:

```cpp
  if (WORLD_CHECKS && !isDefaultFloatEnvironment()) {
    throw WorldInvariantError("the floating-point environment is not the default: round-to-nearest, "
                              "with subnormals neither flushed to zero nor read as zero");
  }
```

Step 3: In `src/sim/world.h`, replace the comment above WorldInvariantError with:

```cpp
// A world the walk cannot cover fully: an unregistered component, an entity without a key, a NaN
// in registered state, or two derived origins sharing a key. Also thrown when a world is created
// under a floating-point environment that would change the simulation's arithmetic.
```

Step 4: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the environment tests pass.

Note from implementing: misc-const-correctness asked for the probe's volatile operands to be const volatile, which still forces each read, so the step's code now declares them that way.

### Task 8: The symbol check

Files:
- Modify: `cmake/check_sim_symbols.cmake` (replace the stub)

Step 1: Replace the file with the check.

```cmake
# Fails when a static archive references a C runtime transcendental function, whose results differ
# between the builds' runtimes (decision 0022). Exactly rounded functions such as sqrt, floor, and
# fmod stay allowed: GCC calls them legitimately, and every runtime gives the same result.
#
# Usage: cmake -DNM=<the toolchain's nm> -DARCHIVE=<archive> -P check_sim_symbols.cmake
# On failure it prints a line "<symbol> referenced by <object>" for each denied symbol.

# A -P script takes its policies from here; IN_LIST needs CMP0057.
cmake_minimum_required(VERSION 3.28)

if(NOT NM OR NOT ARCHIVE)
    message(FATAL_ERROR "usage: cmake -DNM=<nm> -DARCHIVE=<archive> -P check_sim_symbols.cmake")
endif()

set(TRANSCENDENTALS
    exp exp2 exp10 expm1 log log2 log10 log1p pow
    sin cos tan asin acos atan atan2 sinh cosh tanh asinh acosh atanh sincos
    cbrt hypot erf erfc tgamma lgamma
    cexp clog cpow csqrt csin ccos ctan casin cacos catan csinh ccosh ctanh casinh cacosh catanh
    cabs carg)
set(DENIED)
foreach(name IN LISTS TRANSCENDENTALS)
    foreach(variant ${name} ${name}f ${name}l)
        list(APPEND DENIED ${variant} __${variant}_finite)
    endforeach()
endforeach()
foreach(variant lgamma_r lgammaf_r lgammal_r)
    list(APPEND DENIED ${variant} __${variant}_finite)
endforeach()

execute_process(COMMAND ${NM} -u ${ARCHIVE}
    OUTPUT_VARIABLE listing ERROR_VARIABLE errors RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${NM} -u ${ARCHIVE} failed: ${errors}")
endif()

# nm prints each object's name followed by a colon, then one line per undefined symbol. MinGW
# may reference an imported function through an __imp_ symbol.
string(REPLACE "\n" ";" lines "${listing}")
set(object "")
set(report "")
foreach(line IN LISTS lines)
    if(line MATCHES "^(.+):$")
        set(object "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^ *[Uw] +(__imp_)?([A-Za-z0-9_]+)$")
        if(CMAKE_MATCH_2 IN_LIST DENIED)
            string(APPEND report "${CMAKE_MATCH_2} referenced by ${object}\n")
        endif()
    endif()
endforeach()

if(report)
    message(FATAL_ERROR "${ARCHIVE} references C runtime transcendental functions:\n${report}")
endif()
message(STATUS "${ARCHIVE} references no C runtime transcendental function")
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: every test passes, including both symbol-check tests.

### Task 9: Verify on both builds

Step 1: Format the changed sources first, so the pre-commit hook changes nothing and the pre-push build finds nothing to rebuild.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds and every test passes, including both symbol-check tests, run with MinGW's nm.

### Task 10: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Sim: Port musl's exp and log and check for C runtime math`.
