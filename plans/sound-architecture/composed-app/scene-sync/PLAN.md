# Implementation Plan: Scene Sync

## Goal

Move the scene's caches into SceneSync and the tool and the Inspector's subject into Interaction, each following the park session's generation, so main.cpp holds no reset on replacement.

## Approach

SceneSync and Interaction join tpj_app_core, which gains tpj_legible and tpj_tools as dependencies. SceneSync holds what each mesh was last built from and the kept preview, and its three calls answer which meshes a frame rebuilds. scene_uploads.cpp, in the executable, takes those answers and builds and uploads the meshes with main.cpp's ghostMesh and foodOverlayMesh. Interaction holds the ToolState and the subject with one member per writer. main.cpp's runLoop then calls follow and the three sync calls at the points of the frame where its locals were reset and compared, and forgetOldWorld, DrawnIntent, DrawnPreview, and the update functions go.

## Placement

Decision 0027 places each behavior this feature adds:

- Deciding which meshes a frame rebuilds, remembering what each was built from, and keeping the preview: app, new component app/scene_sync.h, SceneSync, in tpj_app_core. It owns every cache derived from the world, so it alone compares the generation for them, and it needs no window, so tests check it.
- Building the park, guest, ghost, and overlay meshes, uploading them, and framing the camera on a new park's mesh: app, new component app/scene_uploads.h, in tpj_app. It is the scene sync's platform edge, the only part that calls the renderer, and it holds no state.
- Owning the tool and the Inspector's subject, giving the tool the buttons, picking on a Look press, and dropping both on a new generation: app, new component app/interaction.h, Interaction, in tpj_app_core. It is the player's hold on the park, not derived from the world, so it is not the scene sync's, and it needs no window.
- PointerButtons moves from main.cpp to app/interaction.h, since Interaction consumes it.
- main.cpp: composition only. runLoop holds a SceneSync and an Interaction and calls them in the frame's existing order. It gains no concern and loses the mesh caches, the update functions, the button and pick handling, and forgetOldWorld.

## Tasks

### Task 1: Describe the scene sync and the interaction in the app spec

Files:
- Modify: `src/app/SPEC.md:11,15,23`

Step 1: Add FEATURE.md's new paragraph after the paragraph under "## Park", and FEATURE.md's new paragraph after the paragraph under "## Tools", each with its exact text.

Step 2: In "## Park files", add FEATURE.md's sentence directly after the sentence ending "and frame the camera on the new park's mesh as at start."

### Task 2: Declare the scene sync

Files:
- Create: `src/app/scene_sync.h`
- Create: `src/app/scene_sync.cpp`

Step 1: Create src/app/scene_sync.h:

```cpp
#ifndef TPJ_APP_SCENE_SYNC_H
#define TPJ_APP_SCENE_SYNC_H

#include "legible/inspect.h"
#include "legible/preview.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>
#include <stdint.h>
#include <vector>

namespace tpj {

// The park and guest meshes a frame rebuilds, and whether to frame the camera on the park mesh.
struct WorldMeshes {
  bool Park = false;
  bool Guests = false;
  bool FrameCamera = false;

  bool operator==(const WorldMeshes &) const = default;
};

// What the ghost and overlay meshes are built from besides the preview: the tool's highlight, the
// Inspector's subject, and whether the food overlay shows.
struct PreviewLook {
  std::optional<EntityKey> Highlight;
  std::optional<InspectorSubject> Inspected;
  bool FoodOverlay = false;

  bool operator==(const PreviewLook &) const = default;
};

// What the scene's meshes were last built from, and the kept preview, so that each frame rebuilds
// exactly the meshes whose sources changed. A world of a generation it has not seen rebuilds every
// mesh.
class SceneSync {
public:
  // After the frame's ticks: which of the park and guest meshes the world needs, remembering it as
  // drawn. For a generation other than the last one it saw, as at the first call, both, with the
  // camera framed, and the kept preview is emptied. Otherwise the park mesh when the world's intent
  // differs from the last call's, and the guest mesh when its tick does.
  WorldMeshes syncWorld(const World &world, uint64_t generation);
  // After the pointer moves: keeps the preview of the edit as keepPreview does, returning whether
  // it was made again.
  bool syncPreview(const World &world, const std::optional<ParkEdit> &edit);
  [[nodiscard]] const Preview &preview() const { return Kept.Made; }
  // After the panels: whether the ghost and overlay meshes need building, which they do at the
  // first call, when the preview was just made again, or when the look differs from the last
  // call's. Remembers the look.
  bool syncLook(bool remade, const PreviewLook &look);

private:
  // The intent a park mesh was built from.
  struct DrawnIntent {
    std::vector<ParkEntrance> Entrances;
    std::vector<ParkPath> Paths;
    std::vector<ParkBox> Boxes;

    bool operator==(const DrawnIntent &) const = default;
  };

  std::optional<uint64_t> Generation;
  std::optional<DrawnIntent> Intent;
  std::optional<uint64_t> GuestTick;
  KeptPreview Kept;
  std::optional<PreviewLook> Look;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/scene_sync.cpp, including "app/scene_sync.h", with stubs inside namespace tpj: syncWorld returns `{}`, and syncPreview and syncLook return false. Mark unused parameters with `/*name*/` comments.

### Task 3: Declare the interaction

Files:
- Create: `src/app/interaction.h`
- Create: `src/app/interaction.cpp`

Step 1: Create src/app/interaction.h:

```cpp
#ifndef TPJ_APP_INTERACTION_H
#define TPJ_APP_INTERACTION_H

