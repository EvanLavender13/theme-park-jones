# Implementation Plan: Shop Records

## Goal

Each shop box has an inspection record with its limiting factor, a starved shop is marked by a floating cube in the scene and in the ghost of any edit that starves it, the Debug panel lists every shop's record, and tests/parks/supply.park shows one starved shop.

## Approach

shopRecord is a query in operations.cpp that computes the record from the world as it stands, with no new state. park_mesh.cpp splits appendBox's faces into a block helper so the starved mark is the same faces raised and closed. appendStarvedMarks draws a mark over each starved shop, buildParkMesh ends with it, and buildGhostMesh ends with the candidate's. The app fills the Debug panel's shop lines from shopRecord each frame.

## Tasks

### Task 1: Operations spec

Files:
- Modify: `src/sim/operations/SPEC.md`

Step 1: In the Registration section, replace "and a shop's offer is in the food-offer field (Food offer)." with "and a shop's offer is in the food-offer field (Food offer). A shop's inspection record is computed from them (Inspection record)."

Step 2: Append this section at the end of the file:

```
## Inspection record

A shop publishes an inspection record about itself for display and tests (decision 0025). No system or resolver reads one, so it carries no interaction. shopRecord(world, key) gives none when key is not the key of a shop box of parkBoxes, and otherwise the shop's ShopRecord in the world as it stands: Stock, the supplies units it holds under its own handle; Queue, the guest-visits units it holds under handles that name live entities, so visits that arrived at the last swap count before its step queues them; OnOrder, inventoryPosition less Stock, which is the supplies on their way to it and the orders on their way to or held by a depot; Starved, whether nearestDepot gives none; and Limit, its limiting factor. The record is computed, not stored, so it adds nothing to a save, and a candidate's shops have theirs at once.

The limiting factor names what holds the shop's service back. It is NoSupplyRoute for a starved shop, whatever its stock, and otherwise Demand when no guest is queued, Supply when the shop holds fewer supplies than guests queued, and ServiceRate when it holds enough for every guest queued, so only the one guest per SERVICE_INTERVAL holds them. So a shop is starved exactly when its factor is NoSupplyRoute. limitingFactorName gives each factor's name for display: demand, supply, service rate, and no supply route. Nothing steps route distance, so Starved changes only when resolution publishes it, and in a world whose route distance fields hold no stepped entries it is a function of intent.
```

Step 3: Check the text.

Run: `grep -c "Inspection record" src/sim/operations/SPEC.md`
Expected: `2`

### Task 2: Render spec

Files:
- Modify: `src/render/SPEC.md`

Step 1: In the Park mesh section's first paragraph, replace "and the networks only through parkNetwork (sim/routes/SPEC.md) and the Network type's public queries (sim/medium/SPEC.md), and changes nothing." with "the networks only through parkNetwork (sim/routes/SPEC.md) and the Network type's public queries (sim/medium/SPEC.md), and shops' inspection records only through shopRecord (sim/operations/SPEC.md), and changes nothing."

Step 2: Insert this paragraph after the paragraph that begins "appendBox adds a box" and before the one that begins "buildParkMesh gives":

```
A starved shop is marked by a cube floating over its roof. appendStarvedMark adds the faces appendBox adds for a pose, the size {STARVED_MARK_SIZE, STARVED_MARK_SIZE}, the height STARVED_MARK_SIZE, and a color, in the same order and with the same normals and colors, but with every vertex STARVED_MARK_BASE higher. Then it adds a bottom face, four vertices at height STARVED_MARK_BASE over the footprint's corners with the normal (0, -1, 0) and the color, and two triangles wound counter-clockwise seen from below, so the cube is closed. STARVED_MARK_SIZE is 1.5 m and STARVED_MARK_BASE 5 m, a meter above a shop's roof. A pose with no footprint for that size adds nothing. appendStarvedMarks adds, for each shop box of parkBoxes whose shopRecord is Starved, in key order, appendStarvedMark at its pose in STARVED_COLOR with its alpha replaced by a given alpha.
```

Step 3: In the paragraph that begins "buildParkMesh gives", replace "each in the order the query gives." with "each in the order the query gives, and then appendStarvedMarks with alpha 1."

