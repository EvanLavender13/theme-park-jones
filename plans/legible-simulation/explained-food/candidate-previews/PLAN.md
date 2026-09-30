# Implementation Plan: Candidate Previews

## Goal

While a tentative edit is accepted, the ghost, the food overlay, and the food tooltip show one candidate world made each frame, and a shop ghost's tooltip gives its hungry footfall and supply route.

## Approach

tpj_legible gains preview.h: previewEdit makes the candidate of an accepted edit and a shop ghost's context, headless and tested, and previewedWorld picks the world to explain. buildGhostMesh gains an overload that takes the candidate instead of making its own. main.cpp makes the preview once a frame after the pointer moves and passes it to the ghost, the overlay, and the tooltips, replacing its ghost cache, and a new app component draws the shop context.

## Placement

Decision 0027 places each behavior this feature adds:

- Making the candidate of an accepted edit and a shop ghost's context: legible, new component legible/preview.h. It reads intent, the networks, fields, and operations' nearestDepot, and makes the candidate with makeCandidate, with no window, so tests check it.
- Drawing a ghost's walkways and marks from a given candidate: render, park_mesh.h's buildGhostMesh overload. Render still links no tpj_legible: it takes a World.
- Drawing the shop context: app, new component app/shop_context_tooltip.h, the platform edge. It only renders a ShopContext.
- Composing them: main.cpp makes the preview as a local of each frame and hands it on. The preview replaces the ghost cache main.cpp held (DrawnGhost and updateGhostMesh), so main.cpp loses a hand-reset cache and gains no state or concern of its own.

## Tasks

### Task 1: Specify previews

Files:
- Modify: `src/legible/SPEC.md`

Step 1: In the module's opening paragraph, replace

```
It reads a world only through the medium's fields and networks, parkNetwork, and park intent (decision 0025), takes it by const reference, and changes nothing, so nothing it computes enters a world's state, hash, or save (principle 1).
```

with

```
It reads a world only through the medium's fields and networks, parkNetwork, park intent, and the owning modules' public queries, such as isAccepted and nearestDepot, and makes candidate worlds only with makeCandidate (decision 0025). It takes a world by const reference and changes nothing, so nothing it computes enters a world's state, hash, or save (principle 1).
```

Step 2: Append this section at the end of the file.

```
## Previews

A preview shows what a tentative edit would do before it is committed (decision 0025). The candidate of an edit on a world is makeCandidate(world, a queue holding the edit through queueEdit): a copy with the edit applied and resolved by the owning modules' resolvers, never stepped, which is what committing the edit at the next cycle gives (src/sim/SPEC.md). previewEdit(world, edit) gives a Preview holding Edit, the edit given; Candidate, the candidate of the edit on the world when the edit is given and isAccepted(world, edit), and none otherwise; and Shop, shopContext(world, the candidate, the edit) when Candidate holds one, and none otherwise. previewedWorld(world, preview) is the preview's Candidate when it holds one and the world otherwise, so what explains the committed park explains the candidate the same way.

shopContext(world, candidate, edit) tells what a shop ghost would find. Its shop is, for an AddBox of kind Shop, the lowest key of a box of parkBoxes(candidate) that no box of parkBoxes(world) has, and for a MoveBox whose Box is the key of a box of kind Shop in parkBoxes(world), that Box. For any other edit, or an AddBox for which the candidate holds no such box, it gives none. Otherwise it gives a ShopContext holding Shop, the shop, Connection, Footfall, and Supply. With C the carrier of parkNetwork(candidate, PathKind::Guest) keyed connectorKey(Shop, Face::Front), the shop's guest connector, Connection is the first place, in order, of stopPlaces of the node of C's last stop whose carrier is the key of a path of kind Guest in parkPaths(candidate): where the connector meets the guest path. It is none when there is no C or no such place. Footfall is fieldValue of HungryFootfall on parkNetwork(world, PathKind::Guest) at Connection, in the world, not the candidate, and 0.0 when there is no Connection. Footfall is an average over past ticks, which a candidate, never stepped, holds only where the last step published it, and an AddBox or MoveBox leaves the paths as they were, so Connection lies on the same line in the world. Supply is nearestDepot(candidate, Shop): the depot with the least supply route length, the least backstage route distance from the shop's backstage anchor, and that length, or none when the edit would leave the shop starved.
```

