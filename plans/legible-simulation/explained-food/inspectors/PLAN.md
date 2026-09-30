# Implementation Plan: Inspectors

## Goal

With the Look tool, clicking a guest or a shop opens the Inspector, which explains it from its inspection record every frame, and marks it on screen.

## Approach

tpj_render's picking.h gains cursorRay, rayEntry, a slab test of a ray against an upright box in its footprint's own frame, and entityAtCursor, the nearest entrance, box, or guest a cursor's ray meets as drawn. tpj_legible gains inspect.h, which turns a key into a subject and a subject into text rows from guestRecord or shopRecord. The app picks on a Look press before the ticks, keeps one subject, draws it in a new Inspector window component, and adds its mark to the ghost mesh it already rebuilds at every tick.

## Placement

Decision 0027 places each behavior this feature adds:

- Picking what the cursor's ray meets: render, picking.h, beside groundAtCursor. It needs the drawn shapes' sizes and heights, which render owns, and the camera, and it reads only intent, parkGuests, and guestRecord, with no window, so tests check it.
- Marking a guest where it is drawn: render, guest_mesh.h's guestPose and appendGuestEntity, shared by the guest mesh and picking.
- Deciding what can be inspected and building the rows: legible, new component legible/inspect.h. It reads only guestRecord, shopRecord, and the world's tick, with no window, so tests check the text.
- Drawing the Inspector window: app, new component app/inspector_window.h, the platform edge. It only renders an Inspection.
- Deciding what a pick does to the subject: legible, inspect.h's pickSubject, so the rule that a guest or shop replaces the subject and anything else leaves it is tested without a window.
- Composing them: main.cpp holds the subject, passes the Look press's pick through entityAtCursor and pickSubject, draws the window, and adds the subject's mark to the ghost mesh with appendEntity and appendGuestEntity, each of which adds nothing for the other's kind, so main.cpp holds no rule of its own. The subject joins what the ghost mesh was last built with, so it is rebuilt when the subject changes, and the rows are built every frame, which costs nothing measurable (RESEARCH.md).

## Tasks

### Task 1: Specify inspectors

Files:
- Modify: `src/legible/SPEC.md`

Step 1: Append to `src/legible/SPEC.md`:

```
## Inspectors

An inspector explains one guest or shop from what it publishes about itself (decision 0025). An InspectorSubject is a Key and a Kind, Guest or Shop, kept after its entity is gone so the inspector can say what it was. inspectorSubject(world, key) gives {key, Guest} when guestRecord(world, key) gives a record, {key, Shop} when shopRecord(world, key) does, and none otherwise, so a depot, an entrance, a path, or a key holding nothing has no inspector. pickSubject(subject, world, key) sets the subject to inspectorSubject(world, key) when the key is given and that gives a subject, and otherwise leaves the subject as it was, so a pick of a guest or shop replaces the subject and a pick of anything else, or of nothing, keeps it.

inspectSubject(world, subject) gives an Inspection: a Title, Gone, whether the subject's entity is gone, Rows, each an InspectorRow of a Label and a Value, and Choices, each a ChoiceRow. It reads the subject's record afresh at each call, so a caller that inspects every frame follows every tick, and it changes nothing. Here a number with n decimals is std::format's "{:.nf}" of it, a count or key is its decimal digits, an enum's display name is the name enumNames gives for it with each '-' replaced by a space, and the seconds of a number of ticks are that number, as a double, times SIM_TICK_SECONDS, with one decimal. The ticks since a tick k are the world's Tick minus k when k is at most the world's Tick, and 0 otherwise.

For a guest, Title is `Guest <key>`. When guestRecord gives none, as once the guest has left the park, Gone is true and Rows and Choices are empty. Otherwise Gone is false, and Rows are, in order, with these Labels:

- Activity: the display name of its Activity.
- Hunger: its Hunger with two decimals.
- Target: `shop <Target>` when Target is not NULL_KEY, and `none` otherwise.
- Stay: `<s> s left`, s the seconds of StayUntil minus the world's Tick, when StayUntil is later than the world's Tick, and `over` otherwise.
- Meals eaten: its MealsEaten.
- Last meal: `none` with no LastMeal, and otherwise `<a> s ago, hunger <b> to <c>`, a the seconds of the ticks since its Tick, and b and c its Before and After with two decimals.
- Last choice: `none` with no LastChoice, and otherwise `<a> s ago at hunger <h>`, a the seconds of the ticks since its Tick, and h its Hunger with two decimals.

Choices has a ChoiceRow for each option of LastChoice, in order, and is empty with no LastChoice. A row's Option is `shop <Shop>` for an option of kind Offer, and the display name of its Kind otherwise. For an Offer, its Relief, Distance, Wait, and Commitment are the option's terms with two decimals, and for another kind they are empty, since carrying on and heading home have no terms. Its Score and Probability are the option's with two decimals, and Picked is true exactly for the row at LastChoice's Picked index. So the table shows every option the guest weighed, each term of its score, its probability, and the one it picked (principle 8).

For a shop, Title is `Shop <key>`. When shopRecord gives none, as once its box is deleted, Gone is true and Rows are empty. Otherwise Gone is false, and Rows are, in order: Stock, Queue, and On order, its Stock, Queue, and OnOrder; Limit, limitingFactorName of its Limit; and Starved, `yes` when Starved is true and `no` otherwise. A shop has no Choices. Keys are never reused (src/sim/SPEC.md), so an entity gone from a world stays gone.
```

