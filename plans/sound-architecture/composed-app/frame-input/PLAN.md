# Implementation Plan: Frame Input

## Goal

Move input mapping, the cursor's arithmetic, and the frame clock out of main.cpp into window-free components in tpj_app_core, with a thin platform edge in the executable, and move the orbit camera into the library.

## Approach

Each core takes plain values: input mapping an SDL_Event, which its header forward-declares, the key array as a span, and ImGui's capture flags as booleans; the cursor the window size and mouse position; the frame clock counter readings and the frequency. platform_input.cpp, in the executable, holds the SDL and ImGui calls that gather those values. main.cpp's runLoop then calls gatherInput, readCursor, and the clock, and steps the session as many times as the clock says, with the arithmetic unchanged so every frame steps the ticks it stepped before.

## Placement

Decision 0027 places each behavior this feature adds:

- Mapping an event and the held keys to the frame's camera input, buttons, and quit request: app, new component app/input_map.h, in tpj_app_core. It is the input's rule, a function of plain values, so it needs no window and tests check it. PointerButtons moves here from interaction.h, since input mapping produces it, and interaction.h includes input_map.h.
- The cursor's normalized device coordinates and aspect ratio, and the ground and entity under it: app, new component app/cursor.h, in tpj_app_core. It turns a window size and mouse position into picking's inputs, needing no window.
- The frame's elapsed time, its clamp, and the ticks it steps: app, new component app/frame_clock.h, in tpj_app_core. It owns the tick accumulator, a function of counter readings, so tests feed it readings. MAX_FRAME_SECONDS moves here from main.cpp.
- Draining SDL's events through ImGui, reading the held keys, and reading the cursor from SDL: app, new component app/platform_input.h, in tpj_app. It is the input's platform edge, the only part that calls SDL and ImGui for input, and holds no rule of its own.
- The view an orbit camera sees: app, app/orbit_camera.h, as orbitCameraView. The orbit camera owns its own geometry, and main.cpp's cameraView helper was that geometry. orbit_camera.cpp moves into tpj_app_core, since it needs no window.
- main.cpp: composition only. runLoop calls gatherInput, readCursor, and the clock where it gathered input, read the cursor, and timed the frame. It gains no concern and loses CursorNdc, cursorNdc, groundUnderCursor, cameraView, entityUnderCursor, keyAxis, addMouseInput, gatherInput, MAX_FRAME_SECONDS, and the accumulator.

## Tasks

### Task 1: Describe the frame input in the app spec

Files:
- Modify: `src/app/SPEC.md:7,39`

Step 1: In "## Main loop", add FEATURE.md's three sentences for it after the sentence ending "so a stall does not trigger a burst of ticks.", with their exact text.

Step 2: In "## Camera", add FEATURE.md's new paragraph after the paragraph beginning "Pitch stays between about 10 and 85 degrees", with its exact text.

### Task 2: Declare input mapping

Files:
- Create: `src/app/input_map.h`
- Create: `src/app/input_map.cpp`
- Modify: `src/app/interaction.h:16-20`

Step 1: Create src/app/input_map.h:

```cpp
#ifndef TPJ_APP_INPUT_MAP_H
#define TPJ_APP_INPUT_MAP_H

#include "app/orbit_camera.h"

#include <span>

union SDL_Event;

namespace tpj {

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// The input gathered over one frame: the camera's, the left button's, and whether the player asked
// to quit.
struct FrameInput {
  CameraInput Camera;
  PointerButtons Buttons;
  bool Quit = false;
};

// Adds one event to the frame's input. A quit event asks to quit, and a left button release is
// always recorded. While ImGui wants the mouse nothing else reaches the input. Otherwise a left
// button press is recorded, motion adds its relative pixels to the orbit deltas with the right
// button held, or else to the pan deltas with the middle button held, and a wheel event adds its
// vertical steps to the zoom.
void mapEvent(const SDL_Event &event, bool mouseWanted, FrameInput &input);
// Sets the camera's move and rotate axes from the keys held, indexed by scancode: W minus S
// forward, D minus A right, and Q minus E rotate, a held key counting 1. A scancode past the end of
// the keys counts as not held. While ImGui wants the keyboard it changes nothing.
void mapKeys(std::span<const bool> keys, bool keyboardWanted, CameraInput &camera);

} // namespace tpj

#endif
```

Step 2: Create src/app/input_map.cpp, including "app/input_map.h", with stubs inside namespace tpj that do nothing. Mark unused parameters with `/*name*/` comments.

Step 3: In src/app/interaction.h, delete PointerButtons with its comment, and add `#include "app/input_map.h"` as the first include.

### Task 3: Declare the cursor

Files:
- Create: `src/app/cursor.h`
- Create: `src/app/cursor.cpp`

Step 1: Create src/app/cursor.h:

```cpp
#ifndef TPJ_APP_CURSOR_H
#define TPJ_APP_CURSOR_H

#include "render/renderer.h"
#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Where the cursor lies in normalized device coordinates, X from -1 at the window's left to 1 at
// its right and Y from -1 at its bottom to 1 at its top, and the window's width over its height.
struct CursorNdc {
  float X = 0.0f;
  float Y = 0.0f;
  float Aspect = 1.0f;

  bool operator==(const CursorNdc &) const = default;
};

// The cursor at (x, y) pixels from the top left of a window width by height pixels, or none when
// the width or the height is not positive.
std::optional<CursorNdc> cursorNdc(int width, int height, float x, float y);
// The ground under the cursor, as groundAtCursor gives it for the view, or none without a cursor.
std::optional<ParkPoint> groundUnderCursor(const std::optional<CursorNdc> &cursor,
                                           const CameraView &view);
// The entity the cursor's ray first meets, as entityAtCursor gives it for the world and the view,
// or none without a cursor.
std::optional<EntityKey> entityUnderCursor(const World &world,
                                           const std::optional<CursorNdc> &cursor,
                                           const CameraView &view);

} // namespace tpj

#endif
```

Step 2: Create src/app/cursor.cpp, including "app/cursor.h", with stubs inside namespace tpj that return `std::nullopt`. Mark unused parameters with `/*name*/` comments.

### Task 4: Declare the frame clock

Files:
- Create: `src/app/frame_clock.h`
- Create: `src/app/frame_clock.cpp`

Step 1: Create src/app/frame_clock.h:

```cpp
#ifndef TPJ_APP_FRAME_CLOCK_H
#define TPJ_APP_FRAME_CLOCK_H

#include <stdint.h>

namespace tpj {

// The longest time one frame covers, so a stall does not step a burst of ticks.
inline constexpr double MAX_FRAME_SECONDS = 0.25;

// What one frame covers: its elapsed seconds, clamped, and the simulation ticks it steps.
struct FrameStep {
  double Dt = 0.0;
  uint32_t Ticks = 0;

  bool operator==(const FrameStep &) const = default;
};

// Turns performance counter readings into each frame's time and ticks, carrying the time a frame
// does not step to the next.
class FrameClock {
public:
  // Starts at a counter reading, the counter advancing frequency readings per second.
  FrameClock(uint64_t counter, uint64_t frequency) : Last(counter), Frequency(frequency) {}

  // For the reading at a frame's start: the seconds since the last reading, at most
  // MAX_FRAME_SECONDS, and as many SIM_TICK_SECONDS ticks as the time not yet stepped then holds,
  // keeping the rest.
  FrameStep advance(uint64_t counter);

private:
  uint64_t Last;
  uint64_t Frequency;
  double Unstepped = 0.0;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/frame_clock.cpp, including "app/frame_clock.h", with a stub inside namespace tpj: advance returns `{}`. Mark the unused parameter with a `/*counter*/` comment.

### Task 5: Declare orbitCameraView

Files:
- Modify: `src/app/orbit_camera.h:4-5,37`
- Modify: `src/app/orbit_camera.cpp`

Step 1: In src/app/orbit_camera.h, add `#include "render/renderer.h"` after `#include "render/park_mesh.h"`, and after the declaration of orbitCameraEye add:

```cpp
// The view the camera gives: from its eye toward its focus, with CameraView's other fields as a
// default CameraView has them.
CameraView orbitCameraView(const OrbitCamera &camera);
```

Step 2: In src/app/orbit_camera.cpp, after orbitCameraEye's definition, add a stub of orbitCameraView that returns `{}`, with a `/*camera*/` comment for its parameter.

### Task 6: Move the components into tpj_app_core

Files:
- Modify: `src/app/CMakeLists.txt:2-9,12-21`

Step 1: Set tpj_app_core's sources to cursor.cpp, frame_clock.cpp, input_map.cpp, interaction.cpp, orbit_camera.cpp, park_file.cpp, park_file_requests.cpp, park_session.cpp, and scene_sync.cpp, one per line in that order, and change its link line to `target_link_libraries(tpj_app_core PUBLIC tpj_legible tpj_render tpj_sim tpj_tools PRIVATE SDL3::SDL3)`. Remove orbit_camera.cpp from tpj_app's sources.

Step 2: Configure and build.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests`
Expected: the build succeeds with no warnings, and the app still links, with orbitCameraView's stub unused.

### Task 7: Test pass

Step 1: Dispatch the test-writer agent with FEATURE.md, src/app/SPEC.md, and src/render/SPEC.md, and the public headers src/app/input_map.h, src/app/cursor.h, src/app/frame_clock.h, src/app/orbit_camera.h, and src/render/picking.h. It creates tests/app/input_map_test.cpp, tests/app/cursor_test.cpp, tests/app/frame_clock_test.cpp, and tests/app/orbit_camera_test.cpp, and adds them to tpj_app_tests in tests/app/CMakeLists.txt.

Step 2: Build and run them.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#input_map_test],[#cursor_test],[#frame_clock_test],[#orbit_camera_test]"`
Expected: the build succeeds, and the tests that need the stubs' behavior fail.

