# Implementation Plan: Carrier Widths

## Goal

Give every carrier a walkable width, and let the network answer the width and the direction of travel at a place, without changing the simulation.

## Approach

Carrier gains a Width member, listed in its visitFields so copies, equality, and hashes cover it. The Network constructor refuses a width that is not finite and above zero. Two queries, widthAt and directionAt, find the place's carrier the way groundPoint does. routes sets each path carrier's and connector's width from pathWidth(kind). The stalls scenario gives its track a named width.

## Placement

Decision 0027 places each behavior this feature adds:

- A carrier's width, its validation, and the widthAt and directionAt queries: sim/medium, the network component in sim/medium/network.h. The network type owns carriers and every query about a place on them.
- Setting each derived carrier's width to its kind's path width: sim/routes, the networks component in sim/routes/networks.h and networks.cpp. routes builds every park network and already turns path intent into carriers there.
- The track's width in the synthetic stalls scenario: scenarios, src/scenarios/stalls.cpp. That scenario builds its own network, and only it uses the track.

## Tasks

### Task 1: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: In the Networks section, after the sentence "It is given as a polyline of ground points with the producer's own arc lengths.", insert this sentence in the same paragraph:

    Each carrier also has a walkable width, the same along its whole length.

Step 2: In the Places section, after the paragraph that starts "groundPoint gives a place's ground position", add this paragraph,

    widthAt gives the walkable width at a place, and directionAt gives the direction of travel there, as a unit vector on the ground pointing toward higher distances along its carrier. At a point where the carrier's polyline bends, the direction is that of the piece ahead. At the carrier's end, it is that of the last piece. Where a place doesn't resolve, both give nothing. If the piece has no length on the ground, directionAt gives nothing.

and replace the last two sentences of the groundPoint paragraph, "These are the medium's only straight-line measures. They are used only to snap a ground position onto a network (principle 4).", with:

    nearestPlace, nearestPlaceOn, and directionAt are the medium's only straight-line measures. The first two are used only to snap a ground position onto a network, and directionAt only to scale a direction to unit length. None of them measures how far anything travels (principle 4).

and add this bullet to the list of what makes a network well formed, after "Each carrier's distances start at 0 and strictly increase.":

    - Each carrier's width is finite and above zero.

### Task 2: Update the routes spec

Files:
- Modify: `src/sim/routes/SPEC.md`

Step 1: In the Networks section, after the sentence "It also has a carrier for each connector.", insert this sentence in the same paragraph:

    Every carrier, a path's or a connector's, has its kind's path width (pathWidth, sim/park/SPEC.md).

### Task 3: Add the width and the queries to the network interface