### Task 2: Specify picking and a guest's mark

Files:
- Modify: `src/render/SPEC.md`

Step 1: In Contract, replace the paragraph beginning `groundAtCursor, in picking.h,` with

```
cursorRay, in picking.h, gives the ray from the eye through a cursor, for a CameraView, the view's aspect ratio, and the cursor's normalized device coordinates: x from -1 at the left edge to 1 at the right, and y from -1 at the bottom to 1 at the top. Its Origin is Eye, and its Direction is f + r * (x * tan(FovY / 2) * aspect) + u * (y * tan(FovY / 2)), where f is the unit direction from the eye to the target, r is normalize(cross(f, +Y)), and u is cross(r, f); drawFrame's projection, multiply(perspective(FovY, aspect, NearZ, FarZ), lookAt(Eye, Target, +Y)) from render/math.h, maps every point Origin + Direction * t, t > 0, to the cursor's position. groundAtCursor gives the point on the ground under the cursor, the x and z of Origin + Direction * t at t = -Origin.Y / Direction.Y, where the ray meets y = 0. There is none when the eye is not above the ground or the ray does not descend.

rayEntry(ray, pose, size, height) gives where a ray first meets the upright box appendBox draws for the pose, size, and height: the least t at least 0 at which Origin + Direction * t lies over footprintOf(pose, size), edges included, at a height from 0 to the height. It gives none when there is no such t or the pose has no footprint. So a ray starting inside the box meets it at 0. entityAtCursor(world, view, aspect, x, y) gives the key of the solid that cursorRay(view, aspect, x, y) first meets as the park and guest meshes draw them: each entrance of parkEntrances at its pose with ENTRANCE_SIZE and ENTRANCE_HEIGHT, each box of parkBoxes with boxSize and boxHeight of its kind, and each guest of parkGuests whose guestRecord has a Position, at guestPose of that Position with GUEST_SIZE and GUEST_HEIGHT. The one with the least rayEntry wins, a later one replacing an earlier only when strictly less, in that order and each query's in the order it gives. It gives none when the ray meets none. Paths, walkways, and starved marks do not stop the ray, so a click meets what stands on the ground, as the player sees it.
```

Step 2: In Guests, replace

```
appendGuest(mesh, point, hunger) adds exactly what appendBox adds for a Pose at the point's x and z with the default facing, GUEST_SIZE, 0.6 m by 0.6 m, GUEST_HEIGHT, 1.8 m, and guestColor(hunger).
```

with

```
guestPose(point) is the Pose at the point's x and z with the default facing. appendGuest(mesh, point, hunger) adds exactly what appendBox adds for guestPose(point), GUEST_SIZE, 0.6 m by 0.6 m, GUEST_HEIGHT, 1.8 m, and guestColor(hunger). appendGuestEntity(mesh, world, key, color) adds exactly what appendBox adds for guestPose of the Position of the key's guestRecord, GUEST_SIZE, GUEST_HEIGHT, and the color, and nothing when guestRecord gives none or the record has no Position. So it marks a guest where buildGuestMesh draws it.
```

### Task 3: Specify the Inspector in the app

Files:
- Modify: `src/app/SPEC.md`

Step 1: In Park files, replace

```
They empty the command queue, select the current tool again so it drops any hold and drawn points, and frame the camera on the new park's mesh as at start.
```

with

```
They empty the command queue, select the current tool again so it drops any hold and drawn points, forget the Inspector's subject (Tooling UI), and frame the camera on the new park's mesh as at start.
```

Step 2: In Tools, replace

```
a press only when ImGui does not want the mouse, and a release always.
```

with

```
a press only when ImGui does not want the mouse, and a release always. While the tool is Look, a press also picks, at the same moment, with the camera still where the last frame drew it: when entityAtCursor of the world, that view, the window's aspect ratio, and the cursor's normalized device coordinates gives, pickSubject with that key sets the Inspector's subject (Tooling UI, legible/SPEC.md), so a guest or shop replaces it, and a press that meets nothing, or an entity with no subject, such as a depot or the entrance, leaves the Inspector as it was.
```

Step 3: In Tools, replace

```
or the tool's highlighted entity or the Food overlay checkbox differs from when the meshes were last built, the app builds the ghost mesh, buildGhostMesh of the world, the preview's edit, and the preview's candidate when there is an edit, and nothing otherwise, followed by appendEntity of the tool's highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh, and it rebuilds the food overlay (Tooling UI).
```

with

```
or the tool's highlighted entity, the Inspector's subject, or the Food overlay checkbox differs from when the meshes were last built, the app builds the ghost mesh, buildGhostMesh of the world, the preview's edit, and the preview's candidate when there is an edit, and nothing otherwise, followed by appendEntity of the tool's highlighted entity in HIGHLIGHT_TINT, and then appendEntity and appendGuestEntity of the Inspector's subject in HIGHLIGHT_TINT, each adding nothing for the other's kind, and gives it to the renderer with setGhostMesh, and it rebuilds the food overlay (Tooling UI). keepPreview makes the preview again at every tick, so a marked guest's mark moves with it.
```

