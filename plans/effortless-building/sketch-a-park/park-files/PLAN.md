# Implementation Plan: Park Files

## Goal

Let the player start a new park, open a park file, and save one from the Tools panel through SDL's file dialogs, with file reading and writing in a small library the app's tests can call.

## Approach

tpj_park_files, a static library in src/app, holds openParkFile, saveParkFile, and withParkExtension over SDL_LoadFile, SDL_SaveFile, loadWorld, and saveWorld. --park uses it too. The Tools panel returns a park action. main.cpp shows the dialogs and receives their paths in a mutex-guarded slot that lives for the whole program. At the start of each frame it saves, or replaces the world, empties the queue, reselects the tool, and forgets the drawn intent so the camera frames the new park.

All commands run from the repository root, /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones.

## Tasks

### Task 1: Specify park files

Files:
- Modify: `src/app/SPEC.md`

Step 1: In "## Park", replace the first sentence, "The app starts from makeNewPark(1), or from the park file --park names, loaded with makeParkSchema, and resolves it.", with the sentence FEATURE.md gives.

Step 2: Between the "## Tools" section and "## Camera", insert the "## Park files" section FEATURE.md gives.

Run: `grep -c "^## " src/app/SPEC.md`
Expected: `7`

### Task 2: Declare the park files library

Files:
- Create: `src/app/park_file.h`
- Create: `src/app/park_file.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/app/CMakeLists.txt`

Step 1: Create src/app/park_file.h:

```cpp
#ifndef TPJ_APP_PARK_FILE_H
#define TPJ_APP_PARK_FILE_H

#include "sim/world.h"

#include <optional>
#include <string>
#include <string_view>

namespace tpj {

// A park file's world, or why there is none.
struct OpenedPark {
  // Loaded with makeParkSchema and resolved.
  std::optional<World> Park;
  // Empty when Park holds a world.
  std::string Error;
};

// Reads the park file at the path, loads it, and resolves it. See app/SPEC.md.
OpenedPark openParkFile(const char *path);
// Writes the world's save to the path, replacing any file there. Returns an empty message, or why
// it could not.
std::string saveParkFile(const World &world, const char *path);
// The path, followed by .park when its file name has no extension.
std::string withParkExtension(std::string_view path);

} // namespace tpj

#endif
```

Step 2: Create src/app/park_file.cpp with stubs:

```cpp
#include "app/park_file.h"

namespace tpj {

OpenedPark openParkFile(const char * /*path*/) {
  return {std::nullopt, "not implemented"};
}

std::string saveParkFile(const World & /*world*/, const char * /*path*/) {
  return "not implemented";
}

std::string withParkExtension(std::string_view path) {
  return std::string(path);
}

} // namespace tpj
```

Step 3: In src/app/CMakeLists.txt, before `add_executable(tpj_app`, add:

```cmake
# Park files, apart from the window, so the app's tests read and write them without a display.
add_library(tpj_park_files STATIC
    park_file.cpp)
target_include_directories(tpj_park_files PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_park_files PUBLIC tpj_sim PRIVATE SDL3::SDL3)
tpj_configure_target(tpj_park_files)

```

and change tpj_app's link line to `target_link_libraries(tpj_app PRIVATE tpj_park_files tpj_render tpj_sim tpj_tools)`.

Step 4: In tests/app/CMakeLists.txt, change the comment's last sentence to "They link the sim and tpj_park_files, to read the errors a park file's load gives and to call the park file functions." and the link line to `target_link_libraries(tpj_app_tests PRIVATE tpj_park_files tpj_sim Catch2::Catch2WithMain)`.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 3: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/app/SPEC.md and src/sim/SPEC.md, and the public header src/app/park_file.h. Its tests of criteria 1 to 5 must build and fail on behavior.

### Task 4: Implement the park files library

Files:
- Modify: `src/app/park_file.cpp`

Step 1: Replace src/app/park_file.cpp with:

