# Implementation Plan: Ground Carry-Over

## Goal

Make carryOver keep a place on its ground when a carrier still passes over it, and keep a guest carried onto another carrier walking the same way over the ground, without changing today's simulation.

## Approach

carryOver keeps its early return for an unchanged carrier. Otherwise it tries, in order, the nearest place on the place's own carrier and the nearest place on any carrier, and accepts the first whose ground position lies within a millimeter of the old one. If neither does, it returns the nearest place on its own carrier, or none, as today. The guests' finisher compares directionAt before and after for a guest that carryOver moved to another carrier, and flips Guest::Forward when they point opposite ways.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping a place on surviving ground: sim/medium, the network component in sim/medium/network.h, in carryOver. The network type owns carry-over and the nearest-place queries it uses, and every holder of places calls it.
- Keeping a carried guest's way over the ground: sim/guests, the guests component in sim/guests/guests.cpp, in its finisher carryGuests. The walking direction is the guest's private state, and only the guests module changes it.

## Tasks

### Task 1: Record the simulation's output before the change

Step 1: Build the scenarios runner as main has it.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with no warnings and no errors.

Step 2: Run every scenario and every park for 3000 ticks, and keep the output. It takes about half a minute.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 3000 tests/parks/*.park > build/windows-debug/carry-before.txt 2> /dev/null && grep -c "" build/windows-debug/carry-before.txt`
Expected: `39344`

### Task 2: Update the medium and guests specs

Files:
- Modify: `src/sim/medium/SPEC.md`
- Modify: `src/sim/guests/SPEC.md`

Step 1: In src/sim/medium/SPEC.md, in the Carry-over section's second paragraph, replace

    If the carrier changed shape, the place moves to the nearest place on the same carrier in the new network. If the place did not resolve before, or its carrier is gone, the place is retired, which is a legitimate state its holder handles (principle 2).

with

    Otherwise, if a carrier of the new network passes over the place's old ground position, allowing only for rounding, the place stays at that ground position. It stays on its own carrier if that carrier passes there, and otherwise goes on the nearest carrier that does. So if an edit splits a carrier and gives a part a new key, places on that part stay where they stood. If no carrier passes there but the place's carrier still exists with a new shape, the place moves to the nearest place on that carrier. If the place did not resolve before, or its carrier is gone and no carrier passes over its ground, the place is retired, which is a legitimate state its holder handles (principle 2).

Step 2: In src/sim/medium/SPEC.md, in the Places section, replace

    nearestPlace, nearestPlaceOn, and directionAt are the medium's only straight-line measures. The first two are used only to snap a ground position onto a network,

with

    nearestPlace, nearestPlaceOn, and directionAt, and carryOver's check that a snapped place lies on a place's old ground, are the medium's only straight-line measures. The first two and that check are used only to snap a ground position onto a network,

Step 3: In src/sim/guests/SPEC.md, in the Carrying across an edit section, replace

    When the player edits the park, each guest keeps its ground position if its path survives the edit. If a guest's path or connector is deleted, the guest moves to the nearest point of the guest network. Carrying changes only where guests stand.

with

    When the player edits the park, each guest keeps its ground position if a path or connector still runs over it, even one with a new key, as when an edit splits a path. The guest stays on its own path or connector if that one still runs over its ground. If the guest is carried onto another one that runs the other way there, it turns, so it keeps walking the same way over the ground. If nothing runs over where the guest stood, but its own path or connector is still there with a new shape, the guest moves to the nearest point of it. If its path or connector is deleted, the guest moves to the nearest point of the guest network. Carrying changes only where guests stand and which way they walk.

Step 4: In src/sim/guests/SPEC.md, in the Hungry footfall section's list, replace

    - if a stretch is split, its value stays with the half holding its middle;

with

    - if a stretch is split, its value stays with the part holding its middle, even if that part has a new key;

and replace

    - if a path is deleted, its footfall is dropped.

with

    - if a path is deleted, its footfall is dropped, except where another path runs over a stretch's middle.

### Task 3: Update the capability brief

Files:
- Modify: `plans/shared-medium/CAPABILITY.md:70-74` (the carry-over outcomes)
- Modify: `plans/shared-medium/CAPABILITY.md:112` (the principle 4 bullet)

Step 1: Replace lines 70 to 74,

    The network type offers one carry-over operation. A holder applies it to its own places, using the networks from before and after a re-derivation, so nobody reads another's internals. It has three outcomes:

    - If the place's carrier is unchanged, the place keeps its spot.
    - If the carrier's geometry changed, the place moves to the point on the same carrier nearest its old ground position.
    - If the carrier no longer exists, the place resolves to no place. This is a legitimate state, and the holder handles it.

with

    The network type offers one carry-over operation. A holder applies it to its own places, using the networks from before and after a re-derivation, so nobody reads another's internals. It has four outcomes:

    - If the place's carrier is unchanged, the place keeps its spot.
    - If the carrier changed or is gone, and a carrier of the new network passes over the place's old ground position, the place stays at that ground position. It stays on its own carrier if that carrier passes there.
    - If no carrier passes there and the carrier's geometry changed, the place moves to the point on the same carrier nearest its old ground position.
    - If no carrier passes there and the carrier no longer exists, the place resolves to no place. This is a legitimate state, and the holder handles it.

Step 2: Replace line 112,

    - Principle 4 is kept by sampling only at network places. Distances between places are measured only along the network, by navigable-networks. The one straight-line measure in the medium is the nearest-point query, and it is used only to snap a ground position onto the network.

with

    - Principle 4 is kept by sampling only at network places. Distances between places are measured only along the network, by navigable-networks. The medium's straight-line measures are the nearest-point queries, carry-over's check that a snapped place lies on a place's old ground, and the direction query. The first two are used only to snap a ground position onto the network, and the direction query only to scale a direction to unit length.

### Task 4: Update carryOver's comment

Files:
- Modify: `src/sim/medium/network.h:215-219` (the comment above carryOver)

Step 1: Replace the comment above the declaration of carryOver with:

```cpp
// Moves a place held across a re-derivation from the network before to the network after. It is
// kept when its carrier's points are unchanged. Otherwise, if a carrier after passes within
// ON_GROUND_TOLERANCE of its old ground position, it stays at that ground position: on its own
// carrier if that one passes there, or else on the carrier nearestPlace finds. Otherwise it moves
// to its carrier's nearest point to its old ground position, or is none when it did not resolve
// before or its carrier is gone.
```

Step 2: Above that comment, after the closing brace of class Network, add:

```cpp
// How far a carrier may pass from a place's old ground position, in meters, and still count as
// passing over it. It allows for rounding where a producer cuts a line into new carriers.
constexpr double ON_GROUND_TOLERANCE = 0.001;
```

### Task 5: Keep a place on surviving ground

Files:
- Modify: `src/sim/medium/network.cpp:128-130` (the end of the anonymous namespace)
- Modify: `src/sim/medium/network.cpp:348-361` (carryOver)

Step 1: In the anonymous namespace, after projectOnto and before `} // namespace`, add:

```cpp
// The snapped place, when its ground position lies within ON_GROUND_TOLERANCE of the point.
std::optional<Place> onGround(const Network &network, const std::optional<Place> &snapped,
                              GroundPoint point) {
  const std::optional<GroundPoint> at = snapped ? network.groundPoint(*snapped) : std::nullopt;
  if (!at) {
    return std::nullopt;
  }
  const double dx = at->X - point.X;
  const double dz = at->Z - point.Z;
  if ((dx * dx) + (dz * dz) > ON_GROUND_TOLERANCE * ON_GROUND_TOLERANCE) {
    return std::nullopt;
  }
  return snapped;
}
```

Step 2: Replace the definition of carryOver with:

```cpp
std::optional<Place> carryOver(const Place &place, const Network &before, const Network &after) {
  const std::optional<GroundPoint> ground = before.groundPoint(place);
  if (!ground) {
    return std::nullopt;
  }
  // The place resolves before, so its carrier is there. Stops are not geometry, so a split that
  // keeps the carrier's key keeps the place.
  const Carrier *was = findCarrier(before.carriers(), place.Carrier);
  const Carrier *now = findCarrier(after.carriers(), place.Carrier);
  if (was != nullptr && now != nullptr && was->Points == now->Points) {
    return place;
  }
  // None when the carrier is gone.
  const std::optional<Place> own = after.nearestPlaceOn(place.Carrier, *ground);
  if (const std::optional<Place> stays = onGround(after, own, *ground)) {
    return stays;
  }
  if (const std::optional<Place> stays = onGround(after, after.nearestPlace(*ground), *ground)) {
    return stays;
  }
  return own;
}
```

Step 3: Build the targets that hold the changed code.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios`
Expected: the build finishes with no warnings and no errors.

### Task 6: Keep a carried guest's way over the ground

Files:
- Modify: `src/sim/guests/guests.cpp:409-433` (carryGuests and its comment)

Step 1: Before the comment above carryGuests, add:

```cpp
// Turns the guest about when the carrier its place is carried to runs against the one it stood
// on, there, so it keeps walking the same way over the ground. If either direction is unknown, or
// the two are square to each other, it keeps its way.
void keepWayOverGround(Guest &guest, const Network &before, const Network &after,
                       const Place &carried) {
  const std::optional<GroundDirection> was = before.directionAt(guest.At);
  const std::optional<GroundDirection> now = after.directionAt(carried);
  if (was && now && (was->X * now->X) + (was->Z * now->Z) < 0.0) {
    guest.Forward = !guest.Forward;
  }
}
```

Step 2: Replace the comment above carryGuests with:

```cpp
// Carries each guest's place from the guest network before the resolution to the one it derived:
// by carryOver, and for a retired place, to the new network's nearest place to where the guest
// stood. A guest that carryOver puts on another carrier keeps walking the same way over the
// ground. A place with neither is left as it is, and its guest leaves when it next steps. Nothing
// changes in a world's first resolution, which has no network before.
```

Step 3: In carryGuests, replace

```cpp
    if (const std::optional<Place> carried = carryOver(guest.At, *before, after)) {
      guest.At = *carried;
      continue;
    }
```

with

```cpp
    if (const std::optional<Place> carried = carryOver(guest.At, *before, after)) {
      if (carried->Carrier != guest.At.Carrier) {
        keepWayOverGround(guest, *before, after, *carried);
      }
      guest.At = *carried;
      continue;
    }
```

Step 4: Build the targets that hold the changed code.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios`
Expected: the build finishes with no warnings and no errors.

### Task 7: Confirm the acceptance criteria

Step 1: Build everything.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

Step 2: Run the test suite.

Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run tidy.

Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Run every scenario and every park for 3000 ticks with the changed build, and compare with the output from Task 1.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 3000 tests/parks/*.park > build/windows-debug/carry-after.txt 2> /dev/null && build/windows-debug/tpj_scenarios.exe --compare build/windows-debug/carry-before.txt build/windows-debug/carry-after.txt && echo same`
Expected: `same`

If the comparison prints a difference instead, the change moved a guest or a footfall value in today's game. Stop and report it.

### Task 8: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/medium/SPEC.md, src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/guests/SPEC.md, src/sim/guests/guests.cpp, plans/shared-medium/CAPABILITY.md, and plans/shared-medium/carrier-widths/ground-carry-over/. Use the subject `Medium: Keep carried places on ground that survives`.