Step 4: In Tooling UI, replace

```
ImGui keeps each panel where the player last left it in imgui.ini, in the working directory, and a saved position wins. ImGui docking is enabled.
```

with

```
ImGui keeps each panel where the player last left it in imgui.ini, in the working directory, and a saved position wins. ImGui docking is enabled.

The app keeps one Inspector subject, none at start, which a Look press sets (Tools). While it has one, each frame, after the panels, the app draws the Inspector window from inspectSubject of the world and the subject (legible/SPEC.md, Inspectors): the Inspection's Title, then `No longer in the park` when it is Gone, and otherwise a table of its Rows, each label beside its value, and, when it has Choices, `Last choice` above a table with the columns Option, Relief, Distance, Wait, Commitment, Score, and Chance after a first, unnamed one, a row for each ChoiceRow in order with its texts, and `>` in the first column of the Picked row alone. So the Inspector follows every tick. Its close button forgets the subject. Selecting another tool leaves it open. With no saved layout, it starts at the window's bottom left corner.
```

### Task 4: Create the inspector interface

Files:
- Create: `src/legible/inspect.h`
- Create: `src/legible/inspect.cpp`
- Modify: `src/legible/CMakeLists.txt`

Step 1: Create `src/legible/inspect.h`.

```cpp
#ifndef TPJ_LEGIBLE_INSPECT_H
#define TPJ_LEGIBLE_INSPECT_H

#include "sim/entity_key.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// What an inspector explains: a guest or a shop.
enum class SubjectKind : uint8_t { Guest, Shop };

// The entity an inspector explains and what it is, kept after the entity is gone.
struct InspectorSubject {
  EntityKey Key = NULL_KEY;
  SubjectKind Kind = SubjectKind::Guest;

  bool operator==(const InspectorSubject &) const = default;
};

// One line of an inspector: what it shows and its value, as text.
struct InspectorRow {
  std::string Label;
  std::string Value;

  bool operator==(const InspectorRow &) const = default;
};

// One option of a guest's last choice, as text: the option, its terms, score, and probability,
// and whether the guest picked it. Carrying on and heading home have empty terms.
struct ChoiceRow {
  std::string Option;
  std::string Relief;
  std::string Distance;
  std::string Wait;
  std::string Commitment;
  std::string Score;
  std::string Probability;
  bool Picked = false;

  bool operator==(const ChoiceRow &) const = default;
};

// What an inspector shows: its title, whether its entity is gone, its lines, and a guest's last
// choice.
struct Inspection {
  std::string Title;
  bool Gone = false;
  std::vector<InspectorRow> Rows;
  std::vector<ChoiceRow> Choices;

  bool operator==(const Inspection &) const = default;
};

// The subject for the key: a guest when the key holds one, a shop when it holds a shop box, and
// none otherwise.
std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key);

// Replaces the subject with the picked key's when it has one, and otherwise leaves it.
void pickSubject(std::optional<InspectorSubject> &subject, const World &world,
                 std::optional<EntityKey> key);

// The subject's inspector, built from its inspection record and the world's tick, or saying its
// entity is gone when the world gives no record. Changes nothing.
Inspection inspectSubject(const World &world, const InspectorSubject &subject);

} // namespace tpj

#endif
```

Step 2: Create `src/legible/inspect.cpp` with stubs.

```cpp
#include "legible/inspect.h"

namespace tpj {

std::optional<InspectorSubject> inspectorSubject(const World & /*world*/, EntityKey /*key*/) {
  return std::nullopt;
}

void pickSubject(std::optional<InspectorSubject> & /*subject*/, const World & /*world*/,
                 std::optional<EntityKey> /*key*/) {}

Inspection inspectSubject(const World & /*world*/, const InspectorSubject & /*subject*/) {
  return {};
}

} // namespace tpj
```

Step 3: In `src/legible/CMakeLists.txt`, replace

```
    food.cpp
    path_place.cpp
    preview.cpp)
```

with

```
    food.cpp
    inspect.cpp
    path_place.cpp
    preview.cpp)
```

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build succeeds with no warnings.

### Task 5: Declare picking and a guest's mark

Files:
- Modify: `src/render/picking.h`
- Modify: `src/render/picking.cpp`
- Modify: `src/render/guest_mesh.h`
- Modify: `src/render/guest_mesh.cpp`

Step 1: Replace the whole of `src/render/picking.h` with

```cpp
#ifndef TPJ_RENDER_PICKING_H
#define TPJ_RENDER_PICKING_H

#include "render/math.h"
#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// A ray from the eye through a cursor: the points Origin + Direction * t for t at least 0.
struct CursorRay {
  Vec3 Origin;
  Vec3 Direction;
};

// The ray from the eye through a cursor at normalized device coordinates, x from -1 at the left to
// 1 at the right and y from -1 at the bottom to 1 at the top.
CursorRay cursorRay(const CameraView &view, float aspect, float ndcX, float ndcY);

// The point on the ground under a cursor at normalized device coordinates, where its ray meets the
// ground. None when the eye is not above the ground or the ray does not descend.
std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY);

// Where the ray first meets the upright box appendBox draws for the pose, size, and height: the
// least ray parameter at least 0 inside it, edges included. None when the ray misses it or the
// pose has no footprint.
std::optional<double> rayEntry(const CursorRay &ray, const Pose &pose, FootprintSize size,
                               float height);

// The entrance, box, or guest the cursor's ray first meets, as the park and guest meshes draw
// them, or none when it meets none.
std::optional<EntityKey> entityAtCursor(const World &world, const CameraView &view, float aspect,
                                        float ndcX, float ndcY);

} // namespace tpj

#endif
```