### Task 2: Specify the ghost's candidate

Files:
- Modify: `src/render/SPEC.md`

Step 1: In the Ghosts section, replace

```
buildGhostMesh gives an edit's ghost on a world: the edit's own ghost, followed, when isAccepted(world, edit), by appendWalkways of the candidate world, makeCandidate(world, a queue holding the edit through queueEdit), with alpha GHOST_ALPHA, and then appendStarvedMarks of that candidate with alpha GHOST_ALPHA.
```

with

```
buildGhostMesh(world, edit, candidate) gives an edit's ghost on a world with the candidate world it shows: the edit's own ghost, followed, when candidate holds a world, by appendWalkways of that world with alpha GHOST_ALPHA, and then appendStarvedMarks of it with alpha GHOST_ALPHA. buildGhostMesh(world, edit) is buildGhostMesh(world, edit, candidate) with the candidate world makeCandidate(world, a queue holding the edit through queueEdit) when isAccepted(world, edit), and none otherwise. So a caller that already holds that candidate, as the app does for its preview, passes it rather than making another.
```

### Task 3: Specify the preview in the app

Files:
- Modify: `src/app/SPEC.md`

Step 1: In Park, replace

```
So the mesh is rebuilt whenever they change, and the ghost, rebuilt with it, shows its candidate's walkways and starved marks.
```

with

```
So the mesh is rebuilt whenever they change.
```

Step 2: In Tools, replace

```
Whenever the tool's edit or highlight differs from the one last drawn, or the park mesh was rebuilt, the app builds the ghost mesh, buildGhostMesh's for the edit followed by appendEntity of the highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh.
```

with

```
Each frame, after the pointer moves, the app makes the frame's preview, previewEdit of the world and the tool's tentativeEdit (legible/SPEC.md, Previews). It builds the ghost mesh, buildGhostMesh of the world, the preview's edit, and the preview's candidate when there is an edit, and nothing otherwise, followed by appendEntity of the tool's highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh. It keeps no preview, candidate, or ghost between frames, so the ghost's walkways and starved marks, the food overlay, and the food tooltip all come from the one candidate of the frame, and follow every cycle and every change of the edit.
```

Step 3: In Tooling UI, replace

```
the app gives the renderer setOverlayMesh of buildFoodOverlay for the world, shaded by foodAvailability's Value at each place,
```

with

```
the app gives the renderer setOverlayMesh of buildFoodOverlay for previewedWorld of the world and the frame's preview, shaded by foodAvailability's Value at each place in it,
```

Step 4: In Tooling UI, replace

```
When it has one, and foodNear with the reach OVERLAY_BAND gives an availability for it,
```

with

```
When it has one, and foodNear on previewedWorld with the reach OVERLAY_BAND gives an availability for it,
```

Step 5: In Tooling UI, after the sentence `So the rows are exactly the terms the value sums.`, insert

```
While the frame's preview has a Shop, each frame, after the food tooltip, the app shows its context in the tooltip at the cursor, below the food tooltip's lines when both show: `Hungry footfall <f>`, f the context's Footfall to two decimals, or `No guest connector` when it has no Connection, and below it `Supply route <d> m from depot <k>`, d its Supply's Distance to one decimal and k its Depot in decimal, or `No supply route` when it has no Supply.
```

### Task 4: Create the preview interface

Files:
- Create: `src/legible/preview.h`
- Create: `src/legible/preview.cpp`
- Modify: `src/legible/CMakeLists.txt`

Step 1: Create `src/legible/preview.h`.

