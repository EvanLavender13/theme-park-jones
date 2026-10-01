# Implementation Plan: Composed Entry

## Goal

Move the command line into an options component in tpj_app_core, give each platform resource an owner, and move the frame loop into an Application that holds the owners and the components, leaving main.cpp only composing.

## Approach

parseOptions moves from main.cpp to app/options.h unchanged in its rules, taking argc and argv and giving an optional Options. Four owner classes in app/platform.h replace main's manual setup and teardown, each taking the owner before it and acquiring only when that one did. The Application in app/application.h holds them as its first members, then the session and the components, and its run is runLoopLogged and runLoop moved with their order unchanged. ToolingUi::build gives the frame's draw data, so the Application calls no ImGui function, and main.cpp shrinks to parsing, the hash, and building and running the Application.

## Placement

Decision 0027 places each behavior this feature adds:

- Reading the command line into Options, refusing what the spec refuses, and logging the usage: app, new component app/options.h, in tpj_app_core. It is a function of the arguments alone, so it needs no window and tests call it. parseCount and PARK_SIZE_METERS leave main.cpp; FramesGiven becomes a local of the parse.
- Acquiring and releasing SDL's video, the window, the ImGui context with its SDL3 backend, and the renderer: app, new component app/platform.h, in tpj_app. They are the platform's resources, and each owner is the one place its resource is released. They call SDL and ImGui, so they live in the executable.
- The order a frame runs in, the catch around the loop, and handing the park's size to the renderer and the camera: app, new component app/application.h, in tpj_app. 0027 asks for the frame order in one place, and the Application owns the components that order calls. It is in the executable because it holds the platform owners and calls SDL's performance counter.
- Ending the ImGui frame and giving its draw data: app, app/tooling_ui.h. The tooling UI already begins and ends the ImGui frame it builds, so it gives the result rather than the Application asking ImGui for it.
- main.cpp: composition only. It loses Options, parseCount, parseOptions, runLoop, runLoopLogged, PARK_SIZE_METERS, and the platform setup and teardown, and gains nothing but the calls to parseOptions and the Application.

## Tasks

### Task 1: Describe the composed entry in the app spec

Files:
- Modify: `src/app/SPEC.md:7,47,51-61`

Step 1: In "## Main loop", replace the first sentence, "The app owns the Dear ImGui context and its SDL3 platform backend, created before the renderer and destroyed after it.", with FEATURE.md's replacement paragraph's exact text, keeping the rest of the paragraph after it.

Step 2: In "## Tooling UI", add FEATURE.md's sentence after the sentence ending "so the scene sync reads the Food overlay checkbox from it.", with its exact text.

Step 3: At the end of "## Command line", add FEATURE.md's new paragraph, with its exact text.

### Task 2: Declare the options

Files:
- Create: `src/app/options.h`
- Create: `src/app/options.cpp`
- Modify: `src/app/CMakeLists.txt:2-11`

Step 1: Create src/app/options.h:

```cpp
#ifndef TPJ_APP_OPTIONS_H
#define TPJ_APP_OPTIONS_H

#include <optional>
#include <stdint.h>

namespace tpj {

// What the command line asks of the app. The paths point into the command line's arguments.
struct Options {
  // The frames to run before exiting, or 0 to run until the player quits.
  int FrameLimit = 0;
  // Where to write the last frame as a BMP, or null.
  const char *CapturePath = nullptr;
  // The park file to start from, or null for a new park.
  const char *ParkPath = nullptr;
  // The cycles to step before the first frame.
  uint64_t Ticks = 0;
  // Write the hash after the ticks and exit without a window.
  bool PrintHash = false;
  // Check the Debug panel's Graph checkbox at start.
  bool ShowGraph = false;
  // Check the Debug panel's Food overlay checkbox at start.
  bool ShowFoodOverlay = false;
};

// Reads the command line, argv[0] being the program's name. --park PATH starts from a park file,
// --ticks N steps it N ticks before the first frame, and --hash prints the state hash after them
// and exits. --frames N exits after N frames. --graph starts with the graph view on, and --overlay
// food with the food overlay on. --capture PATH writes the last frame to PATH as a BMP and implies
// a limit of 3 frames when --frames gives none that is positive. None, after logging the usage,
// for an unknown option, an option missing its value, a --ticks value that is not a decimal count,
// an --overlay value other than food, or --hash with --frames, --capture, --graph, or --overlay.
std::optional<Options> parseOptions(int argc, const char *const *argv);

} // namespace tpj

#endif
```