Step 2: In `src/render/picking.cpp`, replace the namespace's body with the refactored groundAtCursor and stubs:

```cpp
CursorRay cursorRay(const CameraView &view, float aspect, float ndcX, float ndcY) {
  const Vec3 forward = normalize(view.Target - view.Eye);
  const Vec3 right = normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
  const Vec3 up = cross(right, forward);
  const float tanHalf = tanf(0.5f * view.FovY);
  return {view.Eye, forward + right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf)};
}

std::optional<ParkPoint> groundAtCursor(const CameraView &view, float aspect, float ndcX,
                                        float ndcY) {
  const CursorRay ray = cursorRay(view, aspect, ndcX, ndcY);
  if (ray.Origin.Y <= 0.0f || ray.Direction.Y >= 0.0f) {
    return std::nullopt;
  }
  const float t = -ray.Origin.Y / ray.Direction.Y;
  return ParkPoint{ray.Origin.X + ray.Direction.X * t, ray.Origin.Z + ray.Direction.Z * t};
}

std::optional<double> rayEntry(const CursorRay & /*ray*/, const Pose & /*pose*/,
                               FootprintSize /*size*/, float /*height*/) {
  return std::nullopt;
}

std::optional<EntityKey> entityAtCursor(const World & /*world*/, const CameraView & /*view*/,
                                        float /*aspect*/, float /*ndcX*/, float /*ndcY*/) {
  return std::nullopt;
}
```

Step 3: In `src/render/guest_mesh.h`, replace

```cpp
// Adds a guest's box standing at the point, in its hunger's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger);
```

with

```cpp
// Where a guest's box stands for its ground point: there, with the default facing.
Pose guestPose(GroundPoint point);
// Adds a guest's box standing at the point, in its hunger's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger);
// Adds the guest the key holds where buildGuestMesh draws it, in the color. Nothing when the key
// holds no guest or its place does not resolve.
void appendGuestEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color);
```

and replace `#include "sim/medium/network.h"` with

```cpp
#include "sim/entity_key.h"
#include "sim/medium/network.h"
```

Step 4: In `src/render/guest_mesh.cpp`, replace appendGuest with guestPose, the refactored appendGuest, and a stub:

```cpp
Pose guestPose(GroundPoint point) {
  Pose pose;
  pose.X = point.X;
  pose.Z = point.Z;
  return pose;
}

void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger) {
  appendBox(mesh, guestPose(point), GUEST_SIZE, GUEST_HEIGHT, guestColor(hunger));
}

void appendGuestEntity(ParkMesh & /*mesh*/, const World & /*world*/, EntityKey /*key*/,
                       Rgba /*color*/) {}
```

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe -# "[#picking_test],[#guest_mesh_test]" 2>&1 | tr -d '\r' | tail -3`
Expected: the build succeeds with no warnings, and `All tests passed`: groundAtCursor and appendGuest are unchanged.

### Task 6: Run the test pass

Dispatch the test-writer agent with only these paths: `plans/legible-simulation/explained-food/inspectors/FEATURE.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`, `src/sim/SPEC.md`, `src/sim/park/SPEC.md`, `src/sim/operations/SPEC.md`, `src/sim/guests/SPEC.md`, `src/legible/inspect.h`, `src/render/picking.h`, `src/render/guest_mesh.h`, `src/render/park_mesh.h`, `docs/principles.md`, `docs/conventions.md`.

Expected: the agent adds tests to tests/legible/ for criteria 5 to 9 and to tests/render/ for criteria 1 to 4, and reports the files. They build against the stubs. Those needing a subject, rows, an entry, a pick, or a guest's mark fail until Tasks 7 to 9; criterion 1's and those expecting none may pass against the stubs. Criterion 10 is checked by hand in Task 12.

### Task 7: Implement the inspector

Files:
- Modify: `src/legible/inspect.cpp`

Step 1: Replace the whole of `src/legible/inspect.cpp` with

