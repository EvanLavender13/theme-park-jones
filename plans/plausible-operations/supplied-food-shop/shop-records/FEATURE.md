# Feature: Shop Records

## Summary

shop-records makes each shop publish an inspection record (decision 0025), and shows a starved shop in the scene. shopRecord gives a shop box's record from the world as it stands: its stock, its queue, its supplies on order, whether it is starved, and its limiting factor, one of demand, supply, service rate, or no supply route. The park mesh draws a cube floating over each starved shop's roof in STARVED_COLOR. A ghost adds its candidate world's marks translucent, so hovering a backstage path with the delete tool shows a mark over each shop the deletion would starve before the click. The Debug panel lists every shop's record while playing. tests/parks/supply.park holds one supplied and one starved shop, and a --capture of it shows the mark on the starved one only.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema between cycles.

1. shopRecord(W, key) is none exactly when key is not the key of a shop box of parkBoxes(W). For a shop box, Stock is the supplies units the shop holds under its own handle, Queue is the guest-visits units it holds under handles that name live entities, OnOrder is inventoryPosition(W, shop) less Stock, and Starved is whether nearestDepot(W, shop) is none.
2. A record's Limit is NoSupplyRoute when it is Starved, and otherwise Demand when Queue is 0, Supply when Stock is below Queue, and ServiceRate when it is not. limitingFactorName gives "demand", "supply", "service rate", and "no supply route" for Demand, Supply, ServiceRate, and NoSupplyRoute.
3. In every tick of meal-service's randomized runs, park edits (tests/sim/support/route_edits.h) interleaved with synthetic guests, every shop box has a record, and its Starved equals Starved for the same key in a new world with the same intent after its first resolution. So a starved mark drawn from intent is never stale.
4. appendStarvedMark(mesh, pose, color) adds the faces appendBox adds for the pose, the size {STARVED_MARK_SIZE, STARVED_MARK_SIZE}, the height STARVED_MARK_SIZE, and the color, in the same order and with the same normals and colors, but with every vertex STARVED_MARK_BASE higher. Then it adds a bottom face: four vertices at height STARVED_MARK_BASE over the footprint's corners, with the normal (0, -1, 0) and the color, and two triangles wound counter-clockwise seen from below. STARVED_MARK_SIZE is 1.5 m and STARVED_MARK_BASE is 5 m, a meter above a shop's roof. A pose with no footprint for that size adds nothing.
5. appendStarvedMarks(mesh, world, alpha) adds, for each shop box of parkBoxes(world) whose record is Starved, in key order, appendStarvedMark at its pose in STARVED_COLOR with its alpha replaced by alpha. buildParkMesh gives the mesh it gave before this feature, followed by appendStarvedMarks with alpha 1.
6. For an edit isAccepted refuses, buildGhostMesh gives the edit's own ghost, as before. For an accepted edit it gives the own ghost, then appendWalkways of the candidate world, makeCandidate(world, a queue holding the edit through queueEdit), with alpha GHOST_ALPHA, then appendStarvedMarks of that candidate with alpha GHOST_ALPHA. So an accepted edit's ghost marks have the positions and normals of the marks buildParkMesh draws in the world the edit gives.
7. STARVED_COLOR is opaque, and differs in red, green, and blue from each other color park_mesh.h and graph_overlay.h declare, including each path and box kind's color.
8. tests/parks/supply.park loads with makeParkSchema into a physically valid world holding two shop boxes, one depot box, and no other box. After resolution, exactly one of its shops' records is Starved, and buildParkMesh draws exactly one starved mark, over that shop. A --capture of it shows the mark over the starved shop only, and the Debug panel lists both shops' records. The cross-build check passes with it.

## Medium

- Emits nothing new. The record is an inspection record (decision 0025), read by tests, by the park and ghost meshes for the starved mark, and by the app's Debug panel, and later by legible-simulation's shop inspector.
- The record reads what the shop's own step reads: its own stocks, the units addressed to it through inventoryPosition, nearestDepot's route distance, whether its guests' keys are live, and intent.
- The render module reads records only through shopRecord, and the app through shopRecord and limitingFactorName.

No park entity reads a record, so it carries no interaction.

## Principle checks