Step 4: In the Ghosts section's first paragraph, append after its last sentence: " STARVED_COLOR, a violet, is opaque, and differs in red, green, and blue from each of those colors, from ENTRANCE_COLOR, and from every color graph_overlay.h declares."

Step 5: In the paragraph that begins "buildGhostMesh gives", replace "with alpha GHOST_ALPHA. An edit's ghost color" with "with alpha GHOST_ALPHA, and then appendStarvedMarks of that candidate with alpha GHOST_ALPHA. An edit's ghost color", and append after its last sentence, "so only the walkways the edit adds or moves stand out.", this: " Likewise, an accepted edit's ghost marks have the positions and normals of the starved marks buildParkMesh draws in the world the edit gives, and a ghost mark lying on a committed one blends to its color, so only the shops the edit would starve stand out: hovering a backstage path with the delete tool shows a mark over each shop the deletion leaves with no depot."

Step 6: Check the text.

Run: `grep -c "appendStarvedMarks" src/render/SPEC.md; grep -c "STARVED_COLOR" src/render/SPEC.md`
Expected: `3` and `2`

### Task 3: App spec

Files:
- Modify: `src/app/SPEC.md`

Step 1: In the Park section, replace "The mesh holds the walkways of the world's networks, which derive from intent alone (sim/routes/SPEC.md) and are resolved in every world the app holds, so the mesh is rebuilt whenever they change, and the ghost, rebuilt with it, shows its candidate's walkways." with:

```
The mesh holds the walkways of the world's networks, which derive from intent alone (sim/routes/SPEC.md) and are resolved in every world the app holds, and the starved marks of its shops, which change only when resolution publishes route distance (sim/operations/SPEC.md), so only after a change of intent or an opened park. So the mesh is rebuilt whenever they change, and the ghost, rebuilt with it, shows its candidate's walkways and starved marks.
```

Step 2: In the Tooling UI section, replace "and a Graph checkbox, off at start unless --graph is given." with "and a Graph checkbox, off at start unless --graph is given. Below them, each frame, it lists each shop box of parkBoxes, in key order, as a line `Shop <key>: stock <Stock>, queue <Queue>, on order <OnOrder>, <limit>` from its shopRecord, each number in decimal and limit its limitingFactorName."

Step 3: Check the text.

Run: `grep -c "starved marks" src/app/SPEC.md; grep -c "limitingFactorName" src/app/SPEC.md`
Expected: `1` and `1`, since both mentions of the marks lie on one line.

### Task 4: Record interface

Files:
- Modify: `src/sim/operations/operations.h`
- Modify: `src/sim/operations/operations.cpp`

Step 1: In operations.h, insert after the ABANDONED_CAUSE line and before the DepotRoute comment:

```cpp
// What limits a shop's service now: no guests queued, fewer supplies than guests, the service
// rate, or no route to a depot.
enum class LimitingFactor : uint8_t { Demand, Supply, ServiceRate, NoSupplyRoute };

// What a shop publishes about itself for display and tests (decision 0025): the supplies it
// holds, the guests queued, the supplies on order, whether it has no route to a depot, and what
// limits its service. Nothing in the park reads it.
struct ShopRecord {
  int64_t Stock = 0;
  int64_t Queue = 0;
  int64_t OnOrder = 0;
  bool Starved = false;
  LimitingFactor Limit = LimitingFactor::Demand;

  bool operator==(const ShopRecord &) const = default;
};
```

Step 2: In operations.h, insert after the inventoryPosition declaration and before the addOperations comment:

```cpp
// The shop box's inspection record in the world as it stands, or none when the key is not a shop
// box.
std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop);
// The factor's name for display: "demand", "supply", "service rate", or "no supply route".
std::string_view limitingFactorName(LimitingFactor factor);
```

Step 3: In operations.cpp, add stubs after the definition of inventoryPosition:

```cpp
std::optional<ShopRecord> shopRecord(const World & /*world*/, EntityKey /*shop*/) {
  return std::nullopt;
}

std::string_view limitingFactorName(LimitingFactor /*factor*/) { return ""; }
```

Step 4: Build.

Run: `cmake --build --preset linux-debug --target tpj_sim 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 5: Mark interface

Files:
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`

Step 1: In park_mesh.h, insert after the appendBox declaration:

```cpp
// A starved shop's mark: a cube of this size whose bottom floats this high, a meter over the
// shop's roof, in a violet no other part of the park is drawn in.
inline constexpr float STARVED_MARK_SIZE = 1.5f;
inline constexpr float STARVED_MARK_BASE = 5.0f;
inline constexpr Rgba STARVED_COLOR{0.56f, 0.24f, 0.86f, 1.0f};

// Adds a closed cube of STARVED_MARK_SIZE over the pose, its bottom at STARVED_MARK_BASE, or
// nothing when the pose has no footprint.
void appendStarvedMark(ParkMesh &mesh, const Pose &pose, Rgba color);
// Adds a mark over each shop box whose inspection record says it is starved, in key order, in
// STARVED_COLOR with the alpha given.
void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha);
```

Step 2: In park_mesh.cpp, add stubs after the definition of appendBox:

```cpp
void appendStarvedMark(ParkMesh & /*mesh*/, const Pose & /*pose*/, Rgba /*color*/) {}

void appendStarvedMarks(ParkMesh & /*mesh*/, const World & /*world*/, float /*alpha*/) {}
```

Step 3: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 6: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/operations/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, src/sim/routes/SPEC.md, src/sim/park/SPEC.md, src/sim/medium/SPEC.md, and src/sim/SPEC.md, and the public headers src/sim/operations/operations.h and src/render/park_mesh.h.

### Task 7: Shop records

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Replace the two stubs from Task 4 with:

```cpp
std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop) {
  const std::vector<EntityKey> shops = boxKeys(world, BoxKind::Shop);
  if (std::ranges::find(shops, shop) == shops.end()) {
    return std::nullopt;
  }
  ShopRecord record;
  record.Stock = unitsHeld<Supplies>(world, shop, shop);
  for (const FlowHolding &held : stockOf<GuestVisits>(world, shop)) {
    if (isLive(world, held.Handle)) {
      record.Queue += held.Units;
    }
  }
  record.OnOrder = inventoryPosition(world, shop) - record.Stock;
  record.Starved = !nearestDepot(world, shop).has_value();
  if (record.Starved) {
    record.Limit = LimitingFactor::NoSupplyRoute;
  } else if (record.Queue == 0) {
    record.Limit = LimitingFactor::Demand;
  } else if (record.Stock < record.Queue) {
    record.Limit = LimitingFactor::Supply;
  } else {
    record.Limit = LimitingFactor::ServiceRate;
  }
  return record;
}

std::string_view limitingFactorName(LimitingFactor factor) {
  switch (factor) {
  case LimitingFactor::Demand:
    return "demand";
  case LimitingFactor::Supply:
    return "supply";
  case LimitingFactor::ServiceRate:
    return "service rate";
  case LimitingFactor::NoSupplyRoute:
    return "no supply route";
  }
  return "";
}
```

Step 2: Build and run the operations tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for each of the test pass's operations files, one per invocation.
Expected: no build output; all pass.

### Task 8: The starved mark

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Replace the definition of appendBox with a block helper in the anonymous namespace, after addQuad, and appendBox and appendStarvedMark calling it:

In the anonymous namespace, after addQuad:

```cpp
// Adds a block over the footprint from the base up to the top: its top, then its four sides, the
// front one lightened, and, when closed, its bottom.
void appendBlock(ParkMesh &mesh, const Footprint &footprint, float base, float top, bool closed,
                 Rgba color) {
  const auto &[frontLeft, frontRight, backRight, backLeft] = footprint.Corners;
  const ParkPoint forward = footprint.Forward;
  const ParkPoint right = footprint.Right;
  const std::array<float, 4> sideHeights{base, base, top, top};

  addQuad(mesh, {frontLeft, backLeft, backRight, frontRight}, {top, top, top, top}, {}, 1.0f,
          color);
  // Each side runs along its edge counter-clockwise seen from above, then rises back over it.
  addQuad(mesh, {frontLeft, backLeft, backLeft, frontLeft}, sideHeights, {-right.X, -right.Z}, 0.0f,
          color);
  addQuad(mesh, {backLeft, backRight, backRight, backLeft}, sideHeights, {-forward.X, -forward.Z},
          0.0f, color);
  addQuad(mesh, {backRight, frontRight, frontRight, backRight}, sideHeights, right, 0.0f, color);
  addQuad(mesh, {frontRight, frontLeft, frontLeft, frontRight}, sideHeights, forward, 0.0f,
          lightened(color));
  if (closed) {
    // The top's corners in reverse, counter-clockwise seen from below.
    addQuad(mesh, {frontLeft, frontRight, backRight, backLeft}, {base, base, base, base}, {}, -1.0f,
            color);
  }
}
```