#include "legible/inspect.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <optional>
#include <stdint.h>

namespace tpj {

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// The player's hold on the park: the tool, and the guest or shop the Inspector shows. It follows
// the park session's generation, so a replaced world drops the tool's hold and the subject.
class Interaction {
public:
  // Starts with the tool at start and no subject, having seen the generation.
  explicit Interaction(uint64_t generation) : Generation(generation) {}

  [[nodiscard]] const ToolState &tool() const { return Tool; }
  [[nodiscard]] const std::optional<InspectorSubject> &subject() const { return Subject; }

  // For a generation other than the last one it saw, selects the current tool again, so it drops
  // any hold and drawn points, and forgets the subject.
  void follow(uint64_t generation);
  // Gives the tool the frame's press, then its release, queueing the edit a release commits.
  void useButtons(const World &world, CommandQueue &commands, const PointerButtons &buttons);
  // True when the frame's press picks a subject: a press with the Look tool.
  [[nodiscard]] bool picks(const PointerButtons &buttons) const;
  // Sets the subject from the entity under the cursor, as pickSubject does.
  void pick(const World &world, std::optional<EntityKey> entity);
  void forgetSubject() { Subject.reset(); }
  void selectTool(ToolKind kind);
  void movePointer(std::optional<ParkPoint> ground);
  [[nodiscard]] std::optional<ParkEdit> tentativeEdit(const World &world) const;
  [[nodiscard]] std::optional<EntityKey> highlighted(const World &world) const;

private:
  uint64_t Generation;
  ToolState Tool;
  std::optional<InspectorSubject> Subject;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/interaction.cpp, including "app/interaction.h", with stubs inside namespace tpj: follow, useButtons, pick, selectTool, and movePointer do nothing; picks returns false; tentativeEdit and highlighted return `std::nullopt`. Mark unused parameters with `/*name*/` comments.

### Task 4: Add them to tpj_app_core

Files:
- Modify: `src/app/CMakeLists.txt:2-7`

Step 1: Add interaction.cpp and scene_sync.cpp to tpj_app_core's sources, keeping them sorted, and change its link line to `target_link_libraries(tpj_app_core PUBLIC tpj_legible tpj_sim tpj_tools PRIVATE SDL3::SDL3)`.

Step 2: Configure and build.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests`
Expected: the build succeeds with no warnings.

### Task 5: Test pass

Step 1: Dispatch the test-writer agent with FEATURE.md, src/app/SPEC.md, src/legible/SPEC.md, src/tools/SPEC.md, and the public headers src/app/scene_sync.h, src/app/interaction.h, and src/app/park_session.h. It creates tests/app/scene_sync_test.cpp and tests/app/interaction_test.cpp and adds them to tpj_app_tests in tests/app/CMakeLists.txt.

Step 2: Build and run them.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#scene_sync_test],[#interaction_test]"`
Expected: the build succeeds, and the tests that need the stubs' behavior, including those that set up through a stubbed member, fail.

### Task 6: Implement the scene sync

Files:
- Modify: `src/app/scene_sync.cpp`

Step 1: Replace the stubs with:

```cpp
WorldMeshes SceneSync::syncWorld(const World &world, uint64_t generation) {
  WorldMeshes meshes;
  if (Generation != generation) {
    Generation = generation;
    Intent.reset();
    GuestTick.reset();
    Kept = {};
    meshes.FrameCamera = true;
  }
  DrawnIntent current{parkEntrances(world), parkPaths(world), parkBoxes(world)};
  if (Intent != current) {
    Intent = std::move(current);
    meshes.Park = true;
  }
  if (GuestTick != world.Tick) {
    GuestTick = world.Tick;
    meshes.Guests = true;
  }
  return meshes;
}

bool SceneSync::syncPreview(const World &world, const std::optional<ParkEdit> &edit) {
  return keepPreview(Kept, world, edit);
}

bool SceneSync::syncLook(bool remade, const PreviewLook &look) {
  if (!remade && Look == look) {
    return false;
  }
  Look = look;
  return true;
}
```

Add `#include <utility>` to its includes.

Step 2: Run the scene sync's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#scene_sync_test]"`
Expected: every test passes.

### Task 7: Implement the interaction

Files:
- Modify: `src/app/interaction.cpp`

Step 1: Replace the stubs with:

```cpp
void Interaction::follow(uint64_t generation) {
  if (generation == Generation) {
    return;
  }
  Generation = generation;
  tpj::selectTool(Tool, Tool.Kind);
  Subject.reset();
}

void Interaction::useButtons(const World &world, CommandQueue &commands,
                             const PointerButtons &buttons) {
  if (buttons.Pressed) {
    pressPointer(Tool, world);
  }
  if (buttons.Released) {
    if (const std::optional<ParkEdit> edit = releasePointer(Tool, world)) {
      queueEdit(commands, *edit);
    }
  }
}

bool Interaction::picks(const PointerButtons &buttons) const {
  return buttons.Pressed && Tool.Kind == ToolKind::None;
}

void Interaction::pick(const World &world, std::optional<EntityKey> entity) {
  pickSubject(Subject, world, entity);
}

void Interaction::selectTool(ToolKind kind) { tpj::selectTool(Tool, kind); }

void Interaction::movePointer(std::optional<ParkPoint> ground) { tpj::movePointer(Tool, ground); }

std::optional<ParkEdit> Interaction::tentativeEdit(const World &world) const {
  return tpj::tentativeEdit(Tool, world);
}

std::optional<EntityKey> Interaction::highlighted(const World &world) const {
  return highlightedEntity(Tool, world);
}
```

Step 2: Run the interaction's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#interaction_test]"`
Expected: every test passes.

### Task 8: Create the scene uploads

Files:
- Create: `src/app/scene_uploads.h`
- Create: `src/app/scene_uploads.cpp`
- Modify: `src/app/CMakeLists.txt:10-18`

Step 1: Create src/app/scene_uploads.h:

```cpp
#ifndef TPJ_APP_SCENE_UPLOADS_H
#define TPJ_APP_SCENE_UPLOADS_H

#include "app/orbit_camera.h"
#include "app/scene_sync.h"
#include "legible/preview.h"
#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Builds the park and guest meshes the frame needs and gives them to the renderer, framing the
// camera on the park mesh's bounds when asked and the mesh has vertices. False if an upload failed.
bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       OrbitCamera &camera);
// Builds the ghost, meaning the preview's edit with its candidate's walkways and starved marks, then
// the look's highlight, then its inspected guest or shop's mark, and the food overlay of the
// previewed world while the look shows it and an empty mesh otherwise, and gives both to the
// renderer. False if an upload failed.
bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look);

} // namespace tpj

#endif
```

Step 2: Create src/app/scene_uploads.cpp, including "app/scene_uploads.h", "legible/food.h", "render/food_overlay.h", "render/guest_mesh.h", "render/park_mesh.h", and `<optional>`. Inside namespace tpj, an anonymous namespace holds:

```cpp
// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, then the
// highlight, then the inspected guest or shop's mark.
ParkMesh ghostMesh(const World &world, const Preview &preview, const PreviewLook &look) {
  ParkMesh mesh;
  if (preview.Edit) {
    mesh = buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  if (look.Highlight) {
    appendEntity(mesh, world, *look.Highlight, HIGHLIGHT_TINT);
  }
  // Each adds nothing for a key of the other's kind.
  if (look.Inspected) {
    appendEntity(mesh, world, look.Inspected->Key, HIGHLIGHT_TINT);
    appendGuestEntity(mesh, world, look.Inspected->Key, HIGHLIGHT_TINT);
  }
  return mesh;
}
```

and main.cpp's foodOverlayMesh with its comment, without the `tpj::` qualifiers. After the anonymous namespace:

```cpp
bool uploadWorldMeshes(Renderer &renderer, const World &world, const WorldMeshes &meshes,
                       OrbitCamera &camera) {
  if (meshes.Park) {
    const ParkMesh mesh = buildParkMesh(world);
    if (meshes.FrameCamera) {
      if (const std::optional<GroundBounds> bounds = meshBounds(mesh)) {
        frameOrbitCamera(camera, *bounds, CameraView{}.FovY);
      }
    }
    if (!setParkMesh(renderer, mesh)) {
      return false;
    }
  }
  return !meshes.Guests || setGuestMesh(renderer, buildGuestMesh(world));
}

bool uploadPreviewMeshes(Renderer &renderer, const World &world, const Preview &preview,
                         const PreviewLook &look) {
  return setGhostMesh(renderer, ghostMesh(world, preview, look)) &&
         setOverlayMesh(renderer,
                        foodOverlayMesh(previewedWorld(world, preview), look.FoodOverlay));
}
```

Step 3: In src/app/CMakeLists.txt, add scene_uploads.cpp to tpj_app's sources, in sorted order after park_dialogs.cpp.

### Task 9: Compose them in main.cpp

Files:
- Modify: `src/app/main.cpp`

Step 1: Delete from main.cpp, with their comments: DrawnIntent, DrawnPreview, PointerButtons, updateParkMesh, ghostMesh, updateGuestMesh, foodOverlayMesh, updatePreviewMeshes, useButtons, pickOnLookPress, and forgetOldWorld.

Step 2: Add `#include "app/interaction.h"`, `#include "app/scene_sync.h"`, and `#include "app/scene_uploads.h"` in sorted order, and remove `#include "legible/food.h"` and `#include "render/food_overlay.h"`. In gatherInput's signature and comment, PointerButtons becomes tpj::PointerButtons.

Step 3: Change drawPanels to take `tpj::Interaction &interaction` in place of `tpj::ToolState &tool`. In it, call `tpj::drawToolPanel(interaction.tool().Kind, !interaction.tool().Drawn.empty(), dialogs.dialogShowing())`, and replace `tpj::selectTool(tool, *choice.Tool);` with `interaction.selectTool(*choice.Tool);`.

Step 4: Change buildUi's signature to

```cpp
void buildUi(SDL_Window *window, tpj::ParkDialogs &dialogs, const tpj::World &world,
             const tpj::OrbitCamera &camera, ShownViews &shown, tpj::Interaction &interaction,
             const tpj::Preview &preview)
```

pass `interaction` to drawPanels, and replace the Inspector's block with:

```cpp
  if (const std::optional<tpj::InspectorSubject> &subject = interaction.subject();
      subject && !tpj::drawInspector(tpj::inspectSubject(world, *subject))) {
    interaction.forgetSubject();
  }
```

Step 5: In runLoop, replace the locals drawn, guestTick, kept, drawnPreview, inspected, and tool, and the local generation, with `tpj::SceneSync scene;` and `tpj::Interaction interaction(session.generation());`, declared after camera. The loop's buttons local becomes `tpj::PointerButtons buttons;`. Then:

- replace the generation block after `session.useFileRequest(dialogs.take(), dialogs);` with `interaction.follow(session.generation());`;
- replace the useButtons and pickOnLookPress calls with:

```cpp
    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    interaction.useButtons(session.world(), session.commands(), buttons);
    if (interaction.picks(buttons)) {
      interaction.pick(session.world(),
                       entityUnderCursor(renderer.Window, session.world(), cameraView(camera)));
    }
```

- replace the updateParkMesh and updateGuestMesh blocks with:

```cpp
    if (!tpj::uploadWorldMeshes(renderer, session.world(),
                                scene.syncWorld(session.world(), session.generation()), camera)) {
      return false;
    }
```

- replace `tpj::movePointer(tool, ...)` with `interaction.movePointer(...)`, and the keepPreview call with `scene.syncPreview(session.world(), interaction.tentativeEdit(session.world()))`, keeping its comment;
- call buildUi as `buildUi(renderer.Window, dialogs, session.world(), camera, shown, interaction, scene.preview());`;
- replace the updatePreviewMeshes condition, keeping its comment above it, with:

```cpp
    const tpj::PreviewLook look{interaction.highlighted(session.world()), interaction.subject(),
                                shown.FoodOverlay};
    if ((scene.syncLook(remade, look) &&
         !tpj::uploadPreviewMeshes(renderer, session.world(), scene.preview(), look)) ||
        !tpj::drawFrame(renderer, view, ImGui::GetDrawData(),
                        lastFrame ? options.CapturePath : nullptr)) {
      return false;
    }
```

Step 6: Build the app and run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests && build/windows-debug/tpj_app_tests.exe`
Expected: the build succeeds with no warnings, and every test passes.

Run: `grep -n "reset()\|kept = \|forgetOldWorld" src/app/main.cpp`
Expected: no output.

### Task 10: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- 'src/app/*.h' 'src/app/*.cpp' 'tests/app/*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Step 3: Check the scene.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/scene-sync.bmp`
Expected: it exits with status 0, and build/scene-sync.bmp shows the park with its walkways, guests, the graph, and the food band, as before the change.

Step 4: Check the interaction by hand. Run `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park`, then:
- with Look, click a guest, then a shop;
- choose Guest path, click two points, and click the last again to finish;
- choose Guest path, click one point, and press New park;
- press Open park and open tests/parks/warm.park.

Expected: the Inspector shows the guest, then the shop, with its mark in the scene; the path is added; New park drops the drawn point, closes the Inspector, and frames the camera on the new park; Open shows warm.park framed again with no Inspector.

### Task 11: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Give the scene caches and the tool one owner each`.