Files:
- Modify: `src/sim/medium/network.h:44-56` (Carrier and its visitFields)
- Modify: `src/sim/medium/network.h:94-99` (after GroundPoint)
- Modify: `src/sim/medium/network.h:157-159` (after groundPoint's declaration)

Step 1: Replace the Carrier struct and its visitFields with:

```cpp
// A line with a stable key that edges are stretches of, such as a drawn path or a box's connector.
// Its stops cut it into edges, the first at 0 and the last at its length. Its width is how wide a
// mover can walk across it, the same along its whole length.
struct Carrier {
  EntityKey Key = NULL_KEY;
  double Width = 0.0;
  std::vector<CarrierPoint> Points;
  std::vector<CarrierStop> Stops;
};

template <typename Visitor> void visitFields(Visitor &visitor, Carrier &carrier) {
  visitor.field("key", carrier.Key);
  visitor.field("width", carrier.Width);
  visitor.field("points", carrier.Points);
  visitor.field("stops", carrier.Stops);
}
```

Step 2: After the GroundPoint struct, add:

```cpp
// A unit vector on the ground.
struct GroundDirection {
  double X = 0.0;
  double Z = 0.0;

  bool operator==(const GroundDirection &) const = default;
};
```

Step 3: In the Network class, after the declaration of groundPoint, add:

```cpp
  // The walkable width of the place's carrier. None where resolve gives none.
  [[nodiscard]] std::optional<double> widthAt(const Place &place) const;
  // The unit direction toward higher distances along the place's carrier: that of the polyline
  // piece ahead of the place, or of the last piece at the carrier's end. None where resolve gives
  // none, or where that piece has no length on the ground.
  [[nodiscard]] std::optional<GroundDirection> directionAt(const Place &place) const;
```

Step 4: In the comment above the Network constructor, change "a carrier's points or stops out of order, missing, or not finite," to "a carrier's width not finite and above zero, a carrier's points or stops out of order, missing, or not finite,".

### Task 4: Refuse a bad width and answer the two queries

Files:
- Modify: `src/sim/medium/network.cpp:21-39` (after requireValidPoints)
- Modify: `src/sim/medium/network.cpp:136-137` (the constructor's per-carrier checks)
- Modify: `src/sim/medium/network.cpp:268-283` (after groundPoint's definition)

Step 1: After requireValidPoints, in the anonymous namespace, add:

```cpp
void requireValidWidth(const Carrier &carrier) {
  if (!isFinite(carrier.Width) || !(carrier.Width > 0.0)) {
    throw std::invalid_argument(carrierName(carrier.Key) +
                                " has a width that is not finite and above zero");
  }
}
```

Step 2: In the constructor's loop, before `requireValidPoints(Carriers[i]);`, add `requireValidWidth(Carriers[i]);`.

Step 3: After the definition of Network::groundPoint, add:

```cpp
std::optional<double> Network::widthAt(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  return Carriers[*index].Width;
}

std::optional<GroundDirection> Network::directionAt(const Place &place) const {
  const std::optional<size_t> index = findCarrierOf(place);
  if (!index) {
    return std::nullopt;
  }
  const std::vector<CarrierPoint> &points = Carriers[*index].Points;
  // The first point beyond the distance, so a place at a point takes the piece ahead of it. The
  // first point is at 0 and the place is at or above 0, so it is never the first point. Beyond
  // the end there is none, and the last piece is used.
  auto after = std::ranges::upper_bound(points, place.Distance, {}, &CarrierPoint::Distance);
  if (after == points.end()) {
    after = points.end() - 1;
  }
  const CarrierPoint &a = *(after - 1);
  const CarrierPoint &b = *after;
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  const double length = std::sqrt((dx * dx) + (dz * dz));
  if (!(length > 0.0)) {
    return std::nullopt;
  }
  return GroundDirection{dx / length, dz / length};
}
```

### Task 5: Give routes' carriers their kind's path width

Files:
- Modify: `src/sim/routes/networks.cpp:21-36` (pathCarriers)
- Modify: `src/sim/routes/networks.cpp:283-325` (findConnections)
- Modify: `src/sim/routes/networks.cpp:385` (deriveNetwork's call to findConnections)

Step 1: In pathCarriers, change

```cpp
    carriers.push_back(Carrier{.Key = path.Key, .Points = std::move(line), .Stops = {}});
```

to

```cpp
    carriers.push_back(Carrier{
        .Key = path.Key, .Width = pathWidth(kind), .Points = std::move(line), .Stops = {}});
```

Step 2: Change findConnections' comment and signature to take the connectors' width:

```cpp
// The connectors of the doors whose nearest point on the path carriers lies within reach, each
// the given width. The carriers are in key order.
std::vector<Connection> findConnections(const std::vector<Carrier> &carriers,
                                        const std::vector<Door> &doors, double width) {
```

Step 3: In findConnections, in the Connector initializer, add `.Width = width,` after `.Key = key,`, so it reads:

```cpp
        {.Connector = {.Key = key,
                       .Width = width,
                       .Points = {{.X = door.At.X, .Z = door.At.Z, .Distance = 0.0},
                                  {.X = point->X, .Z = point->Z, .Distance = reach}},
                       .Stops = {}},
```

Step 4: In deriveNetwork, change `findConnections(carriers, doors)` to `findConnections(carriers, doors, pathWidth(kind))`.

linesOnly copies the path carriers, so its network gets their widths with no change.

### Task 6: Give the stalls scenario's track a width

Files:
- Modify: `src/scenarios/stalls.cpp:79-85` (the constants)
- Modify: `src/scenarios/stalls.cpp:133-136` (resolveTrack's carrier)

Step 1: After `constexpr uint64_t TRACK_PURPOSE = hashName("track-network");`, add:

```cpp
// How wide the track is. Nothing in the scenario reads it, but every carrier has a width.
constexpr double TRACK_WIDTH = 3.0;
```

Step 2: In resolveTrack, add `.Width = TRACK_WIDTH,` after `.Key = owner,`, so the carrier reads:

```cpp
  Carrier carrier{.Key = owner,
                  .Width = TRACK_WIDTH,
                  .Points = {{.X = 0.0, .Z = 0.0, .Distance = 0.0},
                             {.X = track.Length, .Z = 0.0, .Distance = track.Length}},
                  .Stops = {}};
```

Step 3: Build the targets that hold the changed code. Every carrier now names its width, so no initializer leaves one out.

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

Step 4: Regenerate the slice parks into the build directory and compare them with the checked-in ones.

Run:

```
mkdir -p build/windows-debug/regen && cp tests/parks/fed.park build/windows-debug/regen/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks build/windows-debug/regen/fed.park build/windows-debug/regen/warm.park build/windows-debug/regen/cut.park; for f in fed warm cut; do cmp build/windows-debug/regen/$f.park tests/parks/$f.park && echo "$f same"; done
```

Expected:

```
fed same
warm same
cut same
```

Step 5: Regenerate the full park into the build directory and compare it with the checked-in one. It takes about three minutes.

Run: `build/windows-debug/tpj_bench.exe --full-park build/windows-debug/regen/full.park && cmp build/windows-debug/regen/full.park tests/parks/stress/full.park && echo "full same"`
Expected: `full same`

If any comparison prints a difference instead, the width changed the simulation. Stop and report it.

### Task 8: Commit

Step 1: Commit the feature with the commit-hygiene skill. Stage only these paths: src/sim/medium/SPEC.md, src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/routes/SPEC.md, src/sim/routes/networks.cpp, src/scenarios/stalls.cpp, and plans/shared-medium/carrier-widths/carrier-widths/. Use the subject `Medium: Give every carrier a walkable width`.