```cpp
#include "legible/inspect.h"

#include "sim/guests/guests.h"
#include "sim/operations/operations.h"

#include <algorithm>
#include <format>
#include <stddef.h>
#include <utility>

namespace tpj {
namespace {

std::string keyText(EntityKey key) { return std::format("{}", static_cast<uint64_t>(key)); }

std::string twoDecimals(double value) { return std::format("{:.2f}", value); }

// The seconds a number of ticks lasts, with one decimal.
std::string seconds(uint64_t ticks) {
  return std::format("{:.1f}", static_cast<double>(ticks) * SIM_TICK_SECONDS);
}

// The ticks from a past tick to the world's, and 0 for a later one.
uint64_t ticksSince(const World &world, uint64_t tick) {
  return tick <= world.Tick ? world.Tick - tick : 0;
}

// An enum's name for display, each '-' a space.
template <typename Enum> std::string displayName(Enum value) {
  std::string name(enumNames(value)[static_cast<size_t>(value)]);
  std::ranges::replace(name, '-', ' ');
  return name;
}

std::vector<InspectorRow> guestRows(const World &world, const GuestRecord &record) {
  std::vector<InspectorRow> rows;
  rows.push_back({"Activity", displayName(record.Activity)});
  rows.push_back({"Hunger", twoDecimals(record.Hunger)});
  rows.push_back(
      {"Target", record.Target == NULL_KEY ? "none" : "shop " + keyText(record.Target)});
  rows.push_back({"Stay", record.StayUntil > world.Tick
                              ? seconds(record.StayUntil - world.Tick) + " s left"
                              : "over"});
  rows.push_back({"Meals eaten", std::format("{}", record.MealsEaten)});
  if (const std::optional<GuestMeal> &meal = record.LastMeal) {
    rows.push_back({"Last meal", std::format("{} s ago, hunger {:.2f} to {:.2f}",
                                             seconds(ticksSince(world, meal->Tick)),
                                             meal->Before, meal->After)});
  } else {
    rows.push_back({"Last meal", "none"});
  }
  if (const std::optional<GuestChoice> &choice = record.LastChoice) {
    rows.push_back({"Last choice", std::format("{} s ago at hunger {:.2f}",
                                               seconds(ticksSince(world, choice->Tick)),
                                               choice->Hunger)});
  } else {
    rows.push_back({"Last choice", "none"});
  }
  return rows;
}

// Each option the guest weighed, its terms only for an offer, and the one it picked.
std::vector<ChoiceRow> choiceRows(const GuestChoice &choice) {
  std::vector<ChoiceRow> rows;
  for (size_t index = 0; index < choice.Options.size(); ++index) {
    const ChoiceOption &option = choice.Options[index];
    ChoiceRow row;
    if (option.Kind == ChoiceKind::Offer) {
      row.Option = "shop " + keyText(option.Shop);
      row.Relief = twoDecimals(option.Relief);
      row.Distance = twoDecimals(option.Distance);
      row.Wait = twoDecimals(option.Wait);
      row.Commitment = twoDecimals(option.Commitment);
    } else {
      row.Option = displayName(option.Kind);
    }
    row.Score = twoDecimals(option.Score);
    row.Probability = twoDecimals(option.Probability);
    row.Picked = index == choice.Picked;
    rows.push_back(std::move(row));
  }
  return rows;
}

std::vector<InspectorRow> shopRows(const ShopRecord &record) {
  return {{"Stock", std::format("{}", record.Stock)},
          {"Queue", std::format("{}", record.Queue)},
          {"On order", std::format("{}", record.OnOrder)},
          {"Limit", std::string(limitingFactorName(record.Limit))},
          {"Starved", record.Starved ? "yes" : "no"}};
}

} // namespace

std::optional<InspectorSubject> inspectorSubject(const World &world, EntityKey key) {
  if (guestRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Guest};
  }
  if (shopRecord(world, key)) {
    return InspectorSubject{key, SubjectKind::Shop};
  }
  return std::nullopt;
}

void pickSubject(std::optional<InspectorSubject> &subject, const World &world,
                 std::optional<EntityKey> key) {
  if (!key) {
    return;
  }
  if (const std::optional<InspectorSubject> picked = inspectorSubject(world, *key)) {
    subject = picked;
  }
}

Inspection inspectSubject(const World &world, const InspectorSubject &subject) {
  Inspection inspection;
  if (subject.Kind == SubjectKind::Guest) {
    inspection.Title = "Guest " + keyText(subject.Key);
    const std::optional<GuestRecord> record = guestRecord(world, subject.Key);
    inspection.Gone = !record;
    if (record) {
      inspection.Rows = guestRows(world, *record);
      if (record->LastChoice) {
        inspection.Choices = choiceRows(*record->LastChoice);
      }
    }
    return inspection;
  }
  inspection.Title = "Shop " + keyText(subject.Key);
  const std::optional<ShopRecord> record = shopRecord(world, subject.Key);
  inspection.Gone = !record;
  if (record) {
    inspection.Rows = shopRows(*record);
  }
  return inspection;
}

} // namespace tpj
```

Step 2: Build and run the legible tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 8: Implement picking

Files:
- Modify: `src/render/picking.cpp`

Step 1: Replace the includes of `src/render/picking.cpp` with

```cpp
#include "render/picking.h"

#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "sim/guests/guests.h"
#include "sim/park/geometry.h"

#include <algorithm>
#include <limits>
#include <math.h>
#include <utility>
```

Step 2: Add, after `namespace tpj {`,

```cpp
namespace {

// Narrows [near, far] to the ray parameters at which a coordinate, starting at origin and moving
// by step per unit, lies within [low, high]. False when that leaves no parameter.
bool clipSlab(double origin, double step, double low, double high, double &near, double &far) {
  if (step == 0.0) {
    return origin >= low && origin <= high;
  }
  double enter = (low - origin) / step;
  double leave = (high - origin) / step;
  if (enter > leave) {
    std::swap(enter, leave);
  }
  near = std::max(near, enter);
  far = std::min(far, leave);
  return near <= far;
}

} // namespace

```

Step 3: Replace the rayEntry and entityAtCursor stubs with