```cpp
#include "app/park_file.h"

#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <SDL3/SDL.h>

namespace tpj {

OpenedPark openParkFile(const char *path) {
  size_t size = 0;
  void *text = SDL_LoadFile(path, &size);
  if (text == nullptr) {
    return {std::nullopt, std::string("Cannot read ") + path + ": " + SDL_GetError()};
  }
  OpenedPark opened;
  try {
    opened.Park =
        loadWorld(makeParkSchema(), std::string_view(static_cast<const char *>(text), size));
  } catch (const LoadError &error) {
    opened.Error = std::string("Cannot load ") + path + ": " + error.what();
  }
  SDL_free(text);
  if (opened.Park) {
    resolveWorld(*opened.Park);
  }
  return opened;
}

std::string saveParkFile(const World &world, const char *path) {
  const std::string text = saveWorld(world);
  if (!SDL_SaveFile(path, text.data(), text.size())) {
    return std::string("Cannot save ") + path + ": " + SDL_GetError();
  }
  return {};
}

std::string withParkExtension(std::string_view path) {
  const size_t slash = path.find_last_of("/\\");
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  std::string result(path);
  if (name.find('.') == std::string_view::npos) {
    result += ".park";
  }
  return result;
}

} // namespace tpj
```

Run: `cmake --build --preset linux-debug --target tpj_app_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_app_tests "<the test pass's park file test names, or its file's tag>" 2>&1 | tail -2`
Expected: no warnings, and `All tests passed`, covering criteria 1 to 5.

### Task 5: Open --park through openParkFile

Files:
- Modify: `src/app/main.cpp`

Step 1: Add `#include "app/park_file.h"` after `#include "app/orbit_camera.h"`.

Step 2: Before startingWorld, add:

```cpp
// A new park, resolved.
tpj::World resolvedNewPark() {
  tpj::World world = tpj::makeNewPark(1);
  tpj::resolveWorld(world);
  return world;
}
```

Step 3: Replace the body of startingWorld with:

```cpp
  std::optional<tpj::World> world;
  if (options.ParkPath == nullptr) {
    world = resolvedNewPark();
  } else {
    tpj::OpenedPark opened = tpj::openParkFile(options.ParkPath);
    if (!opened.Park) {
      (void)fprintf(stderr, "%s\n", opened.Error.c_str());
      return std::nullopt;
    }
    world = std::move(opened.Park);
  }
  for (uint64_t tick = 0; tick < options.Ticks; ++tick) {
    tpj::stepWorld(*world);
  }
  return world;
```

Run: `cmake --build --preset linux-debug --target tpj_app tpj_app_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_app_tests 2>&1 | tail -2`
Expected: no warnings, and `All tests passed`, including the command-line tests for unreadable and unloadable park files.

### Task 6: Add the park buttons to the Tools panel

Files:
- Modify: `src/app/tool_panel.h`
- Modify: `src/app/tool_panel.cpp`

Step 1: In src/app/tool_panel.h, add `#include <stdint.h>` after `#include <optional>`, and replace the drawToolPanel declaration and its comment with:

```cpp
// A park button the player pressed in the Tools panel.
enum class ParkAction : uint8_t { None, New, Open, Save };

// What the player chose in the Tools panel this frame.
struct ToolPanelChoice {
  // The tool to select: a different one, or the current one again when they cancelled a path.
  std::optional<ToolKind> Tool;
  ParkAction Park = ParkAction::None;
};

// Draws the Tools panel: the park buttons, disabled while a dialog shows, a choice for each tool,
// and while the tool is drawing a path, a hint and a Cancel path button. Call between
// ImGui::NewFrame and ImGui::Render.
ToolPanelChoice drawToolPanel(ToolKind current, bool drawing, bool dialogShowing);
```

Step 2: In src/app/tool_panel.cpp, replace drawToolPanel with:

```cpp
ToolPanelChoice drawToolPanel(ToolKind current, bool drawing, bool dialogShowing) {
  ToolPanelChoice choice;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginDisabled(dialogShowing);
    if (ImGui::Button("New park")) {
      choice.Park = ParkAction::New;
    }
    ImGui::SameLine();
    if (ImGui::Button("Open park")) {
      choice.Park = ParkAction::Open;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save park")) {
      choice.Park = ParkAction::Save;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    for (const ToolChoice &option : TOOL_CHOICES) {
      if (ImGui::RadioButton(option.Label, current == option.Kind) && current != option.Kind) {
        choice.Tool = option.Kind;
      }
    }
    if (drawing) {
      ImGui::Separator();
      ImGui::TextUnformatted("Click the last point again to finish.");
      if (ImGui::Button("Cancel path")) {
        choice.Tool = current;
      }
    }
  }
  ImGui::End();
  return choice;
}
```

