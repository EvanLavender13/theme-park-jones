# Implementation Plan: Box Connections

## Goal

Derive connectors from the entrance's and boxes' doors to the nearest same-kind path within CONNECTION_REACH, anchored to their entities, and mark connectors and anchored nodes in the graph view.

## Approach

The path-networks resolver gathers each kind's doors from parkEntrances and parkBoxes, finds each door's connection place with nearestPlace over a network of the path carriers alone, and appends a two-point connector carrier for each door within reach. Each connection enters the existing stop grouping and joining as one more meeting, between the connector's end and the path, and each connector's door node is anchored to its entity. The graph overlay flags lines whose carrier is not a path and gives each node its anchor, and the app picks colors and radii from them.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify connectors

Files:
- Modify: `src/sim/routes/SPEC.md`

Step 1: Make the routes changes FEATURE.md's Spec changes give, in order: the introduction's intent queries, the Networks paragraph, the Meetings first sentence and added paragraph, the new Connectors section after Meetings, and the anchors sentence in Stops and nodes. Paste indented blocks without their four-space indent.

Run: `grep -c "^## " src/sim/routes/SPEC.md; grep -c "CONNECTION_REACH" src/sim/routes/SPEC.md`
Expected: `4`, then `1`.

### Task 2: Specify the graph view's marks

Files:
- Modify: `src/render/SPEC.md`
- Modify: `src/app/SPEC.md`

Step 1: Make the render and app changes FEATURE.md's Spec changes give.

Run: `grep -c "GRAPH_ANCHOR_COLOR" src/render/SPEC.md; grep -c "of its radius and color" src/app/SPEC.md`
Expected: `1`, then `1`.

### Task 3: Declare connectors and the overlay's marks

Files:
- Modify: `src/sim/routes/networks.h`
- Modify: `src/render/graph_overlay.h`

Step 1: In networks.h, add after the JUNCTION_TOLERANCE declaration:

```cpp
// A door farther than this from every path of its kind has no connector.
inline constexpr double CONNECTION_REACH = 4.0;
```

and after the networkKey function:

```cpp
inline constexpr uint64_t CONNECTOR_PURPOSE = hashName("connector");

// A face of an entrance or box, whose midpoint is a door that may connect to a network.
enum class Face : uint8_t { Front, Back };

// The key of the connector from the door on the entity's face.
constexpr EntityKey connectorKey(EntityKey entity, Face face) {
  return deriveKey(entity, CONNECTOR_PURPOSE, static_cast<uint64_t>(face));
}
```

Step 2: In graph_overlay.h, add after GRAPH_NODE_COLOR:

```cpp
inline constexpr Rgba GRAPH_CONNECTOR_COLOR{1.0f, 0.90f, 0.10f, 1.0f};
inline constexpr Rgba GRAPH_ANCHOR_COLOR{1.0f, 0.45f, 0.10f, 1.0f};
```

add after GRAPH_NODE_RADIUS:

```cpp
inline constexpr float GRAPH_ANCHOR_RADIUS = 6.0f;
```

add as GraphLine's last member:

```cpp
  // Whether the carrier is a connector rather than a path.
  bool Connector = false;
```

and as GraphNode's last member:

```cpp
  // The node's anchored entity, or NULL_KEY.
  EntityKey Anchor = NULL_KEY;
```

Run: `cmake --build --preset linux-debug --target tpj_sim tpj_render 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 4: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/routes/SPEC.md, src/sim/medium/SPEC.md, src/sim/park/SPEC.md, src/render/SPEC.md, and src/app/SPEC.md, and the public headers src/sim/routes/networks.h, src/sim/medium/network.h, src/sim/park/geometry.h, src/sim/park/intent.h, and src/render/graph_overlay.h.

### Task 5: Find each kind's doors

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: Add `#include <cmath>` to the standard includes, keeping them sorted.

Step 2: Add to the anonymous namespace, before deriveNetwork:

```cpp
// The midpoint of an entity's face, serving one network kind.
struct Door {
  EntityKey Entity = NULL_KEY;
  Face Side = Face::Front;
  GroundPoint At;
};

// Adds the door at the midpoint of the face of the pose's footprint for the size: the first two
// corners for the front, the last two for the back. Nothing when the pose has no footprint.
void addDoor(std::vector<Door> &doors, EntityKey entity, const Pose &pose, FootprintSize size,
             Face face) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return;
  }
  const size_t first = face == Face::Front ? 0 : 2;
  const ParkPoint &a = footprint->Corners[first];
  const ParkPoint &b = footprint->Corners[first + 1];
  doors.push_back({.Entity = entity, .Side = face, .At = {(a.X + b.X) / 2.0, (a.Z + b.Z) / 2.0}});
}

// The doors serving the kind: an entrance's front serves guests, a shop's front guests and its back
// backstage, and a depot's front backstage.
std::vector<Door> doorsServing(const std::vector<ParkEntrance> &entrances,
                               const std::vector<ParkBox> &boxes, PathKind kind) {
  std::vector<Door> doors;
  if (kind == PathKind::Guest) {
    for (const ParkEntrance &entrance : entrances) {
      addDoor(doors, entrance.Key, entrance.At, ENTRANCE_SIZE, Face::Front);
    }
  }
  for (const ParkBox &box : boxes) {
    const bool shop = box.Kind == BoxKind::Shop;
    if (kind == PathKind::Guest && shop) {
      addDoor(doors, box.Key, box.At, boxSize(box.Kind), Face::Front);
    } else if (kind == PathKind::Backstage) {
      addDoor(doors, box.Key, box.At, boxSize(box.Kind), shop ? Face::Back : Face::Front);
    }
  }
  return doors;
}
```

Nothing is built yet: the compiler rejects unused functions in an anonymous namespace, and Task 7 is the first to call these. Task 7 builds and runs the tests.

### Task 6: Find connections

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: Add after doorsServing:

```cpp
// The path carriers, each stopping only at its two ends, as a network nearestPlace can search.
Network linesOnly(const std::vector<Carrier> &carriers) {
  std::vector<Carrier> lines = carriers;
  uint32_t nodeCount = 0;
  for (Carrier &line : lines) {
    line.Stops = {{.Distance = 0.0, .Node = nodeCount},
                  {.Distance = line.Points.back().Distance, .Node = nodeCount + 1}};
    nodeCount += 2;
  }
  return Network(std::move(lines), nodeCount, {});
}

// A connector, its door's entity, and the index of the path carrier it meets and the distance
// there.
struct Connection {
  Carrier Connector;
  EntityKey Entity = NULL_KEY;
  size_t Path = 0;
  double PathDistance = 0.0;
};

// The connectors of the doors whose nearest point on the path carriers lies within reach. The
// carriers are in key order.
std::vector<Connection> findConnections(const std::vector<Carrier> &carriers,
                                        const std::vector<Door> &doors) {
  std::vector<Connection> connections;
  if (carriers.empty()) {
    return connections;
  }
  const Network lines = linesOnly(carriers);
  for (const Door &door : doors) {
    const std::optional<Place> place = lines.nearestPlace(door.At);
    const std::optional<GroundPoint> point =
        place ? lines.groundPoint(*place) : std::optional<GroundPoint>();
    if (!point) {
      continue;
    }
    const double dx = point->X - door.At.X;
    const double dz = point->Z - door.At.Z;
    const double reach = std::sqrt((dx * dx) + (dz * dz));
    if (reach > CONNECTION_REACH || !(reach > JUNCTION_TOLERANCE)) {
      continue;
    }
    // A path keyed like the connector, which only a hand-written save holds, keeps carrier keys
    // unique by leaving the door unconnected.
    const EntityKey key = connectorKey(door.Entity, door.Side);
    if (std::ranges::binary_search(carriers, key, {}, &Carrier::Key)) {
      continue;
    }
    const auto path = std::ranges::lower_bound(carriers, place->Carrier, {}, &Carrier::Key);
    connections.push_back(
        {.Connector = {.Key = key,
                       .Points = {{.X = door.At.X, .Z = door.At.Z, .Distance = 0.0},
                                  {.X = point->X, .Z = point->Z, .Distance = reach}},
                       .Stops = {}},
         .Entity = door.Entity,
         .Path = static_cast<size_t>(path - carriers.begin()),
         .PathDistance = place->Distance});
  }
  return connections;
}
```