```cpp
#ifndef TPJ_LEGIBLE_PREVIEW_H
#define TPJ_LEGIBLE_PREVIEW_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// What a shop ghost would find: where its guest connector meets the guest path in the candidate,
// the committed world's hungry footfall there, and its nearest depot and supply route length in
// the candidate.
struct ShopContext {
  EntityKey Shop = NULL_KEY;
  std::optional<Place> Connection;
  double Footfall = 0.0;
  std::optional<DepotRoute> Supply;

  bool operator==(const ShopContext &) const = default;
};

// A tentative edit, the candidate world it gives when accepted, and a shop ghost's context.
struct Preview {
  std::optional<ParkEdit> Edit;
  std::optional<World> Candidate;
  std::optional<ShopContext> Shop;
};

// The preview of the edit on the world: the candidate, makeCandidate with the edit queued, when
// the edit is accepted, and its shop context when it adds or moves a shop. Changes nothing.
Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit);

// The preview's candidate when it holds one, and the world otherwise.
const World &previewedWorld(const World &world, const Preview &preview);

// A shop ghost's context in the candidate of the edit on the world, or none when the edit neither
// adds nor moves a shop. Changes nothing.
std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit);

} // namespace tpj

#endif
```

Step 2: Create `src/legible/preview.cpp` with stubs.

```cpp
#include "legible/preview.h"

namespace tpj {

Preview previewEdit(const World & /*world*/, const std::optional<ParkEdit> &edit) {
  return Preview{edit, std::nullopt, std::nullopt};
}

const World &previewedWorld(const World &world, const Preview & /*preview*/) { return world; }

std::optional<ShopContext> shopContext(const World & /*world*/, const World & /*candidate*/,
                                       const ParkEdit & /*edit*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 3: In `src/legible/CMakeLists.txt`, replace

```
    food.cpp
    path_place.cpp)
```

with

```
    food.cpp
    path_place.cpp
    preview.cpp)
```

Step 4: Build the library.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build succeeds with no warnings.

### Task 5: Declare the ghost overload

Files:
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`

Step 1: In `src/render/park_mesh.h`, replace

```cpp
// The ghost of an edit on a world: translucent in its kind's color when accepted, INVALID_TINT when
// not, and DELETE_TINT for a deletion, followed for an accepted edit by its candidate's walkways
// and starved marks.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit);
```

with

```cpp
// The ghost of an edit on a world: translucent in its kind's color when accepted, INVALID_TINT when
// not, and DELETE_TINT for a deletion, followed by the candidate's walkways and starved marks when
// a candidate is given.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit,
                        const std::optional<World> &candidate);
// The same ghost with the candidate makeCandidate gives for the edit when it is accepted, and none
// when it is not.
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit);
```

Step 2: In `src/render/park_mesh.cpp`, insert this stub directly above `ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit) {`.

```cpp
ParkMesh buildGhostMesh(const World & /*world*/, const ParkEdit & /*edit*/,
                        const std::optional<World> & /*candidate*/) {
  return {};
}

```

Step 3: Build the renderer.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build succeeds with no warnings.

### Task 6: Run the test pass

Dispatch the test-writer agent with only these paths: `plans/legible-simulation/explained-food/candidate-previews/FEATURE.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`, `src/sim/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/park/SPEC.md`, `src/sim/operations/SPEC.md`, `src/sim/guests/SPEC.md`, `src/legible/preview.h`, `src/legible/food.h`, `src/render/park_mesh.h`, `docs/principles.md`, `docs/conventions.md`.

Expected: the agent adds tests to tests/legible/ for criteria 1 and 3 to 6, to tests/render/ for criterion 2, and to tests/integration/ for criteria 7 and 8, and reports the files. They build against the stubs. Those needing a candidate, a context, or a ghost's walkways fail until Tasks 7 and 8. Those expecting none, and criterion 6's, may pass against the stubs. Criterion 9 is checked by hand in Task 11.

### Task 7: Implement the preview

Files:
- Modify: `src/legible/preview.cpp`

Step 1: Replace the file with this content.