Step 2: Create src/app/options.cpp with a stub:

```cpp
#include "app/options.h"

namespace tpj {

std::optional<Options> parseOptions(int, const char *const *) { return std::nullopt; }

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, add `options.cpp` to tpj_app_core's sources, after `interaction.cpp`.

Step 4: Build the library.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build succeeds with no warnings.

### Task 3: Test pass

Dispatch the test-writer as implementing-features describes, with:
- Feature: plans/sound-architecture/composed-app/composed-entry/FEATURE.md
- Specs: src/app/SPEC.md
- Public headers: src/app/options.h

Expected: tests/app/options_test.cpp, added to tpj_app_tests in tests/app/CMakeLists.txt. The tests of an accepted command line fail against the stub, and the refusal tests pass.

### Task 4: Implement the options

Files:
- Modify: `src/app/options.cpp`

Step 1: Replace src/app/options.cpp with main.cpp's parseCount and parseOptions, moved, with FramesGiven a local and the result an optional:

```cpp
#include "app/options.h"

#include <SDL3/SDL_log.h>

#include <charconv>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <system_error>

namespace tpj {
namespace {

bool parseCount(const char *text, uint64_t &count) {
  const std::string_view value(text);
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, count);
  return !value.empty() && result.ec == std::errc() && result.ptr == end;
}

} // namespace

std::optional<Options> parseOptions(int argc, const char *const *argv) {
  Options options;
  bool framesGiven = false;
  bool valid = true;
  for (int i = 1; i < argc && valid; ++i) {
    const bool hasValue = i + 1 < argc;
    if (strcmp(argv[i], "--frames") == 0 && hasValue) {
      options.FrameLimit = static_cast<int>(strtol(argv[++i], nullptr, 10));
      framesGiven = true;
    } else if (strcmp(argv[i], "--capture") == 0 && hasValue) {
      options.CapturePath = argv[++i];
    } else if (strcmp(argv[i], "--park") == 0 && hasValue) {
      options.ParkPath = argv[++i];
    } else if (strcmp(argv[i], "--ticks") == 0 && hasValue) {
      valid = parseCount(argv[++i], options.Ticks);
    } else if (strcmp(argv[i], "--hash") == 0) {
      options.PrintHash = true;
    } else if (strcmp(argv[i], "--graph") == 0) {
      options.ShowGraph = true;
    } else if (strcmp(argv[i], "--overlay") == 0 && hasValue) {
      // food is the only overlay.
      valid = strcmp(argv[++i], "food") == 0;
      options.ShowFoodOverlay = valid;
    } else {
      valid = false;
    }
  }
  if (options.PrintHash && (framesGiven || options.CapturePath != nullptr || options.ShowGraph ||
                            options.ShowFoodOverlay)) {
    valid = false;
  }
  if (!valid) {
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH] [--graph] "
            "[--overlay food]",
            argv[0]);
    return std::nullopt;
  }
  if (options.CapturePath != nullptr && options.FrameLimit <= 0) {
    options.FrameLimit = 3;
  }
  return options;
}

} // namespace tpj
```

Step 2: Build and run the options' tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#options_test]"`
Expected: the build succeeds with no warnings, and every test in options_test.cpp passes.

### Task 5: Create the platform owners

Files:
- Create: `src/app/platform.h`
- Create: `src/app/platform.cpp`
- Modify: `src/app/CMakeLists.txt:16-27`

Step 1: Create src/app/platform.h:

```cpp
#ifndef TPJ_APP_PLATFORM_H
#define TPJ_APP_PLATFORM_H

#include "render/renderer.h"

struct SDL_Window;

namespace tpj {

// SDL's video subsystem, initialized for the owner's lifetime.
class SdlVideo {
public:
  // Initializes it, logging SDL's error when it cannot.
  SdlVideo();
  SdlVideo(const SdlVideo &) = delete;
  SdlVideo &operator=(const SdlVideo &) = delete;
  SdlVideo(SdlVideo &&) = delete;
  SdlVideo &operator=(SdlVideo &&) = delete;
  ~SdlVideo();

  [[nodiscard]] bool ready() const { return Ready; }

private:
  bool Ready = false;
};

// The app's window, open for the owner's lifetime.
class MainWindow {
public:
  // Opens the resizable, high pixel density window titled Theme Park Jones, 1600 by 900, when
  // SDL's video is ready, logging SDL's error when it cannot.
  explicit MainWindow(const SdlVideo &video);
  MainWindow(const MainWindow &) = delete;
  MainWindow &operator=(const MainWindow &) = delete;
  MainWindow(MainWindow &&) = delete;
  MainWindow &operator=(MainWindow &&) = delete;
  ~MainWindow();

  // The window, or null when it is not open.
  [[nodiscard]] SDL_Window *get() const { return Window; }

private:
  SDL_Window *Window = nullptr;
};

// The Dear ImGui context, with docking enabled and the dark style, and its SDL3 platform backend
// for the window, for the owner's lifetime.
class UiContext {
public:
  // Creates them when the window is open.
  explicit UiContext(const MainWindow &window);
  UiContext(const UiContext &) = delete;
  UiContext &operator=(const UiContext &) = delete;
  UiContext(UiContext &&) = delete;
  UiContext &operator=(UiContext &&) = delete;
  ~UiContext();

  [[nodiscard]] bool ready() const { return Ready; }

private:
  bool Ready = false;
};

// The renderer for the window, with a square terrain terrainSize meters across, for the owner's
// lifetime.
class RendererOwner {
public:
  // Creates it when the ImGui context its GPU backend needs is ready, logging through SDL when it
  // cannot.
  RendererOwner(const MainWindow &window, const UiContext &ui, float terrainSize);
  RendererOwner(const RendererOwner &) = delete;
  RendererOwner &operator=(const RendererOwner &) = delete;
  RendererOwner(RendererOwner &&) = delete;
  RendererOwner &operator=(RendererOwner &&) = delete;
  ~RendererOwner();

  [[nodiscard]] bool ready() const { return Ready; }
  [[nodiscard]] Renderer &renderer() { return Gpu; }

private:
  Renderer Gpu;
  bool Ready = false;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/platform.cpp, with main's setup and teardown calls moved into the owners:

```cpp
#include "app/platform.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

namespace tpj {

SdlVideo::SdlVideo() : Ready(SDL_Init(SDL_INIT_VIDEO)) {
  if (!Ready) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
  }
}

SdlVideo::~SdlVideo() {
  if (Ready) {
    SDL_Quit();
  }
}