### Task 7: Derive connectors and anchors

Files:
- Modify: `src/sim/routes/networks.cpp`

Step 1: Change deriveNetwork's signature to:

```cpp
Network deriveNetwork(const std::vector<ParkPath> &paths, const std::vector<Door> &doors,
                      PathKind kind) {
```

Step 2: Replace its first two lines with:

```cpp
  std::vector<Carrier> carriers = pathCarriers(paths, kind);
  std::vector<Meeting> meetings = findMeetings(carriers);

  // Each connector meets its path at its connection.
  std::vector<Connection> connections = findConnections(carriers, doors);
  const size_t pathCount = carriers.size();
  for (Connection &connection : connections) {
    meetings.push_back({.First = connection.Path,
                        .FirstDistance = connection.PathDistance,
                        .Second = carriers.size(),
                        .SecondDistance = connection.Connector.Points.back().Distance});
    carriers.push_back(std::move(connection.Connector));
  }
```

Step 3: Number nodes walking every carrier in key order, since a path may hold a derived key in a hand-written save, so connectors need not follow the paths. Replace the numbering loop's comment and header,

```cpp
  // Nodes are numbered in order of first stop, carriers in key order and stops by distance.
```

and `  for (size_t c = 0; c < carriers.size(); ++c) {` just above `    const double length = carriers[c].Points.back().Distance;`, keeping the three lines between them, so the code reads:

```cpp
  // Nodes are numbered in order of first stop, carriers in key order and stops by distance. Paths
  // and connectors are walked together, since a hand-written save can give a path a key above a
  // connector's.
  std::vector<size_t> keyOrder(carriers.size());
  std::iota(keyOrder.begin(), keyOrder.end(), size_t{0});
  std::ranges::sort(keyOrder, {}, [&carriers](size_t c) { return carriers[c].Key; });
  constexpr uint32_t UNNUMBERED = UINT32_MAX;
  std::vector<uint32_t> numbers(stopCount, UNNUMBERED);
  uint32_t nodeCount = 0;
  for (const size_t c : keyOrder) {
    const double length = carriers[c].Points.back().Distance;
```

Step 4: Replace its last line, `return Network(std::move(carriers), nodeCount, {});`, with:

```cpp
  // Each connector's door is its first stop, a node nothing else meets, anchored to its entity.
  std::vector<NodeAnchor> anchors;
  for (size_t k = 0; k < connections.size(); ++k) {
    anchors.push_back(
        {.Node = carriers[pathCount + k].Stops.front().Node, .Entity = connections[k].Entity});
  }
  return Network(std::move(carriers), nodeCount, std::move(anchors));
```

Step 5: In resolvePathNetworks, replace its body's first three lines, from `const std::vector<ParkPath> paths` through `Network network = deriveNetwork(paths, kind);`, with:

```cpp
  const std::vector<ParkPath> paths = parkPaths(world);
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  const std::vector<ParkBox> boxes = parkBoxes(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    Network network = deriveNetwork(paths, doorsServing(entrances, boxes, kind), kind);
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#networks_test],[#network_edits_test],[#routes_park_test]" 2>&1 | tail -2`
Expected: no build output, and every routes test passes, including the test pass's connector tests in those or new files under tests/sim/routes/. If the test pass added files, add their `[#<file>]` tags to the filter.

