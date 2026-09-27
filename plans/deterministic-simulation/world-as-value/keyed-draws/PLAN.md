# Implementation Plan: Keyed Draws

## Goal

Give tpj_sim keyed random draws: 64 bits from a DrawKey, uniform doubles in [0, 1), and weighted picks from integer or double weights, each a pure function of the key.

## Approach

drawBits and drawUniform are constexpr functions in a new header, sim/draw.h, built on the Hasher that already serves the state hash and derived keys. The two drawPick overloads and drawKey live in sim/draw.cpp, so the header needs only a forward declaration of World. The integer pick uses Lemire's multiply-shift without rejection, with the high half of the product computed from four 32-bit partial products in standard C++, and the double pick walks a running sum that ends exactly at the total (RESEARCH.md).

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Insert the two paragraphs in FEATURE.md's "Spec changes" section exactly, after the paragraph that begins "makeCandidate copies a world" and before the paragraph that begins "Stepping is deterministic".

### Task 2: Declare the draws

Files:
- Create: `src/sim/draw.h`

Step 1: Create the header. drawUniform is its full definition; drawBits is a stub until Task 5.

```cpp
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
constexpr uint64_t drawBits(const DrawKey & /*key*/) { return 0; }

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
```

### Task 3: Build the new source

Files:
- Create: `src/sim/draw.cpp`
- Modify: `src/sim/CMakeLists.txt:3-7`

Step 1: Create `src/sim/draw.cpp` with stubs:

```cpp
#include "sim/draw.h"

#include "sim/world.h"

namespace tpj {

DrawKey drawKey(const World & /*world*/, EntityKey /*entity*/, uint64_t /*purpose*/,
                uint64_t /*index*/) {
  return {};
}

size_t drawPick(const DrawKey & /*key*/, std::span<const uint64_t> /*weights*/) { return 0; }

size_t drawPick(const DrawKey & /*key*/, std::span<const double> /*weights*/) { return 0; }

} // namespace tpj
```

Step 2: In `src/sim/CMakeLists.txt`, add `draw.cpp` to the tpj_sim sources, after `cycle.cpp`:

```cmake
add_library(tpj_sim STATIC
    cycle.cpp
    draw.cpp
    schema.cpp
    walk.cpp
    world.cpp)
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 51 existing tests pass.

### Task 4: Test pass

Dispatch the test-writer agent through implementing-features' test pass template. Feature: this directory's FEATURE.md. Specs: `src/sim/SPEC.md`. Public headers: `src/sim/draw.h`, `src/sim/mix.h`, `src/sim/entity_key.h`, `src/sim/world.h`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 51 existing tests pass. The new tests of the expected-draw table, drawKey, the uniform distribution's chi-square statistic, pick frequencies, and refused weights fail against the stubs. Tests of properties that any pure function satisfies, such as order independence, drawUniform's range, and an unchanged world hash, may pass against them.

### Task 5: Draw bits and draw keys

Files:
- Modify: `src/sim/draw.h`
- Modify: `src/sim/draw.cpp`

Step 1: In `src/sim/draw.h`, replace the drawBits stub with:

```cpp
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
```

Step 2: In `src/sim/draw.cpp`, replace the drawKey stub with:

```cpp
DrawKey drawKey(const World &world, EntityKey entity, uint64_t purpose, uint64_t index) {
  return DrawKey{world.Seed, entity, purpose, world.Tick, index};
}
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests of drawKey and the uniform distribution pass. The tests of pick frequencies and refused weights still fail, and so do any tests of the table that include its picks.

### Task 6: Integer picks

Files:
- Modify: `src/sim/draw.cpp`

Step 1: Add `#include <limits>` and `#include <stdexcept>` to the system includes, after a blank line following `#include "sim/world.h"`. After `namespace tpj {`, add:

```cpp
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
```

Step 2: Replace the integer drawPick stub with:

```cpp
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
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests of integer picks, their zero weights, their refused weights, and the table's integer picks pass. The tests of double picks still fail.

### Task 7: Double picks

Files:
- Modify: `src/sim/draw.cpp`

Step 1: Add `#include <cmath>` to the system includes, keeping them sorted. `<limits>` is already included from Task 6.

Step 2: Replace the double drawPick stub with:

```cpp
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
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: every test passes.

### Task 8: Verify on both builds

Step 1: Format the changed sources first, so the pre-commit hook changes nothing and the pre-push build finds nothing to rebuild.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds and every test passes, which shows the expected-draw table matches on the Windows build.

### Task 9: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Sim: Add keyed random draws and weighted picks`.