```cpp
#include "legible/preview.h"

#include "sim/command_queue.h"
#include "sim/guests/footfall.h"
#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

#include <algorithm>
#include <variant>
#include <vector>

namespace tpj {
namespace {

bool holdsBox(const std::vector<ParkBox> &boxes, EntityKey key) {
  return std::ranges::any_of(boxes, [key](const ParkBox &box) { return box.Key == key; });
}

// The shop box the edit adds or moves: an AddBox's is the lowest key the candidate holds a box at
// and the world does not, and a MoveBox's is its Box when the world holds a shop there.
std::optional<EntityKey> ghostShop(const World &world, const World &candidate,
                                   const ParkEdit &edit) {
  const std::vector<ParkBox> boxes = parkBoxes(world);
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    if (add->Kind != BoxKind::Shop) {
      return std::nullopt;
    }
    // Boxes come in ascending key order, so the first new one has the lowest key.
    for (const ParkBox &box : parkBoxes(candidate)) {
      if (!holdsBox(boxes, box.Key)) {
        return box.Key;
      }
    }
    return std::nullopt;
  }
  if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : boxes) {
      if (box.Key == move->Box && box.Kind == BoxKind::Shop) {
        return box.Key;
      }
    }
  }
  return std::nullopt;
}

// Where the shop's guest connector meets a guest path in the candidate: the first place at its
// last stop's node on a guest path's carrier, or none.
std::optional<Place> connectionPlace(const World &candidate, EntityKey shop) {
  const Network &network = parkNetwork(candidate, PathKind::Guest);
  const std::vector<Carrier> &carriers = network.carriers();
  const auto connector =
      std::ranges::find(carriers, connectorKey(shop, Face::Front), &Carrier::Key);
  if (connector == carriers.end() || connector->Stops.empty()) {
    return std::nullopt;
  }
  const std::vector<ParkPath> paths = parkPaths(candidate);
  for (const Place &place : network.stopPlaces(connector->Stops.back().Node)) {
    const bool onGuestPath = std::ranges::any_of(paths, [&place](const ParkPath &path) {
      return path.Kind == PathKind::Guest && path.Key == place.Carrier;
    });
    if (onGuestPath) {
      return place;
    }
  }
  return std::nullopt;
}

} // namespace

Preview previewEdit(const World &world, const std::optional<ParkEdit> &edit) {
  Preview preview{edit, std::nullopt, std::nullopt};
  if (!edit || !isAccepted(world, *edit)) {
    return preview;
  }
  CommandQueue queue;
  queueEdit(queue, *edit);
  preview.Candidate = makeCandidate(world, queue);
  preview.Shop = shopContext(world, *preview.Candidate, *edit);
  return preview;
}

const World &previewedWorld(const World &world, const Preview &preview) {
  return preview.Candidate ? *preview.Candidate : world;
}

std::optional<ShopContext> shopContext(const World &world, const World &candidate,
                                       const ParkEdit &edit) {
  const std::optional<EntityKey> shop = ghostShop(world, candidate, edit);
  if (!shop) {
    return std::nullopt;
  }
  ShopContext context;
  context.Shop = *shop;
  context.Connection = connectionPlace(candidate, *shop);
  // Footfall is sampled in the committed world, since a candidate is never stepped.
  if (context.Connection) {
    context.Footfall =
        fieldValue<HungryFootfall>(world, parkNetwork(world, PathKind::Guest), *context.Connection);
  }
  context.Supply = nearestDepot(candidate, *shop);
  return context;
}

} // namespace tpj
```

Step 2: Build and run the legible tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 8: Implement the ghost overload

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Replace the stub from Task 5 and the two-argument buildGhostMesh after it with

```cpp
ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit,
                        const std::optional<World> &candidate) {
  ParkMesh mesh;
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(mesh, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(world, edit, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == move->Box) {
        appendBox(mesh, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(world, edit, boxColor(box.Kind)));
      }
    }
  } else if (const auto *path = std::get_if<AddPath>(&edit)) {
    appendPath(mesh, path->Kind, path->Points, ghostColor(world, edit, pathColor(path->Kind)));
  } else if (isAccepted(world, edit)) {
    const EntityKey key = std::holds_alternative<DeletePath>(edit) ? std::get<DeletePath>(edit).Path
                                                                   : std::get<DeleteBox>(edit).Box;
    appendEntity(mesh, world, key, DELETE_TINT);
  }
  if (candidate) {
    appendWalkways(mesh, *candidate, GHOST_ALPHA);
    appendStarvedMarks(mesh, *candidate, GHOST_ALPHA);
  }
  return mesh;
}

ParkMesh buildGhostMesh(const World &world, const ParkEdit &edit) {
  std::optional<World> candidate;
  if (isAccepted(world, edit)) {
    CommandQueue queue;
    queueEdit(queue, edit);
    candidate = makeCandidate(world, queue);
  }
  return buildGhostMesh(world, edit, candidate);
}
```