- Principle 1: criterion 3. Starved is a function of intent in every world the runs reach, so the mark, rebuilt when intent changes, is always right. The record adds no state: it is computed from the world, and saves are unchanged.
- Principle 2: criterion 3. Every shop, starved, supplied, or unconnected, has a defined record in every world the runs reach.
- Principles 3 and 6: decision 0025. Review checks that no system or resolver calls shopRecord, and that shopRecord reads only what criterion 1 names.
- Principle 8: criteria 2 and 6. The limiting factor names the cause of a shop's state, and the ghost's marks are exactly those the committed edit gives.
- Principle 10: criterion 8. The cross-build check runs every tests/parks file, supply.park included.

## Spec changes

- src/sim/operations/SPEC.md: a new Inspection record section defines shopRecord, the limiting factor, and limitingFactorName. PLAN.md's Task 1 gives the text.
- src/render/SPEC.md: the Park mesh section says the mesh reads records through shopRecord, defines appendStarvedMark and appendStarvedMarks, and adds the marks to buildParkMesh. The Ghosts section adds STARVED_COLOR to the distinct colors and the candidate's marks to buildGhostMesh.
- src/app/SPEC.md: the Park section says the mesh holds the starved marks, which change only with intent or an opened park, and the Tooling UI section adds the Debug panel's shop lines.

Public interface added to src/sim/operations/operations.h:

```cpp
// What limits a shop's service now: no guests queued, fewer supplies than guests, the service
// rate, or no route to a depot.
enum class LimitingFactor : uint8_t { Demand, Supply, ServiceRate, NoSupplyRoute };

// What a shop publishes about itself for display and tests (decision 0025). Nothing in the park
// reads it.
struct ShopRecord {
  int64_t Stock = 0;
  int64_t Queue = 0;
  int64_t OnOrder = 0;
  bool Starved = false;
  LimitingFactor Limit = LimitingFactor::Demand;

  bool operator==(const ShopRecord &) const = default;
};

// The shop box's record in the world as it stands, or none when the key is not a shop box.
std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop);
// The factor's name for display: "demand", "supply", "service rate", or "no supply route".
std::string_view limitingFactorName(LimitingFactor factor);
```

Added to src/render/park_mesh.h:

```cpp
// A starved shop's mark: a cube of this size floating this high, a meter over the shop's roof.
inline constexpr float STARVED_MARK_SIZE = 1.5f;
inline constexpr float STARVED_MARK_BASE = 5.0f;
inline constexpr Rgba STARVED_COLOR{0.56f, 0.24f, 0.86f, 1.0f};

// Adds a closed cube of STARVED_MARK_SIZE over the pose, its bottom at STARVED_MARK_BASE, or
// nothing when the pose has no footprint.
void appendStarvedMark(ParkMesh &mesh, const Pose &pose, Rgba color);
// Adds a mark over each starved shop of the world, in STARVED_COLOR with the alpha given.
void appendStarvedMarks(ParkMesh &mesh, const World &world, float alpha);
```

## Files affected

- Create: tests/parks/supply.park
- Modify: src/sim/operations/operations.h, src/sim/operations/operations.cpp, src/sim/operations/SPEC.md, src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/SPEC.md, src/app/debug_panel.h, src/app/debug_panel.cpp, src/app/main.cpp, src/app/SPEC.md
- Tests (test pass): tests/sim/operations/, tests/render/, and their CMakeLists.txt. tests/render/park_mesh_test.cpp and tests/render/ghost_mesh_test.cpp hold tests whose worlds have starved shops and which expect buildParkMesh to end with the boxes and buildGhostMesh with the candidate's walkways; criteria 5 and 6 add marks after them, so the test pass updates those tests.

## Dependencies

- meal-service: the queue of guest visits and the randomized runs with synthetic guests. Met.
- supply-chain: nearestDepot and inventoryPosition. Met.
- effortless-building's sketch-a-park: appendBox, the ghost, the Debug panel, and --capture. Met.

## Out of scope

- Showing in a ghost that a starved shop would be supplied again (MILESTONE.md's deepening candidates).
- A depot's inspection record (MILESTONE.md's deepening candidates).
- Clicking a shop to inspect it: legible-simulation's shop inspector.

## Open questions

None.