### Task 8: Implement input mapping

Files:
- Modify: `src/app/input_map.cpp`

Step 1: Replace the file with:

```cpp
#include "app/input_map.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <stddef.h>

namespace tpj {
namespace {

bool isHeld(std::span<const bool> keys, SDL_Scancode code) {
  const auto index = static_cast<size_t>(code);
  return index < keys.size() && keys[index];
}

float keyAxis(std::span<const bool> keys, SDL_Scancode positive, SDL_Scancode negative) {
  return (isHeld(keys, positive) ? 1.0f : 0.0f) - (isHeld(keys, negative) ? 1.0f : 0.0f);
}

void addMouseInput(const SDL_Event &event, CameraInput &camera) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if ((event.motion.state & SDL_BUTTON_RMASK) != 0) {
      camera.OrbitDx += event.motion.xrel;
      camera.OrbitDy += event.motion.yrel;
    } else if ((event.motion.state & SDL_BUTTON_MMASK) != 0) {
      camera.PanDx += event.motion.xrel;
      camera.PanDy += event.motion.yrel;
    }
  } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    camera.Zoom += event.wheel.y;
  }
}

} // namespace

void mapEvent(const SDL_Event &event, bool mouseWanted, FrameInput &input) {
  if (event.type == SDL_EVENT_QUIT) {
    input.Quit = true;
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    input.Buttons.Released = true;
  } else if (!mouseWanted) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
      input.Buttons.Pressed = true;
    }
    addMouseInput(event, input.Camera);
  }
}

void mapKeys(std::span<const bool> keys, bool keyboardWanted, CameraInput &camera) {
  if (keyboardWanted) {
    return;
  }
  camera.MoveForward = keyAxis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
  camera.MoveRight = keyAxis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);
  camera.Rotate = keyAxis(keys, SDL_SCANCODE_Q, SDL_SCANCODE_E);
}

} // namespace tpj
```

Step 2: Run input mapping's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#input_map_test]"`
Expected: every test passes.

### Task 9: Implement the cursor

Files:
- Modify: `src/app/cursor.cpp`

Step 1: Replace the stubs with these definitions, and add `#include "render/picking.h"` after `#include "app/cursor.h"`:

```cpp
std::optional<CursorNdc> cursorNdc(int width, int height, float x, float y) {
  if (width <= 0 || height <= 0) {
    return std::nullopt;
  }
  return CursorNdc{2.0f * x / static_cast<float>(width) - 1.0f,
                   1.0f - 2.0f * y / static_cast<float>(height),
                   static_cast<float>(width) / static_cast<float>(height)};
}

std::optional<ParkPoint> groundUnderCursor(const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
}

std::optional<EntityKey> entityUnderCursor(const World &world,
                                           const std::optional<CursorNdc> &cursor,
                                           const CameraView &view) {
  if (!cursor) {
    return std::nullopt;
  }
  return entityAtCursor(world, view, cursor->Aspect, cursor->X, cursor->Y);
}
```

Step 2: Run the cursor's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#cursor_test]"`
Expected: every test passes.

### Task 10: Implement the frame clock

Files:
- Modify: `src/app/frame_clock.cpp`

Step 1: Replace the stub with this definition, and add `#include "sim/world.h"` after `#include "app/frame_clock.h"` and `#include <algorithm>` below them:

```cpp
FrameStep FrameClock::advance(uint64_t counter) {
  FrameStep step;
  step.Dt = std::min(static_cast<double>(counter - Last) / static_cast<double>(Frequency),
                     MAX_FRAME_SECONDS);
  Last = counter;
  Unstepped += step.Dt;
  while (Unstepped >= SIM_TICK_SECONDS) {
    ++step.Ticks;
    Unstepped -= SIM_TICK_SECONDS;
  }
  return step;
}
```

Step 2: Run the frame clock's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#frame_clock_test]"`
Expected: every test passes.

### Task 11: Implement orbitCameraView

Files:
- Modify: `src/app/orbit_camera.cpp`

Step 1: Replace orbitCameraView's stub with:

```cpp
CameraView orbitCameraView(const OrbitCamera &camera) {
  CameraView view;
  view.Eye = orbitCameraEye(camera);
  view.Target = camera.Focus;
  return view;
}
```

Step 2: Run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#orbit_camera_test]"`
Expected: every test passes.

### Task 12: Create the input's platform edge