MainWindow::MainWindow(const SdlVideo &video) {
  if (!video.ready()) {
    return;
  }
  Window = SDL_CreateWindow("Theme Park Jones", 1600, 900,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (Window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
  }
}

MainWindow::~MainWindow() {
  if (Window != nullptr) {
    SDL_DestroyWindow(Window);
  }
}

UiContext::UiContext(const MainWindow &window) {
  if (window.get() == nullptr) {
    return;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(window.get());
  Ready = true;
}

UiContext::~UiContext() {
  if (Ready) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
}

// destroyRenderer releases only what createRenderer made, so it runs even when creation failed.
RendererOwner::RendererOwner(const MainWindow &window, const UiContext &ui, float terrainSize)
    : Ready(ui.ready() && createRenderer(Gpu, window.get(), terrainSize)) {}

RendererOwner::~RendererOwner() { destroyRenderer(Gpu); }

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, add `platform.cpp` to tpj_app's sources, after `park_dialogs.cpp`.

Step 4: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 6: Give the tooling UI's draw data

Files:
- Modify: `src/app/tooling_ui.h:10,28-34`
- Modify: `src/app/tooling_ui.cpp:42-63`
- Modify: `src/app/main.cpp:154,162`

Step 1: In src/app/tooling_ui.h, add `struct ImDrawData;` above `struct SDL_Window;`, and replace build's comment and declaration with:

```cpp
  // Builds the frame's ImGui frame and gives its draw data: the Debug and Tools panels, which set
  // the shown views from their checkboxes, select the tool the player chose, and start the park
  // action they pressed; the Inspector while the interaction has a subject, forgetting it when
  // closed; the graph over the scene while it is shown; the food tooltip at the cursor while the
  // overlay is, on the preview's candidate when it has one; and a shop ghost's context.
  [[nodiscard]] ImDrawData *build(SDL_Window *window, ParkDialogs &dialogs, const World &world,
                                  const OrbitCamera &camera, Interaction &interaction,
                                  const Preview &preview);
```

Step 2: In src/app/tooling_ui.cpp, build's definition returns `ImDrawData *`, and its last line, `ImGui::Render();`, becomes:

```cpp
  ImGui::Render();
  return ImGui::GetDrawData();
```

Step 3: In src/app/main.cpp's runLoop, the build call becomes `ImDrawData *drawData = ui.build(renderer.Window, dialogs, session.world(), camera, interaction, scene.preview());`, and drawFrame's third argument, `ImGui::GetDrawData()`, becomes `drawData`.

Step 4: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 7: Create the Application

Files:
- Create: `src/app/application.h`
- Create: `src/app/application.cpp`
- Modify: `src/app/CMakeLists.txt:16-27`

Step 1: Create src/app/application.h:

```cpp
#ifndef TPJ_APP_APPLICATION_H
#define TPJ_APP_APPLICATION_H

#include "app/frame_clock.h"
#include "app/interaction.h"
#include "app/options.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "app/park_session.h"
#include "app/platform.h"
#include "app/scene_sync.h"
#include "app/tooling_ui.h"
#include "sim/world.h"

namespace tpj {

// The app: the platform owners, acquired first and released last, and the components a frame
// calls, in the order the frame runs.
class Application {
public:
  // Acquires the platform resources in order, stopping at the first that fails, which its owner
  // logs, and starts from the world.
  Application(const Options &options, World start);

  // Runs frames until the player quits or the frame limit is reached. False when a platform
  // resource was not acquired, a frame failed to render, or the loop threw, which it logs as
  // `Main loop: <what>`.
  bool run();

private:
  // The frames, each step a call into a component. False when a frame failed to render.
  bool runFrames();

  // The owners come first, so they are released after every component, in reverse order.
  SdlVideo Video;
  MainWindow Window;
  UiContext Gui;
  RendererOwner Gpu;
  ParkSession Session;
  OrbitCamera Camera;
  SceneSync Scene;
  Interaction Interactions;
  ToolingUi Ui;
  FrameClock Clock;
  ParkDialogs Dialogs;
  int FrameLimit;
  const char *CapturePath;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/application.cpp, with runLoop's body as runFrames and runLoopLogged's catch as run, their order and comments unchanged:

```cpp
#include "app/application.h"

#include "app/cursor.h"
#include "app/platform_input.h"
#include "app/scene_uploads.h"
#include "core/profile.h"
#include "render/renderer.h"

#include <SDL3/SDL.h>

#include <exception>
#include <stdint.h>
#include <utility>

namespace tpj {
namespace {

// The side of the square terrain the renderer draws, centered on the origin, inside which the
// camera's focus stays.
constexpr float PARK_SIZE_METERS = 256.0f;

} // namespace

Application::Application(const Options &options, World start)
    : Window(Video), Gui(Window), Gpu(Window, Gui, PARK_SIZE_METERS), Session(std::move(start)),
      Interactions(Session.generation()), Ui({options.ShowGraph, options.ShowFoodOverlay}),
      Clock(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency()), Dialogs(Window.get()),
      FrameLimit(options.FrameLimit), CapturePath(options.CapturePath) {}

// What the loop throws, such as a world invariant the simulation checks, is logged here, inside
// the owners' lifetime, so they are still released.
bool Application::run() {
  if (!Gpu.ready()) {
    return false;
  }
  try {
    return runFrames();
  } catch (const std::exception &error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Main loop: %s", error.what());
    return false;
  }
}

bool Application::runFrames() {
  Renderer &renderer = Gpu.renderer();
  for (int frame = 1;; ++frame) {
    const FrameInput input = gatherInput();
    if (input.Quit) {
      return true;
    }
    Session.useFileRequest(Dialogs.take(), Dialogs);
    Interactions.follow(Session.generation());

    const FrameStep step = Clock.advance(SDL_GetPerformanceCounter());

    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    Interactions.useButtons(Session.world(), Session.commands(), input.Buttons);
    if (Interactions.picks(input.Buttons)) {
      Interactions.pick(Session.world(), entityUnderCursor(Session.world(), readCursor(Window.get()),
                                                           orbitCameraView(Camera)));
    }

    // The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at
    // the next (principle 10).
    for (uint32_t tick = 0; tick < step.Ticks; ++tick) {
      Session.step();
    }
    if (!uploadWorldMeshes(renderer, Session.world(),
                           Scene.syncWorld(Session.world(), Session.generation()), Camera)) {
      return false;
    }

    updateOrbitCamera(Camera, input.Camera, static_cast<float>(step.Dt), 0.5f * PARK_SIZE_METERS);
    const CameraView view = orbitCameraView(Camera);
    Interactions.movePointer(groundUnderCursor(readCursor(Window.get()), view));
    // Made again only when the world ticks or the edit changes, so the ghost, the overlay, and the
    // tooltips show one candidate, and frames between ticks rebuild nothing.
    const bool remade =
        Scene.syncPreview(Session.world(), Interactions.tentativeEdit(Session.world()));

    ImDrawData *drawData =
        Ui.build(Window.get(), Dialogs, Session.world(), Camera, Interactions, Scene.preview());

    const bool lastFrame = FrameLimit > 0 && frame >= FrameLimit;
    // The meshes are updated after the panels, so a change of the checkbox shows in this frame.
    const PreviewLook look{Interactions.highlighted(Session.world()), Interactions.subject(),
                           Ui.shown().FoodOverlay};
    if ((Scene.syncLook(remade, look) &&
         !uploadPreviewMeshes(renderer, Session.world(), Scene.preview(), look)) ||
        !drawFrame(renderer, view, drawData, lastFrame ? CapturePath : nullptr)) {
      return false;
    }
    TPJ_PROFILE_FRAME();
    if (lastFrame) {
      return true;
    }
  }
}

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, add `application.cpp` to tpj_app's sources, first.

Step 4: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 8: Compose the Application in main.cpp

Files:
- Modify: `src/app/main.cpp`

Step 1: Replace src/app/main.cpp with:

```cpp
#include "app/application.h"
#include "app/options.h"
#include "app/park_session.h"
#include "sim/world.h"

#include <SDL3/SDL_main.h>

#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <utility>

int main(int argc, char **argv) {
  const std::optional<tpj::Options> options = tpj::parseOptions(argc, argv);
  if (!options) {
    return EXIT_FAILURE;
  }
  tpj::OpenedPark start = tpj::startingPark(options->ParkPath, options->Ticks);
  if (!start.Park) {
    (void)fprintf(stderr, "%s\n", start.Error.c_str());
    return EXIT_FAILURE;
  }
  // The hash needs no window, so it can be checked where there is no display.
  if (options->PrintHash) {
    printf("tick %llu hash %016llx\n", static_cast<unsigned long long>(start.Park->Tick),
           static_cast<unsigned long long>(tpj::hashWorld(*start.Park)));
    return EXIT_SUCCESS;
  }
  tpj::Application app(*options, std::move(*start.Park));
  return app.run() ? EXIT_SUCCESS : EXIT_FAILURE;
}
```

Step 2: Build the app and its tests, run the command line's and the options' tests, and check main.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#command_line_test],[#options_test]" && grep -n "SDL_Init\|ImGui\|createRenderer\|for (" src/app/main.cpp`
Expected: the build succeeds with no warnings, every test in command_line_test.cpp and options_test.cpp passes, and grep prints nothing.

### Task 9: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- 'src/app/*.h' 'src/app/*.cpp' 'tests/app/*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes, the layer check and the private header check among them.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Step 3: Check the scene.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/composed-entry.bmp; echo $?`
Expected: it prints 0, and build/composed-entry.bmp shows the park with its walkways, guests, the graph, and the food band, and the Debug panel with `Shop 7: stock 6, queue 6, on order 16, service rate`, `Guests 35, mean hunger 0.54`, and `Waiting 7, meals eaten 17`, as before the change.

Step 4: Check the app by hand. Run `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park`, then:
- orbit, pan, and zoom the camera, and resize the window;
- with Look, click a guest;
- press Open park and open a park from the parks folder;
- close the window.

Expected: the camera and the window respond as before, the Inspector shows the guest, the opened park replaces the scene with the camera framed on it, and the app exits at once, with no error in the log.

### Task 10: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Compose the app from options, owners, and an Application`.