Step 3: In src/app/main.cpp, in drawPanels, replace the `if (const std::optional<tpj::ToolKind> kind = ...` statement with:

```cpp
  const tpj::ToolPanelChoice choice =
      tpj::drawToolPanel(tool.Kind, !tool.Drawn.empty(), false);
  if (choice.Tool) {
    tpj::selectTool(tool, *choice.Tool);
  }
```

Run: `cmake --build --preset linux-debug --target tpj_app 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 7: Hand dialog paths to the loop and act on them

Files:
- Modify: `src/app/main.cpp`

Step 1: Add `#include <mutex>` and `#include <string>` to the standard includes, in alphabetical order.

Step 2: After the DrawnGhost struct, add:

```cpp
// What the park buttons and the file dialogs ask of the main loop, handed over under a lock, since
// a dialog's callback may run on another thread.
struct FileRequests {
  std::mutex Lock;
  bool DialogShowing = false;
  tpj::ParkAction Action = tpj::ParkAction::None;
  std::string Path;
};

// One request, taken by the main loop.
struct FileRequest {
  tpj::ParkAction Action = tpj::ParkAction::None;
  std::string Path;
};

constexpr SDL_DialogFileFilter PARK_FILTERS[] = {{"Park files", "park"}};

// Lives for the whole program, since a dialog left open at quit may still call back.
FileRequests &fileRequests() {
  static FileRequests requests;
  return requests;
}

// A dialog's callback: hands the first chosen path to the main loop, and nothing when cancelled.
void handOver(void *userdata, const char *const *filelist, tpj::ParkAction action) {
  auto &requests = *static_cast<FileRequests *>(userdata);
  if (filelist == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File dialog: %s", SDL_GetError());
  }
  const std::scoped_lock lock(requests.Lock);
  requests.DialogShowing = false;
  if (filelist != nullptr && filelist[0] != nullptr) {
    requests.Action = action;
    requests.Path = filelist[0];
  }
}

void SDLCALL onOpenChosen(void *userdata, const char *const *filelist, int /*filter*/) {
  handOver(userdata, filelist, tpj::ParkAction::Open);
}

void SDLCALL onSaveChosen(void *userdata, const char *const *filelist, int /*filter*/) {
  handOver(userdata, filelist, tpj::ParkAction::Save);
}

bool dialogShowing() {
  FileRequests &requests = fileRequests();
  const std::scoped_lock lock(requests.Lock);
  return requests.DialogShowing;
}

// Acts on a park button: New waits for the next frame, and Open and Save show their dialog. Does
// nothing while a dialog is showing.
void startParkAction(tpj::ParkAction action, SDL_Window *window) {
  if (action == tpj::ParkAction::None) {
    return;
  }
  FileRequests &requests = fileRequests();
  {
    const std::scoped_lock lock(requests.Lock);
    if (requests.DialogShowing) {
      return;
    }
    if (action == tpj::ParkAction::New) {
      requests.Action = action;
      requests.Path.clear();
      return;
    }
    requests.DialogShowing = true;
  }
  // The callback may run before these return, so the lock is not held while they run.
  if (action == tpj::ParkAction::Open) {
    SDL_ShowOpenFileDialog(onOpenChosen, &requests, window, PARK_FILTERS, 1, nullptr, false);
  } else {
    SDL_ShowSaveFileDialog(onSaveChosen, &requests, window, PARK_FILTERS, 1, nullptr);
  }
}

FileRequest takeFileRequest() {
  FileRequests &requests = fileRequests();
  const std::scoped_lock lock(requests.Lock);
  FileRequest request{requests.Action, std::move(requests.Path)};
  requests.Action = tpj::ParkAction::None;
  requests.Path.clear();
  return request;
}

void reportFileError(SDL_Window *window, const std::string &message) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
  (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Theme Park Jones", message.c_str(), window);
}

// Asks whether to replace the file at the path. True when the player chooses Replace.
bool confirmReplace(SDL_Window *window, const std::string &path) {
  const std::string message = path + " already exists. Replace it?";
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Replace"}};
  const SDL_MessageBoxData data{SDL_MESSAGEBOX_WARNING, window, "Theme Park Jones",
                                message.c_str(), 2, buttons, nullptr};
  int chosen = 0;
  return SDL_ShowMessageBox(&data, &chosen) && chosen == 1;
}

// Saves, or replaces the world, as the request asks. Returns true when it replaced the world. A
// save whose extension was added asks before replacing a file, since the dialog asked only about
// the name as typed.
bool useFileRequest(SDL_Window *window, const FileRequest &request, tpj::World &world) {
  if (request.Action == tpj::ParkAction::Save) {
    const std::string path = tpj::withParkExtension(request.Path);
    if (path != request.Path && SDL_GetPathInfo(path.c_str(), nullptr) &&
        !confirmReplace(window, path)) {
      return false;
    }
    const std::string error = tpj::saveParkFile(world, path.c_str());
    if (!error.empty()) {
      reportFileError(window, error);
    }
    return false;
  }
  if (request.Action == tpj::ParkAction::New) {
    world = resolvedNewPark();
    return true;
  }
  if (request.Action == tpj::ParkAction::Open) {
    tpj::OpenedPark opened = tpj::openParkFile(request.Path.c_str());
    if (!opened.Park) {
      reportFileError(window, opened.Error);
      return false;
    }
    world = std::move(*opened.Park);
    return true;
  }
  return false;
}
```