Step 2: Build and run the render tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 9: Create the shop context tooltip

Files:
- Create: `src/app/shop_context_tooltip.h`
- Create: `src/app/shop_context_tooltip.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Create `src/app/shop_context_tooltip.h`.

```cpp
#ifndef TPJ_APP_SHOP_CONTEXT_TOOLTIP_H
#define TPJ_APP_SHOP_CONTEXT_TOOLTIP_H

#include "legible/preview.h"

namespace tpj {

// Draws, in the tooltip at the cursor, a shop ghost's hungry footfall where it would meet the guest
// path and its supply route, or that it has none. Call between ImGui::NewFrame and ImGui::Render.
void drawShopContextTooltip(const ShopContext &context);

} // namespace tpj

#endif
```

Step 2: Create `src/app/shop_context_tooltip.cpp`.

```cpp
#include "app/shop_context_tooltip.h"

#include <imgui.h>

namespace tpj {

void drawShopContextTooltip(const ShopContext &context) {
  // A second tooltip in a frame joins the first, so this reads below the food tooltip.
  if (!ImGui::BeginTooltip()) {
    return;
  }
  if (context.Connection) {
    ImGui::Text("Hungry footfall %.2f", context.Footfall);
  } else {
    ImGui::TextUnformatted("No guest connector");
  }
  if (context.Supply) {
    ImGui::Text("Supply route %.1f m from depot %llu", context.Supply->Distance,
                static_cast<unsigned long long>(context.Supply->Depot));
  } else {
    ImGui::TextUnformatted("No supply route");
  }
  ImGui::EndTooltip();
}

} // namespace tpj
```

Step 3: In `src/app/CMakeLists.txt`, replace

```
    orbit_camera.cpp
    tool_panel.cpp)
```

with

```
    orbit_camera.cpp
    shop_context_tooltip.cpp
    tool_panel.cpp)
```

### Task 10: Compose the preview in the main loop

Files:
- Modify: `src/app/main.cpp`

Step 1: Add the includes `#include "app/shop_context_tooltip.h"` after `#include "app/park_file.h"`, and `#include "legible/preview.h"` after `#include "legible/food.h"`.

Step 2: Delete the struct DrawnGhost with its comment:

```cpp
// The edit and highlight a ghost mesh was built from.
struct DrawnGhost {
  std::optional<tpj::ParkEdit> Edit;
  std::optional<tpj::EntityKey> Highlight;

  bool operator==(const DrawnGhost &) const = default;
};

```

Step 3: Replace updateParkMesh's comment, signature, and body with

```cpp
// Rebuilds the park mesh when the world's intent differs from what was last drawn, framing the
// camera on the first one. Returns false if the upload failed.
bool updateParkMesh(tpj::Renderer &renderer, const tpj::World &world,
                    std::optional<DrawnIntent> &drawn, tpj::OrbitCamera &camera) {
  DrawnIntent current{tpj::parkEntrances(world), tpj::parkPaths(world), tpj::parkBoxes(world)};
  if (drawn && *drawn == current) {
    return true;
  }
  const tpj::ParkMesh mesh = tpj::buildParkMesh(world);
  if (!drawn) {
    if (const std::optional<tpj::GroundBounds> bounds = tpj::meshBounds(mesh)) {
      tpj::frameOrbitCamera(camera, *bounds, tpj::CameraView{}.FovY);
    }
  }
  drawn = std::move(current);
  return tpj::setParkMesh(renderer, mesh);
}
```

Step 4: Replace updateGhostMesh, with its comment, by

```cpp
// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, then the
// tool's highlight.
tpj::ParkMesh ghostMesh(const tpj::World &world, const tpj::ToolState &tool,
                        const tpj::Preview &preview) {
  tpj::ParkMesh mesh;
  if (preview.Edit) {
    mesh = tpj::buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  if (const std::optional<tpj::EntityKey> highlight = tpj::highlightedEntity(tool, world)) {
    tpj::appendEntity(mesh, world, *highlight, tpj::HIGHLIGHT_TINT);
  }
  return mesh;
}
```

Step 5: Replace buildUi, with its comment, by