### Task 8: Mark connectors and anchors in the overlay

Files:
- Modify: `src/render/graph_overlay.cpp`

Step 1: Add `#include "sim/park/intent.h"` after `#include "render/math.h"`, and `#include <algorithm>` and `#include <vector>` with `<cstddef>`, keeping includes sorted.

Step 2: Give appendLines a parameter `const std::vector<EntityKey> &pathKeys` after `PathKind kind`, and in its push_back set the new member:

```cpp
        overlay.Lines.push_back(GraphLine{kind, carrier.Key, static_cast<uint32_t>(index), *from,
                                          *to,
                                          !std::ranges::binary_search(pathKeys, carrier.Key)});
```

Step 3: In appendNodes, push the node's anchor:

```cpp
      overlay.Nodes.push_back(GraphNode{kind, node, *at, network.nodeAnchor(node)});
```

Step 4: In buildGraphOverlay, after the width and height check, add:

```cpp
  // parkPaths gives paths in ascending key order, so their keys can be searched.
  std::vector<EntityKey> pathKeys;
  for (const ParkPath &path : parkPaths(world)) {
    pathKeys.push_back(path.Key);
  }
```

and pass `pathKeys` to appendLines after `kind`.

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests 2>&1 | tail -2`
Expected: no build output, and all tpj_render_tests pass.

### Task 9: Draw connectors and anchored nodes

Files:
- Modify: `src/app/main.cpp`

Step 1: In drawGraph, replace the two loops with:

```cpp
  for (const tpj::GraphLine &line : overlay.Lines) {
    const tpj::Rgba color = line.Connector ? tpj::GRAPH_CONNECTOR_COLOR : tpj::graphColor(line.Kind);
    drawList->AddLine(ImVec2(line.From.X, line.From.Y), ImVec2(line.To.X, line.To.Y),
                      imColor(color), tpj::GRAPH_LINE_THICKNESS);
  }
  for (const tpj::GraphNode &node : overlay.Nodes) {
    const bool anchored = node.Anchor != tpj::NULL_KEY;
    drawList->AddCircleFilled(ImVec2(node.At.X, node.At.Y),
                              anchored ? tpj::GRAPH_ANCHOR_RADIUS : tpj::GRAPH_NODE_RADIUS,
                              imColor(anchored ? tpj::GRAPH_ANCHOR_COLOR : tpj::GRAPH_NODE_COLOR));
  }
```

Step 2: Change drawGraph's comment to: "Draws the networks over the scene and behind every panel: lines in their kind's graph color, or the connector color, then nodes, anchored ones larger in the anchor color."

Run: `cmake --build --preset linux-debug --target tpj_app 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 10: Verify

Step 1: Format, then build and test linux-debug in full.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i; cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings or errors, and "100% tests passed".

Step 2: Build and test windows-debug, then run the cross-build check, since the simulation's derived data changed.

Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/cross-build-check.sh 2>&1 | tail -2`
Expected: the build finishes, "100% tests passed", and the cross-build check reports identical outputs.

Step 3: Capture routes.park with the graph on, with the Windows build, which captures reliably, convert it to PNG with PIL, and look at it.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/routes.park --graph --capture build/windows-debug/connections.bmp`
Expected: exit status 0. The capture shows four short yellow connectors: from the entrance's front to the first guest path, from the shop's front to that path, from the shop's back to the backstage path, and from the depot's front to the backstage path's end. Each has a larger orange dot at its door.

### Task 11: Commit

Stage the changed paths by name (src/sim/routes, src/render, src/app, tests/sim/routes, tests/render, tests/sim/CMakeLists.txt when the test pass changed it, and plans/navigable-networks/paths-become-routes/box-connections), never parks/sketch.park, and commit via the commit-hygiene skill with the subject "Routes: Connect doors to the networks by connectors".