```cpp
std::optional<double> rayEntry(const CursorRay &ray, const Pose &pose, FootprintSize size,
                               float height) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  if (!footprint) {
    return std::nullopt;
  }
  // In the footprint's own frame: along Forward, across it along Right, and up.
  const double dx = ray.Origin.X - pose.X;
  const double dz = ray.Origin.Z - pose.Z;
  const auto along = [](double x, double z, ParkPoint axis) { return x * axis.X + z * axis.Z; };
  const ParkPoint forward = footprint->Forward;
  const ParkPoint right = footprint->Right;
  double near = 0.0;
  double far = std::numeric_limits<double>::infinity();
  const bool meets =
      clipSlab(along(dx, dz, forward), along(ray.Direction.X, ray.Direction.Z, forward),
               -0.5 * size.Depth, 0.5 * size.Depth, near, far) &&
      clipSlab(along(dx, dz, right), along(ray.Direction.X, ray.Direction.Z, right),
               -0.5 * size.Width, 0.5 * size.Width, near, far) &&
      clipSlab(ray.Origin.Y, ray.Direction.Y, 0.0, height, near, far);
  if (!meets) {
    return std::nullopt;
  }
  return near;
}

std::optional<EntityKey> entityAtCursor(const World &world, const CameraView &view, float aspect,
                                        float ndcX, float ndcY) {
  const CursorRay ray = cursorRay(view, aspect, ndcX, ndcY);
  std::optional<EntityKey> nearest;
  double nearestEntry = 0.0;
  // A later solid replaces an earlier one only when strictly nearer.
  const auto consider = [&nearest, &nearestEntry](EntityKey key, std::optional<double> entry) {
    if (entry && (!nearest || *entry < nearestEntry)) {
      nearest = key;
      nearestEntry = *entry;
    }
  };
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    consider(entrance.Key, rayEntry(ray, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT));
  }
  for (const ParkBox &box : parkBoxes(world)) {
    consider(box.Key, rayEntry(ray, box.At, boxSize(box.Kind), boxHeight(box.Kind)));
  }
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    if (record && record->Position) {
      consider(guest, rayEntry(ray, guestPose(*record->Position), GUEST_SIZE, GUEST_HEIGHT));
    }
  }
  return nearest;
}
```

Step 4: Build and run the picking tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe -# "[#picking_test]" 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 9: Implement a guest's mark

Files:
- Modify: `src/render/guest_mesh.cpp`

Step 1: Replace the appendGuestEntity stub with

```cpp
void appendGuestEntity(ParkMesh &mesh, const World &world, EntityKey key, Rgba color) {
  const std::optional<GuestRecord> record = guestRecord(world, key);
  if (record && record->Position) {
    appendBox(mesh, guestPose(*record->Position), GUEST_SIZE, GUEST_HEIGHT, color);
  }
}
```

Step 2: Build and run the render tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 10: Create the Inspector window

Files:
- Create: `src/app/inspector_window.h`
- Create: `src/app/inspector_window.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Create `src/app/inspector_window.h`.

```cpp
#ifndef TPJ_APP_INSPECTOR_WINDOW_H
#define TPJ_APP_INSPECTOR_WINDOW_H

#include "legible/inspect.h"

namespace tpj {

// Draws the Inspector window: the inspection's title, then that its entity is gone or its rows,
// and a guest's last choice as a table with the picked option marked. Returns false when the
// player closes it. Call between ImGui::NewFrame and ImGui::Render.
bool drawInspector(const Inspection &inspection);

} // namespace tpj

#endif
```

Step 2: Create `src/app/inspector_window.cpp`.

```cpp
#include "app/inspector_window.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace tpj {
namespace {

void drawRows(const std::vector<InspectorRow> &rows) {
  if (!ImGui::BeginTable("rows", 2, ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const InspectorRow &row : rows) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.Label.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.Value.c_str());
  }
  ImGui::EndTable();
}

// The guest's last choice, one option to a row, the picked one marked in the first column.
void drawChoices(const std::vector<ChoiceRow> &choices) {
  ImGui::TextUnformatted("Last choice");
  if (!ImGui::BeginTable("choices", 8,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const char *heading :
       {"", "Option", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance"}) {
    ImGui::TableSetupColumn(heading);
  }
  ImGui::TableHeadersRow();
  for (const ChoiceRow &choice : choices) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(choice.Picked ? ">" : "");
    for (const std::string *cell : {&choice.Option, &choice.Relief, &choice.Distance, &choice.Wait,
                                    &choice.Commitment, &choice.Score, &choice.Probability}) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(cell->c_str());
    }
  }
  ImGui::EndTable();
}

} // namespace

bool drawInspector(const Inspection &inspection) {
  bool open = true;
  // At the bottom left, clear of the Tools panel above it and the Debug panel at the top right.
  ImGui::SetNextWindowPos(ImVec2(12.0f, ImGui::GetIO().DisplaySize.y - 12.0f),
                          ImGuiCond_FirstUseEver, ImVec2(0.0f, 1.0f));
  if (ImGui::Begin("Inspector", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(inspection.Title.c_str());
    if (inspection.Gone) {
      ImGui::TextUnformatted("No longer in the park");
    } else {
      drawRows(inspection.Rows);
      if (!inspection.Choices.empty()) {
        drawChoices(inspection.Choices);
      }
    }
  }
  ImGui::End();
  return open;
}

} // namespace tpj
```

Step 3: In `src/app/CMakeLists.txt`, replace

```
    food_tooltip.cpp
    main.cpp