The public definitions become:

```cpp
void appendBox(ParkMesh &mesh, const Pose &pose, FootprintSize size, float height, Rgba color) {
  if (const std::optional<Footprint> footprint = footprintOf(pose, size)) {
    appendBlock(mesh, *footprint, 0.0f, height, false, color);
  }
}

void appendStarvedMark(ParkMesh &mesh, const Pose &pose, Rgba color) {
  if (const std::optional<Footprint> footprint =
          footprintOf(pose, FootprintSize{STARVED_MARK_SIZE, STARVED_MARK_SIZE})) {
    appendBlock(mesh, *footprint, STARVED_MARK_BASE, STARVED_MARK_BASE + STARVED_MARK_SIZE, true,
                color);
  }
}
```

Step 2: Build and run the render tests.

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_render_tests -# "[#park_mesh_test]"; build/linux-debug/tpj_render_tests -# "[#ghost_mesh_test]"`, then the test pass's render file for the mark, by its `-#` tag.
Expected: no build output; the test pass's appendStarvedMark tests pass, and so does every park and ghost mesh test whose world has no starved shop. The tests of appendStarvedMarks, and those of buildParkMesh and buildGhostMesh on worlds with starved shops, fail until Tasks 9 and 10.

### Task 9: Marks in the park mesh

Files:
- Modify: `src/render/park_mesh.cpp`
- Modify: `src/render/park_mesh.h`

Step 1: Add `#include "sim/operations/operations.h"` to park_mesh.cpp's includes, in order.

Step 2: Replace the appendStarvedMarks stub with:

```cpp
void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha) {
  for (const ParkBox &box : parkBoxes(world)) {
    const std::optional<ShopRecord> record = shopRecord(world, box.Key);
    if (record && record->Starved) {
      appendStarvedMark(mesh, box.At,
                        Rgba{STARVED_COLOR.R, STARVED_COLOR.G, STARVED_COLOR.B, alpha});
    }
  }
}
```

Step 3: In buildParkMesh, after the loop over parkBoxes and before `return mesh;`, add `appendStarvedMarks(mesh, world, 1.0f);`.

Step 4: In park_mesh.h, replace buildParkMesh's comment with `// The world's entrances, paths, walkways, and boxes, in that order and each in key order, then a
// starved mark over each starved shop.`

Step 5: Build and run the render tests.

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_render_tests -# "[#<file>]"` for park_mesh_test and each of the test pass's render files, one per invocation.
Expected: no build output; all pass except the buildGhostMesh tests on worlds with starved shops, which pass after Task 10, and any supply.park test, which passes after Task 11.

### Task 10: Marks in the ghost

Files:
- Modify: `src/render/park_mesh.cpp`
- Modify: `src/render/park_mesh.h`

Step 1: In buildGhostMesh, replace

```cpp
    appendWalkways(mesh, makeCandidate(world, queue), GHOST_ALPHA);
```

with

```cpp
    const World candidate = makeCandidate(world, queue);
    appendWalkways(mesh, candidate, GHOST_ALPHA);
    appendStarvedMarks(mesh, candidate, GHOST_ALPHA);
```

Step 2: In park_mesh.h, replace buildGhostMesh's comment with `// The ghost of an edit on a world: translucent in its kind's color when accepted, INVALID_TINT when
// not, and DELETE_TINT for a deletion, followed for an accepted edit by its candidate's walkways
// and starved marks.`

Step 3: Build and run the render tests.

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_render_tests -# "[#<file>]"` for ghost_mesh_test and each of the test pass's render files, one per invocation.
Expected: no build output; all pass except any supply.park test, which passes after Task 11.

### Task 11: supply.park

Files:
- Create: `tests/parks/supply.park`