```cpp
// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, the graph over the scene while it is shown, the food tooltip at the cursor while the
// overlay is, on the preview's candidate when it has one, and a shop ghost's context.
void buildUi(SDL_Window *window, const tpj::World &world, const tpj::OrbitCamera &camera,
             const tpj::CameraView &view, ShownViews &shown, tpj::ToolState &tool,
             const tpj::Preview &preview) {
  tpj::beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(window, world, camera, shown, tool);
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

Step 6: In runLoop, delete the line `  std::optional<DrawnGhost> drawnGhost;`.

Step 7: In runLoop, replace

```cpp
    bool parkRebuilt = false;
    if (!updateParkMesh(renderer, world, drawn, camera, parkRebuilt)) {
```

with

```cpp
    if (!updateParkMesh(renderer, world, drawn, camera)) {
```

Step 8: In runLoop, replace

```cpp
    tpj::movePointer(tool, groundUnderCursor(renderer.Window, view));
    if (!updateGhostMesh(renderer, world, tool, drawnGhost, parkRebuilt)) {
      return false;
    }

    buildUi(renderer.Window, world, camera, view, shown, tool);

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    // The overlay is built after the panels, so a change of its checkbox shows in this frame.
    if (!tpj::setOverlayMesh(renderer, foodOverlayMesh(world, shown.FoodOverlay)) ||
```

with

```cpp
    tpj::movePointer(tool, groundUnderCursor(renderer.Window, view));
    // Made afresh each frame, so the ghost, the overlay, and the tooltips show one candidate of
    // this frame's world and edit.
    const tpj::Preview preview = tpj::previewEdit(world, tpj::tentativeEdit(tool, world));
    if (!tpj::setGhostMesh(renderer, ghostMesh(world, tool, preview))) {
      return false;
    }

    buildUi(renderer.Window, world, camera, view, shown, tool, preview);

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    // The overlay is built after the panels, so a change of its checkbox shows in this frame.
    if (!tpj::setOverlayMesh(renderer, foodOverlayMesh(tpj::previewedWorld(world, preview),
                                                       shown.FoodOverlay)) ||
```

Step 9: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

Step 10: Capture warm.park with the overlay, from `build/windows-debug`, and compare it with food-overlay's capture.

Run: `cd build/windows-debug && timeout 120 ./ThemeParkJones.exe --park ../../tests/parks/warm.park --ticks 3000 --overlay food --capture preview.bmp; echo $?`
Expected: `0`. With no pointer there is no ghost, so the capture shows the committed world's band as before. Convert it to PNG in the scratchpad, view it, and delete the BMP.

### Task 11: Check the preview by hand

Step 1: Ask Evan to run, from `build/windows-debug`, `./ThemeParkJones.exe --park ../../tests/parks/warm.park --overlay food`, and to:

- choose Place shop and hover beside the guest path south of the shop, near (6.5, 106.8), between path 5 and the shop, and then over a spot where it is refused;
- choose Delete and hover the backstage path east of the shop;
- choose Move box and drag the shop.

Expected (criterion 9): an accepted shop ghost brightens the band near it and adds its shop's row to the food tooltip, with `Hungry footfall` and `Supply route` lines below; a refused one leaves the band as committed; hovering the backstage path turns the band the zero color and marks the shop starved; a moved shop's tooltip shows its context. Anything else is a deviation to report.

### Task 12: Verify the feature

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

### Task 13: Commit

Step 1: Stage exactly the feature's paths: `git add plans/legible-simulation/explained-food/candidate-previews src/legible src/render src/app tests/legible tests/render tests/integration`, and check `git status --short` shows nothing else staged.

Step 2: Dispatch the reviewer on the staged diff, `git diff --cached`, with FEATURE.md, src/legible/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, docs/principles.md, and docs/decisions/0027-code-architecture.md as context, and give its findings to Evan verbatim.

Step 3: Commit through commit-hygiene with the subject `Legible: Preview edits on a candidate world` and a body of at most 72-character lines saying that previewEdit makes the candidate of an accepted edit and a shop ghost's context, that the app makes it each frame so the ghost, overlay, and tooltip share one candidate, replacing the ghost cache, and that the tooltip gives a shop ghost's footfall and supply route, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