```

with

```
    food_tooltip.cpp
    inspector_window.cpp
    main.cpp
```

### Task 11: Compose the Inspector in the main loop

Files:
- Modify: `src/app/main.cpp`

Step 1: Replace

```cpp
#include "app/food_tooltip.h"
```

with

```cpp
#include "app/food_tooltip.h"
#include "app/inspector_window.h"
```

and

```cpp
#include "legible/food.h"
```

with

```cpp
#include "legible/food.h"
#include "legible/inspect.h"
```

Step 2: Replace DrawnPreview, with its comment, by

```cpp
// The highlight, inspected subject, and overlay choice the ghost and overlay meshes were last
// built with.
struct DrawnPreview {
  std::optional<tpj::EntityKey> Highlight;
  std::optional<tpj::InspectorSubject> Inspected;
  bool FoodOverlay = false;

  bool operator==(const DrawnPreview &) const = default;
};
```

Step 3: Replace ghostMesh, with its comment, by

```cpp
// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, then the
// tool's highlight, then the inspected guest or shop's mark.
tpj::ParkMesh ghostMesh(const tpj::World &world, const tpj::ToolState &tool,
                        const tpj::Preview &preview,
                        const std::optional<tpj::InspectorSubject> &inspected) {
  tpj::ParkMesh mesh;
  if (preview.Edit) {
    mesh = tpj::buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  if (const std::optional<tpj::EntityKey> highlight = tpj::highlightedEntity(tool, world)) {
    tpj::appendEntity(mesh, world, *highlight, tpj::HIGHLIGHT_TINT);
  }
  // Each adds nothing for a key of the other's kind.
  if (inspected) {
    tpj::appendEntity(mesh, world, inspected->Key, tpj::HIGHLIGHT_TINT);
    tpj::appendGuestEntity(mesh, world, inspected->Key, tpj::HIGHLIGHT_TINT);
  }
  return mesh;
}
```

Step 4: Replace updatePreviewMeshes, with its comment, by

```cpp
// Rebuilds the ghost and food overlay meshes when the preview was made again, or the tool's
// highlight, the inspected subject, or the overlay's checkbox differs from when they were last
// built. Returns false if an upload failed.
bool updatePreviewMeshes(tpj::Renderer &renderer, const tpj::World &world,
                         const tpj::ToolState &tool, const tpj::Preview &preview, bool remade,
                         const std::optional<tpj::InspectorSubject> &inspected, bool showOverlay,
                         std::optional<DrawnPreview> &drawn) {
  const DrawnPreview current{tpj::highlightedEntity(tool, world), inspected, showOverlay};
  if (!remade && drawn && *drawn == current) {
    return true;
  }
  drawn = current;
  return tpj::setGhostMesh(renderer, ghostMesh(world, tool, preview, inspected)) &&
         tpj::setOverlayMesh(renderer,
                             foodOverlayMesh(tpj::previewedWorld(world, preview), showOverlay));
}
```

Step 5: Replace groundUnderCursor, with its comment, by

```cpp
// Where the cursor lies in normalized device coordinates, and the window's aspect ratio.
struct CursorNdc {
  float X = 0.0f;
  float Y = 0.0f;
  float Aspect = 1.0f;
};

// The cursor, or none while ImGui wants the mouse or the window has no size.
std::optional<CursorNdc> cursorNdc(SDL_Window *window) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0) {
    return std::nullopt;
  }
  float x = 0.0f;
  float y = 0.0f;
  SDL_GetMouseState(&x, &y);
  return CursorNdc{2.0f * x / static_cast<float>(width) - 1.0f,
                   1.0f - 2.0f * y / static_cast<float>(height),
                   static_cast<float>(width) / static_cast<float>(height)};
}

// The ground under the cursor, or none while ImGui wants the mouse or the cursor meets no ground.
std::optional<tpj::ParkPoint> groundUnderCursor(SDL_Window *window, const tpj::CameraView &view) {
  const std::optional<CursorNdc> cursor = cursorNdc(window);
  if (!cursor) {
    return std::nullopt;
  }
  return tpj::groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
}

// The view the camera gives.
tpj::CameraView cameraView(const tpj::OrbitCamera &camera) {
  tpj::CameraView view;
  view.Eye = tpj::orbitCameraEye(camera);
  view.Target = camera.Focus;
  return view;
}

