# Implementation Plan: Place Carry-Over

## Goal

Add carryOver and nearestPlaceOn beside the network type in src/sim/medium, so a holder can move its places across a re-derivation.

## Approach

nearestPlace's per-segment projection moves into a helper that projects onto one carrier and keeps a candidate only when it is strictly nearer. nearestPlace runs it over every carrier in key order, and nearestPlaceOn runs it over one, so both share the arithmetic and the tie rule. carryOver is a free function over the networks' public functions: groundPoint of the place before, a key lookup in each network's carriers(), a comparison of the two carriers' points, and nearestPlaceOn when they differ. CarrierPoint gains a defaulted operator== for that comparison.

## Tasks

### Task 1: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: Add the nearestPlaceOn paragraph after the nearestPlace paragraph, and the Carry-over section before the addNetworkComponent paragraph, with the exact text in FEATURE.md's Spec changes. The Carry-over section's heading is `## Carry-over`, and the addNetworkComponent paragraph stays last, under it.

### Task 2: Declare the operations

Files:
- Modify: `src/sim/medium/network.h`

Step 1: Give CarrierPoint a defaulted equality, after its Distance member:

```cpp
  bool operator==(const CarrierPoint &) const = default;
```

Step 2: In class Network, after nearestPlace's declaration, add:

```cpp
  // The place on the carrier whose ground position is nearest the point, ties to the lower
  // distance. None when the carrier is not in the network or the point is not finite.
  [[nodiscard]] std::optional<Place> nearestPlaceOn(EntityKey carrier, GroundPoint point) const;
```

Step 3: After class Network and before addNetworkComponent's declaration, add:

```cpp
// Moves a place held across a re-derivation from the network before to the network after: kept
// when its carrier's points are unchanged, moved to the carrier's nearest point to its old ground
// position when they changed, and none when it did not resolve before or its carrier is gone.
[[nodiscard]] std::optional<Place> carryOver(const Place &place, const Network &before,
                                             const Network &after);
```

### Task 3: Stub the operations

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: After nearestPlace's definition, add the stubs:

```cpp
std::optional<Place> Network::nearestPlaceOn(EntityKey /*carrier*/, GroundPoint /*point*/) const {
  static_cast<void>(this); // Stub until implemented.
  return std::nullopt;
}

std::optional<Place> carryOver(const Place & /*place*/, const Network & /*before*/,
                               const Network & /*after*/) {
  return std::nullopt; // Stub until implemented.
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 139 existing tests pass.

### Task 4: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, and the public header src/sim/medium/network.h. It creates tests/sim/medium/carry_over_test.cpp, extends tests/sim/support/synthetic_network.h as it needs, and adds the test file to tpj_sim_tests in tests/sim/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 139 existing tests pass. The new tests of unchanged carriers, changed carriers, and nearestPlaceOn fail against the stubs. The "no place" tests of criterion 1 and of nearestPlaceOn may pass vacuously.

### Task 5: Share the projection and add nearestPlaceOn

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: In the anonymous namespace, after requireValidAnchors, add a carrier lookup and the projection helper. The helper's body is the inner loop of the current nearestPlace, unchanged.

```cpp
// The carrier with the key, or null when there is none.
const Carrier *findCarrier(const std::vector<Carrier> &carriers, EntityKey key) {
  const auto found = std::ranges::lower_bound(carriers, key, {}, &Carrier::Key);
  return found != carriers.end() && found->Key == key ? &*found : nullptr;
}

// Projects the point onto each segment of the carrier, replacing best only with a strictly nearer
// projection, so earlier carriers and then lower distances win ties.
void projectOnto(const Carrier &carrier, GroundPoint point, std::optional<Place> &best,
                 double &bestSquared) {
  for (size_t i = 0; i + 1 < carrier.Points.size(); ++i) {
    const CarrierPoint &a = carrier.Points[i];
    const CarrierPoint &b = carrier.Points[i + 1];
    const double dx = b.X - a.X;
    const double dz = b.Z - a.Z;
    const double lengthSquared = dx * dx + dz * dz;
    double t = 0.0;
    if (lengthSquared > 0.0) {
      t = std::clamp(((point.X - a.X) * dx + (point.Z - a.Z) * dz) / lengthSquared, 0.0, 1.0);
    }
    // A segment's ends are exact, so a junction is equally near on every carrier stopping there
    // and the tie rule decides between them.
    CarrierPoint projected{a.X + t * dx, a.Z + t * dz, a.Distance + t * (b.Distance - a.Distance)};
    if (t == 0.0) {
      projected = a;
    } else if (t == 1.0) {
      projected = b;
    }
    const double offX = point.X - projected.X;
    const double offZ = point.Z - projected.Z;
    const double squared = offX * offX + offZ * offZ;
    if (!best || squared < bestSquared) {
      best = Place{carrier.Key, projected.Distance};
      bestSquared = squared;
    }
  }
}
```

Step 2: Replace nearestPlace's body, and the nearestPlaceOn stub, with:

```cpp
std::optional<Place> Network::nearestPlace(GroundPoint point) const {
  if (!isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  for (const Carrier &carrier : Carriers) {
    projectOnto(carrier, point, best, bestSquared);
  }
  return best;
}

std::optional<Place> Network::nearestPlaceOn(EntityKey carrier, GroundPoint point) const {
  const Carrier *found = findCarrier(Carriers, carrier);
  if (found == nullptr || !isFinite(point.X) || !isFinite(point.Z)) {
    return std::nullopt;
  }
  std::optional<Place> best;
  double bestSquared = 0.0;
  projectOnto(*found, point, best, bestSquared);
  return best;
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, the 139 earlier tests still pass, and the nearestPlaceOn tests pass.

### Task 6: Carry places over

Files:
- Modify: `src/sim/medium/network.cpp`

Step 1: Replace carryOver's stub with:

```cpp
std::optional<Place> carryOver(const Place &place, const Network &before, const Network &after) {
  const std::optional<GroundPoint> ground = before.groundPoint(place);
  const Carrier *now = findCarrier(after.carriers(), place.Carrier);
  if (!ground || now == nullptr) {
    return std::nullopt;
  }
  // The place resolves before, so its carrier is there. Stops are not geometry, so a split keeps it.
  const Carrier *was = findCarrier(before.carriers(), place.Carrier);
  if (was != nullptr && was->Points == now->Points) {
    return place;
  }
  return after.nearestPlaceOn(place.Carrier, *ground);
}
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build is clean, and every test passes.

### Task 7: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: the script prints its stages and passes, with the scenarios' output unchanged, since no scenario uses the medium yet.

### Task 8: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Medium: Carry places across a re-derivation`.