Step 1: Write the file. It is tests/parks/routes.park, whose shop 7 connects to the guest path and, by its back door, to the backstage path to depot 8, with a second shop, 9, whose front door faces guest path 3 from 3 m away and whose back door is far from any backstage path:

```
tpj-park 1
seed 1
tick 0
next-key 10

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1

[path]
2 kind=guest points=[{x=0 z=123} {x=0 z=100}]
3 kind=guest points=[{x=0 z=110} {x=-30 z=110}]
4 kind=guest points=[{x=-15 z=120} {x=-15 z=95}]
5 kind=guest points=[{x=0 z=100} {x=20 z=100} {x=30 z=80}]
6 kind=backstage points=[{x=12 z=122} {x=12 z=87}]

[box]
7 kind=shop x=6.5 z=115 facing-x=-1 facing-z=0
8 kind=depot x=12 z=80 facing-x=0 facing-z=1
9 kind=shop x=-25 z=116 facing-x=0 facing-z=-1
```

Step 2: Check that it loads and runs, and that the render tests pass.

Run: `build/linux-debug/tpj_scenarios tests/parks/supply.park > /dev/null; echo $?`, then `build/linux-debug/tpj_render_tests -# "[#<file>]"` for each of the test pass's render files, one per invocation.
Expected: `0`; all pass.

### Task 12: Debug panel shop lines

Files:
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`
- Modify: `src/app/main.cpp:443-451`

Step 1: In debug_panel.h, add the includes `"sim/entity_key.h"`, `"sim/operations/operations.h"`, and `<vector>`, and replace DebugStats and the drawDebugPanel comment with:

```cpp
// A shop box's key and its inspection record, as the Debug panel lists them.
struct ShopLine {
  EntityKey Shop = NULL_KEY;
  ShopRecord Record;
};

struct DebugStats {
  uint64_t SimTick = 0;
  Vec3 Focus;
  float Distance = 0.0f;
  std::vector<ShopLine> Shops;
};

// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, and a line for each shop's record. Call between ImGui::NewFrame and
// ImGui::Render.
```

Step 2: In debug_panel.cpp, add `#include <string_view>`, and after `ImGui::Checkbox("Graph", &showGraph);` add:

```cpp
    if (!stats.Shops.empty()) {
      ImGui::Separator();
    }
    for (const ShopLine &line : stats.Shops) {
      const std::string_view limit = limitingFactorName(line.Record.Limit);
      ImGui::Text("Shop %llu: stock %lld, queue %lld, on order %lld, %.*s",
                  static_cast<unsigned long long>(line.Shop),
                  static_cast<long long>(line.Record.Stock),
                  static_cast<long long>(line.Record.Queue),
                  static_cast<long long>(line.Record.OnOrder), static_cast<int>(limit.size()),
                  limit.data());
    }
```

Step 3: In main.cpp, add `#include "sim/operations/operations.h"` to the includes in order, and in drawPanels, after `stats.Distance = camera.Distance;`, add:

```cpp
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (const std::optional<tpj::ShopRecord> record = tpj::shopRecord(world, box.Key)) {
      stats.Shops.push_back({box.Key, *record});
    }
  }
```

Step 4: Build and capture supply.park after 900 ticks with the Windows build, and view it.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; cmake.exe --build --preset windows-debug > /dev/null; timeout 60 build/windows-debug/ThemeParkJones.exe --park tests/parks/supply.park --ticks 900 --capture build/windows-debug/supply.bmp; echo $?`. Convert the capture to PNG and view it with the Read tool.
Expected: no build output, then `0`. The capture shows a violet cube floating over the shop at the left, beside the horizontal guest path, and none over the shop between the guest and backstage paths. The Debug panel shows a line for shop 7 that ends in demand and one for shop 9 that ends in no supply route.

### Task 13: Confirm the acceptance criteria

Run:
- `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
- `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
- `ctest --preset linux-debug`
- `cmake.exe --build --preset windows-debug`
- `ctest.exe --preset windows-debug`
- `scripts/cross-build-check.sh`

Expected: no warnings; every test passes on both builds; the cross-build check passes, with supply.park among its park files.

### Task 14: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, with FEATURE.md, the three changed SPEC.md files, and docs/principles.md as context.

Step 2: Commit once via commit-hygiene, staging `src tests plans` by path:

```
Operations: Publish shop records and mark starved shops
```