// The entity the cursor's ray first meets in the view, or none while ImGui wants the mouse.
std::optional<tpj::EntityKey> entityUnderCursor(SDL_Window *window, const tpj::World &world,
                                                const tpj::CameraView &view) {
  const std::optional<CursorNdc> cursor = cursorNdc(window);
  if (!cursor) {
    return std::nullopt;
  }
  return tpj::entityAtCursor(world, view, cursor->Aspect, cursor->X, cursor->Y);
}
```

Step 6: Replace buildUi, with its comment, by

```cpp
// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, the Inspector while it has a subject, forgetting it when closed, the graph over the
// scene while it is shown, the food tooltip at the cursor while the overlay is, on the preview's
// candidate when it has one, and a shop ghost's context.
void buildUi(SDL_Window *window, const tpj::World &world, const tpj::OrbitCamera &camera,
             const tpj::CameraView &view, ShownViews &shown, tpj::ToolState &tool,
             const tpj::Preview &preview, std::optional<tpj::InspectorSubject> &inspected) {
  tpj::beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(window, world, camera, shown, tool);
  if (inspected && !tpj::drawInspector(tpj::inspectSubject(world, *inspected))) {
    inspected.reset();
  }
  if (shown.Graph) {
    drawGraph(world, view);
  }
  if (shown.FoodOverlay) {
    tpj::drawFoodTooltip(tpj::previewedWorld(world, preview), groundUnderCursor(window, view));
  }
  if (preview.Shop) {
    tpj::drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
}
```

Step 7: In runLoop, replace

```cpp
  std::optional<DrawnPreview> drawnPreview;
```

with

```cpp
  std::optional<DrawnPreview> drawnPreview;
  std::optional<tpj::InspectorSubject> inspected;
```

Step 8: In runLoop, replace

```cpp
      guestTick.reset();
      kept = {};
```

with

```cpp
      guestTick.reset();
      kept = {};
      inspected.reset();
```

Step 9: In runLoop, replace

```cpp
    useButtons(tool, world, commands, buttons);
```

with

```cpp
    useButtons(tool, world, commands, buttons);
    // The camera has not moved yet this frame, so a Look press picks from the view on screen.
    if (buttons.Pressed && tool.Kind == tpj::ToolKind::None) {
      tpj::pickSubject(inspected, world,
                       entityUnderCursor(renderer.Window, world, cameraView(camera)));
    }
```

Step 10: In runLoop, replace

```cpp
    tpj::CameraView view;
    view.Eye = tpj::orbitCameraEye(camera);
    view.Target = camera.Focus;
```

with

```cpp
    const tpj::CameraView view = cameraView(camera);
```

Step 11: In runLoop, replace

```cpp
    buildUi(renderer.Window, world, camera, view, shown, tool, kept.Made);
```

with

```cpp
    buildUi(renderer.Window, world, camera, view, shown, tool, kept.Made, inspected);
```

and

```cpp
    if (!updatePreviewMeshes(renderer, world, tool, kept.Made, remade, shown.FoodOverlay,
                             drawnPreview) ||
```

with

```cpp
    if (!updatePreviewMeshes(renderer, world, tool, kept.Made, remade, inspected,
                             shown.FoodOverlay, drawnPreview) ||
```

Step 12: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

Step 13: Capture warm.park, from `build/windows-debug`.

Run: `cd build/windows-debug && timeout 120 ./ThemeParkJones.exe --park ../../tests/parks/warm.park --ticks 3000 --capture inspect.bmp; echo $?`
Expected: `0`. With no click there is no Inspector, so the capture shows the park as before. Convert it to PNG in the scratchpad, view it, and delete the BMP.

### Task 12: Check the Inspector by hand

Step 1: Ask Evan to run, from `build/windows-debug`, `./ThemeParkJones.exe --park ../../tests/parks/warm.park --ticks 3000`, and with Look selected to:

- click a walking guest's body, from a low camera, then click another guest;
- click the shop's roof, then its side;
- click empty ground, a path, the depot, and the entrance;
- click a guest standing behind the depot, on the depot;
- select Place shop, then Look again;
- delete the inspected shop with Delete, or wait on an inspected guest until it leaves;
- press the Inspector's close button.

Expected (criterion 10): each guest or shop click opens or switches the Inspector at the bottom left, its rows changing as the park runs, and marks the guest or shop white, the mark walking with the guest; a guest's Last choice table marks its picked option with `>`. The empty ground, path, depot, and entrance clicks, and the click on the depot in front of a guest, leave the Inspector as it was. Selecting another tool leaves it open. A deleted shop or a departed guest shows `No longer in the park` with no mark. Closing it removes the mark. Anything else is a deviation to report.

### Task 13: Verify the feature

Step 1: Format.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
Expected: no output.

Step 2: Build and test windows-debug.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "warning|error" ; ctest.exe --preset windows-debug 2>&1 | tr -d '\r' | tail -4`
Expected: no warnings or errors, and `100% tests passed`.

Step 3: Build and test linux-debug.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug 2>&1 | tail -4`
Expected: no warnings or errors, and `100% tests passed`.

Step 4: Tidy.

Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

### Task 14: Commit

Step 1: Stage exactly the feature's paths: `git add plans/legible-simulation/explained-food/inspectors src/legible src/render src/app tests/legible tests/render`, and check `git status --short` shows nothing else staged.

Step 2: Dispatch the reviewer on the staged diff, `git diff --cached`, with FEATURE.md, src/legible/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, docs/principles.md, and docs/decisions/0027-code-architecture.md as context, and give its findings to Evan verbatim.

Step 3: Commit through commit-hygiene with the subject `Legible: Inspect guests and shops` and a body of at most 72-character lines saying that a Look click picks the entrance, box, or guest the cursor's ray first meets as drawn, that inspectSubject builds a guest's or shop's rows and a guest's last choice from their records every frame, and that the app draws them in the Inspector window and marks the subject, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