Files:
- Create: `src/app/platform_input.h`
- Create: `src/app/platform_input.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Create src/app/platform_input.h:

```cpp
#ifndef TPJ_APP_PLATFORM_INPUT_H
#define TPJ_APP_PLATFORM_INPUT_H

#include "app/cursor.h"
#include "app/input_map.h"

#include <optional>

struct SDL_Window;

namespace tpj {

// Drains SDL's pending events, handing each to ImGui and then to mapEvent, and maps the held keys
// with mapKeys, each given whether ImGui wants the mouse or the keyboard.
FrameInput gatherInput();
// The cursor in the window, or none while ImGui wants the mouse or the window's size cannot be
// read.
std::optional<CursorNdc> readCursor(SDL_Window *window);

} // namespace tpj

#endif
```

Step 2: Create src/app/platform_input.cpp:

```cpp
#include "app/platform_input.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <span>
#include <stddef.h>

namespace tpj {

FrameInput gatherInput() {
  FrameInput input;
  const ImGuiIO &io = ImGui::GetIO();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    mapEvent(event, io.WantCaptureMouse, input);
  }
  int count = 0;
  const bool *keys = SDL_GetKeyboardState(&count);
  mapKeys(std::span<const bool>(keys, static_cast<size_t>(count)), io.WantCaptureKeyboard,
          input.Camera);
  return input;
}

std::optional<CursorNdc> readCursor(SDL_Window *window) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height)) {
    return std::nullopt;
  }
  float x = 0.0f;
  float y = 0.0f;
  SDL_GetMouseState(&x, &y);
  return cursorNdc(width, height, x, y);
}

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, add platform_input.cpp to tpj_app's sources, in sorted order after park_dialogs.cpp.

### Task 13: Compose them in main.cpp

Files:
- Modify: `src/app/main.cpp`

Step 1: Delete from main.cpp, with their comments: MAX_FRAME_SECONDS, CursorNdc, cursorNdc, groundUnderCursor, cameraView, entityUnderCursor, keyAxis, addMouseInput, and gatherInput.

Step 2: Add `#include "app/cursor.h"`, `#include "app/frame_clock.h"`, and `#include "app/platform_input.h"` in sorted order, and remove `#include "render/picking.h"`.

Step 3: In buildUi, `cameraView(camera)` becomes `tpj::orbitCameraView(camera)`, and `groundUnderCursor(window, view)` becomes `tpj::groundUnderCursor(tpj::readCursor(window), view)`.

Step 4: In runLoop, replace the locals lastCounter and simAccumulator with `tpj::FrameClock clock(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency());`. Then:

- replace the loop's first five lines, from `tpj::CameraInput input;` through the `return true;` block, with:

```cpp
    const tpj::FrameInput input = tpj::gatherInput();
    if (input.Quit) {
      return true;
    }
```

- replace the block from `const uint64_t counter = SDL_GetPerformanceCounter();` through the clamp's closing brace with `const tpj::FrameStep step = clock.advance(SDL_GetPerformanceCounter());`;
- in the useButtons call and the picks condition, `buttons` becomes `input.Buttons`, and the pick's entity becomes `tpj::entityUnderCursor(session.world(), tpj::readCursor(renderer.Window), tpj::orbitCameraView(camera))`;
- replace the accumulator's lines below the principle 10 comment, keeping the comment, with:

```cpp
    for (uint32_t tick = 0; tick < step.Ticks; ++tick) {
      session.step();
    }
```

- the updateOrbitCamera call becomes `tpj::updateOrbitCamera(camera, input.Camera, static_cast<float>(step.Dt), 0.5f * PARK_SIZE_METERS);`, the view becomes `const tpj::CameraView view = tpj::orbitCameraView(camera);`, and the movePointer call becomes `interaction.movePointer(tpj::groundUnderCursor(tpj::readCursor(renderer.Window), view));`.

Step 5: Build the app and run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests && build/windows-debug/tpj_app_tests.exe`
Expected: the build succeeds with no warnings, and every test passes.

Run: `grep -n "SDL_PollEvent\|SDL_GetMouseState\|simAccumulator\|MAX_FRAME_SECONDS\|cameraView(" src/app/main.cpp`
Expected: no output.

### Task 14: Verify on both builds

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

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/frame-input.bmp`
Expected: it exits with status 0, and build/frame-input.bmp shows the park framed with its walkways, guests, the graph, and the food band, as before the change.

Step 4: Check the input by hand. Run `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park`, then:
- drag with the right button, drag with the middle button, scroll, and hold W, A, S, D, Q, and E;
- with Look, click a guest;
- choose Guest path, click two points, and click the last again to finish;
- check Food overlay and hover a walkway;
- close the window.

Expected: the camera orbits, pans, zooms, moves, and rotates; the Inspector shows the guest; the path is added; the food tooltip follows the cursor; the window closes.

### Task 15: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Move frame input, the cursor, and the clock out of main`.