Step 3: Change drawPanels to take the window first, `void drawPanels(SDL_Window *window, const tpj::World &world, const tpj::OrbitCamera &camera, tpj::ToolState &tool)`, update its comment to "Builds the Debug and Tools panels, selecting the tool the player chose and starting the park action they pressed.", pass `dialogShowing()` in place of `false` to drawToolPanel, and after the `if (choice.Tool)` block add `startParkAction(choice.Park, window);`.

Step 4: In runLoop, directly after the gatherInput block that returns on quit, add:

```cpp
    if (useFileRequest(renderer.Window, takeFileRequest(), world)) {
      commands.clear();
      tpj::selectTool(tool, tool.Kind);
      drawn.reset();
    }
```

and change the drawPanels call to `drawPanels(renderer.Window, world, camera, tool);`.

Run: `cmake --build --preset linux-debug --target tpj_app 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 8: Verify

Step 1: Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Step 2: Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug 2>&1 | tail -3`
Expected: no warnings, and `100% tests passed`.

Step 3: Run: `cmake.exe --build --preset windows-debug 2>&1 | tail -1; ctest.exe --preset windows-debug 2>&1 | tail -3`
Expected: the build finishes, and `100% tests passed`.

Step 4: Run: `scripts/cross-build-check.sh`
Expected: it passes.

Step 5: Run: `build/linux-debug/ThemeParkJones --park tests/parks/sketch.park --capture build/linux-debug/sketch.bmp`, convert the capture to PNG, and view it.
Expected: the Tools panel shows New park, Open park, and Save park above the tools.

Step 6: Dispatch the reviewer agent on the staged diff via the reviewing skill, with FEATURE.md, src/app/SPEC.md, and docs/principles.md.
Expected: Evan decides on each finding.

Step 7: Ask Evan to run build/windows-debug/ThemeParkJones.exe and check criterion 6.
Expected: Evan confirms it.

### Task 9: Commit

Step 1: Commit via the commit-hygiene skill, staging the paths this plan and the test pass touched by name, never image.png, with the message:

```
App: Start, open, and save parks from the Tools panel
```

and a body saying that tpj_park_files reads and writes park files for --park and the panel, that SDL's dialogs hand their path to the loop under a lock, and that opening or starting a park replaces the world at the start of a frame with the queue emptied and the tool's hold dropped, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

Run: `git log -1 --format=%s`
Expected: `App: Start, open, and save parks from the Tools panel`
