# Implementation Plan: Park Session

## Goal

Move the world, the command queue, the starting park, and the park file flow out of main.cpp into ParkSession, ParkFileRequests, and ParkDialogs, with the window-free ones in tpj_app_core and tested through their headers.

## Approach

tpj_park_files becomes tpj_app_core, which gains park_file_requests.cpp, the mailbox moved out of main.cpp's FileRequests functions, and park_session.cpp, which holds the world, its queue, its generation, startingPark, and useFileRequest with the two window calls behind the ParkFileEdge interface. ParkDialogs, in the executable, implements that interface with SDL's message boxes, shows SDL's dialogs, and keeps the one ParkFileRequests for the program's lifetime. main.cpp then builds the session from startingPark, holds a ParkDialogs, and keys its existing resets on the session's generation.

## Placement

Decision 0027 places each behavior this feature adds:

- Holding the world and the command queue, stepping them, replacing the world, and the generation: app, new component app/park_session.h, ParkSession, in tpj_app_core. It is the world's one owner, so the one place it can be replaced, and it needs no window, so tests check it.
- Using a park file request (choosing the save path, asking before a replace, saving, opening, replacing): app, ParkSession::useFileRequest. The decision is the session's, since it acts on the world; the two window calls go through ParkFileEdge, and park_session.cpp calls no SDL.
- Whether a file exists at a path: app, park_file.h's fileExists, beside the park file input and output it already does through SDL, so the session's save decision stays free of platform calls.
- The starting park: app, park_session.h's startingPark, since it gives the world a session begins with, and it needs no window.
- The request mailbox shared with the dialog threads: app, new component app/park_file_requests.h, ParkFileRequests, in tpj_app_core. Handing requests between threads under a lock is its own concern, apart from what a request does to the world. ParkAction moves into this header from app/tool_panel.h, since it is the mailbox's vocabulary and the library cannot include ImGui code.
- SDL's dialogs, the confirm and error message boxes, the parks folder, and the mailbox's one instance: app, new component app/park_dialogs.h, ParkDialogs, in tpj_app. It is the flow's platform edge, and the instance's lifetime is set by SDL's callbacks.
- main.cpp: composition only. It builds a ParkSession from startingPark and a ParkDialogs, hands each frame's request to useFileRequest, and keys the resets it already does on the generation. It gains no concern and loses the file flow and the starting world.

## Tasks

### Task 1: Describe the session in the app spec

Files:
- Modify: `src/app/SPEC.md:19-21`

Step 1: In "## Park files", replace the first sentence and the dialog callback sentence with FEATURE.md's exact text, add FEATURE.md's fileExists sentence after the sentence on withParkExtension, and insert FEATURE.md's new paragraph after the paragraph the first sentence opens, before the paragraph beginning "The Tools panel has New park".

### Task 2: Declare the request mailbox

Files:
- Create: `src/app/park_file_requests.h`
- Create: `src/app/park_file_requests.cpp`
- Modify: `src/app/tool_panel.h:4-12`

Step 1: Create src/app/park_file_requests.h:

```cpp
#ifndef TPJ_APP_PARK_FILE_REQUESTS_H
#define TPJ_APP_PARK_FILE_REQUESTS_H

#include <mutex>
#include <stdint.h>
#include <string>

namespace tpj {

// A park button the player pressed in the Tools panel.
enum class ParkAction : uint8_t { None, New, Open, Save };

// A park dialog to show, or none.
enum class ParkDialog : uint8_t { None, Open, Save };

// One request, taken by the main loop: New with no path, or Open or Save with the chosen path.
struct FileRequest {
  ParkAction Action = ParkAction::None;
  std::string Path;
};

// What the park buttons and the file dialogs ask of the main loop. A dialog's answer may come from
// another thread, so every call takes a lock.
class ParkFileRequests {
public:
  // A pressed button. With no dialog showing, New becomes the request, and Open or Save shows its
  // dialog, which it returns. While a dialog shows, or for None, it does nothing.
  ParkDialog press(ParkAction action);
  // A dialog's answer: no dialog shows from then on, and a chosen path, when there is one, becomes
  // the request with the dialog's action.
  void answer(ParkDialog dialog, const char *chosen);
  [[nodiscard]] bool dialogShowing() const;
  // The request, leaving none.
  FileRequest take();

private:
  mutable std::mutex Lock;
  bool DialogShowing = false;
  FileRequest Request;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/park_file_requests.cpp, including "app/park_file_requests.h", with stub definitions inside namespace tpj: press returns `ParkDialog::None`, answer and the others do nothing, dialogShowing returns false, and take returns `{}`. Mark unused parameters with `/*name*/` comments.

Step 3: In src/app/tool_panel.h, delete the ParkAction enum and its comment, and add `#include "app/park_file_requests.h"` before `#include "tools/tools.h"`.

### Task 3: Declare fileExists

Files:
- Modify: `src/app/park_file.h:27-28`
- Modify: `src/app/park_file.cpp`

Step 1: In src/app/park_file.h, after withParkExtension's declaration, add:

```cpp
// True when a file or directory exists at the path.
bool fileExists(const char *path);
```

Step 2: In src/app/park_file.cpp, add the stub `bool fileExists(const char * /*path*/) { return false; }` after withParkExtension.

### Task 4: Declare the session

Files:
- Create: `src/app/park_session.h`
- Create: `src/app/park_session.cpp`

Step 1: Create src/app/park_session.h:

```cpp
#ifndef TPJ_APP_PARK_SESSION_H
#define TPJ_APP_PARK_SESSION_H

#include "app/park_file.h"
#include "app/park_file_requests.h"
#include "sim/command_queue.h"
#include "sim/world.h"

#include <stdint.h>
#include <string>

namespace tpj {

// What using a park file request needs from the platform: asking before replacing a file, and
// showing why an open or a save failed.
class ParkFileEdge {
public:
  ParkFileEdge() = default;
  ParkFileEdge(const ParkFileEdge &) = delete;
  ParkFileEdge &operator=(const ParkFileEdge &) = delete;
  ParkFileEdge(ParkFileEdge &&) = delete;
  ParkFileEdge &operator=(ParkFileEdge &&) = delete;
  virtual ~ParkFileEdge() = default;

  // True when the player chooses to replace the file at the path.
  virtual bool confirmReplace(const std::string &path) = 0;
  // Shows a failed open's or save's message.
  virtual void reportError(const std::string &message) = 0;
};

// makeNewPark(1), resolved.
World resolvedNewPark();

// The park the app starts from: resolvedNewPark(), or the park file at parkPath when it is not
// null, stepped ticks cycles with no commands. No world, and openParkFile's message, when the file
// cannot be read or loaded.
OpenedPark startingPark(const char *parkPath, uint64_t ticks);

// The world the app holds and the commands waiting for its next cycle. It is the one place the
// world is replaced, and its generation changes exactly when the world is.
class ParkSession {
public:
  explicit ParkSession(World world);

  [[nodiscard]] const World &world() const { return Current; }
  [[nodiscard]] CommandQueue &commands() { return Commands; }
  // Changes exactly when the world is replaced, to a value it has not had.
  [[nodiscard]] uint64_t generation() const { return Generation; }

  // One cycle with the queued commands, which it empties.
  void step();
  // Acts on a request. Save writes the world as it is to withParkExtension of the path, first
  // asking the edge when that added the extension and a file exists there, and saving nothing
  // unless it confirms. New replaces the world with resolvedNewPark(), and Open with openParkFile's
  // world. A failed open or save leaves the world as it was and reports its message to the edge.
  // Replacing the world empties the queue.
  void useFileRequest(const FileRequest &request, ParkFileEdge &edge);

private:
  void replace(World world);

  World Current;
  CommandQueue Commands;
  uint64_t Generation = 0;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/park_session.cpp, including "app/park_session.h", "sim/park_schema.h", and `<utility>`, with stubs inside namespace tpj: resolvedNewPark returns `makeNewPark(1)`; startingPark returns `{resolvedNewPark(), {}}`; the constructor is `ParkSession::ParkSession(World world) : Current(std::move(world)) {}`; step, useFileRequest, and replace do nothing. Mark unused parameters with `/*name*/` comments.

### Task 5: Make tpj_app_core

Files:
- Modify: `src/app/CMakeLists.txt:1-17`
- Modify: `tests/app/CMakeLists.txt:1-7`

Step 1: In src/app/CMakeLists.txt, replace the tpj_park_files library and its comment with:

```cmake
# The app's components that need no window, so the app's tests call them through their headers.
add_library(tpj_app_core STATIC
    park_file.cpp
    park_file_requests.cpp
    park_session.cpp)
target_include_directories(tpj_app_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_app_core PUBLIC tpj_sim PRIVATE SDL3::SDL3)
tpj_configure_target(tpj_app_core)
```

In tpj_app's target_link_libraries, replace tpj_park_files with tpj_app_core.

Step 2: In tests/app/CMakeLists.txt, replace tpj_park_files with tpj_app_core in the target_link_libraries line, and in the comment replace "They link the sim and tpj_park_files, to read the errors a park file's load gives and to call the park file functions." with "They link the sim and tpj_app_core, to read the errors a park file's load gives and to call the app's window-free components."

Step 3: Configure and build.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests`
Expected: the build succeeds with no warnings.

### Task 6: Test pass

Step 1: Dispatch the test-writer agent with FEATURE.md, src/app/SPEC.md, and the public headers src/app/park_file_requests.h, src/app/park_session.h, and src/app/park_file.h. It creates tests/app/park_file_requests_test.cpp and tests/app/park_session_test.cpp, adds them to tpj_app_tests in tests/app/CMakeLists.txt, and adds fileExists's test to tests/app/park_file_test.cpp.

Step 2: Build and run them.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#park_file_requests_test],[#park_session_test],[#park_file_test]"`
Expected: the build succeeds. The tests of the mailbox's presses, answers, and takes fail, as do the session's tests of the generation, step, useFileRequest, and startingPark's stepping and file cases, against the stubs. fileExists's test fails against its stub where something exists. The test that a constructed session holds its world with an empty queue passes, and park_file_test's existing tests pass.

### Task 7: Implement the mailbox

Files:
- Modify: `src/app/park_file_requests.cpp`

Step 1: Replace the stubs with:

```cpp
ParkDialog ParkFileRequests::press(ParkAction action) {
  const std::scoped_lock lock(Lock);
  if (DialogShowing || action == ParkAction::None) {
    return ParkDialog::None;
  }
  if (action == ParkAction::New) {
    Request = FileRequest{ParkAction::New, {}};
    return ParkDialog::None;
  }
  DialogShowing = true;
  return action == ParkAction::Open ? ParkDialog::Open : ParkDialog::Save;
}

void ParkFileRequests::answer(ParkDialog dialog, const char *chosen) {
  const std::scoped_lock lock(Lock);
  DialogShowing = false;
  if (chosen != nullptr && dialog != ParkDialog::None) {
    Request = FileRequest{dialog == ParkDialog::Open ? ParkAction::Open : ParkAction::Save, chosen};
  }
}

bool ParkFileRequests::dialogShowing() const {
  const std::scoped_lock lock(Lock);
  return DialogShowing;
}

FileRequest ParkFileRequests::take() {
  const std::scoped_lock lock(Lock);
  FileRequest request = std::move(Request);
  Request = FileRequest{};
  return request;
}
```

Add `#include <utility>` to its includes.

Step 2: Run the mailbox's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#park_file_requests_test]"`
Expected: every test passes.

### Task 8: Implement fileExists and the session

Files:
- Modify: `src/app/park_file.cpp`
- Modify: `src/app/park_session.cpp`

Step 1: In src/app/park_file.cpp, replace fileExists's stub with:

```cpp
bool fileExists(const char *path) { return SDL_GetPathInfo(path, nullptr); }
```

Step 2: In src/app/park_session.cpp, replace the stubs with:

```cpp
World resolvedNewPark() {
  World world = makeNewPark(1);
  resolveWorld(world);
  return world;
}

OpenedPark startingPark(const char *parkPath, uint64_t ticks) {
  OpenedPark opened;
  if (parkPath == nullptr) {
    opened.Park = resolvedNewPark();
  } else {
    opened = openParkFile(parkPath);
  }
  if (opened.Park) {
    for (uint64_t tick = 0; tick < ticks; ++tick) {
      stepWorld(*opened.Park);
    }
  }
  return opened;
}

ParkSession::ParkSession(World world) : Current(std::move(world)) {}

void ParkSession::step() { stepWorld(Current, Commands); }

void ParkSession::useFileRequest(const FileRequest &request, ParkFileEdge &edge) {
  if (request.Action == ParkAction::Save) {
    // The dialog asked only about the name as typed, so a save whose extension was added asks
    // before replacing a file.
    const std::string path = withParkExtension(request.Path);
    if (path != request.Path && fileExists(path.c_str()) && !edge.confirmReplace(path)) {
      return;
    }
    const std::string error = saveParkFile(Current, path.c_str());
    if (!error.empty()) {
      edge.reportError(error);
    }
  } else if (request.Action == ParkAction::New) {
    replace(resolvedNewPark());
  } else if (request.Action == ParkAction::Open) {
    OpenedPark opened = openParkFile(request.Path.c_str());
    if (!opened.Park) {
      edge.reportError(opened.Error);
      return;
    }
    replace(std::move(*opened.Park));
  }
}

void ParkSession::replace(World world) {
  Current = std::move(world);
  Commands.clear();
  ++Generation;
}
```

Step 3: Run the session's and the park file's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#park_session_test],[#park_file_test]"`
Expected: every test passes.

### Task 9: Move the dialogs to their own component

Files:
- Create: `src/app/park_dialogs.h`
- Create: `src/app/park_dialogs.cpp`
- Modify: `src/app/CMakeLists.txt:9-16`

Step 1: Create src/app/park_dialogs.h:

```cpp
#ifndef TPJ_APP_PARK_DIALOGS_H
#define TPJ_APP_PARK_DIALOGS_H

#include "app/park_file_requests.h"
#include "app/park_session.h"

#include <SDL3/SDL.h>

#include <string>

namespace tpj {

// The park file flow's platform edge: SDL's open and save dialogs for the window, which answer the
// program's one ParkFileRequests, and the message boxes that ask before a replace and show errors.
class ParkDialogs final : public ParkFileEdge {
public:
  explicit ParkDialogs(SDL_Window *window);

  // Acts on a park button: New waits for the next frame, and Open and Save show their dialog,
  // filtered to .park files and starting in the parks folder beside the executable. Does nothing
  // while a dialog is showing.
  void press(ParkAction action);
  [[nodiscard]] bool dialogShowing() const;
  // The request the buttons and dialogs made, leaving none.
  FileRequest take();

  bool confirmReplace(const std::string &path) override;
  void reportError(const std::string &message) override;

private:
  SDL_Window *Window;
  // The program's one mailbox, which outlives every ParkDialogs.
  ParkFileRequests &Requests;
};

} // namespace tpj

#endif
```

Step 2: Create src/app/park_dialogs.cpp, including "app/park_dialogs.h" and `<string>`. Inside namespace tpj, an anonymous namespace holds, moved from src/app/main.cpp with their comments:
- PARK_FILTERS;
- parksFolder;
- the program's one ParkFileRequests, as `ParkFileRequests &requests() { static ParkFileRequests instance; return instance; }`, under main.cpp's comment "Lives for the whole program, since a dialog left open at quit may still call back.";
- a callback helper, replacing main.cpp's handOver:

```cpp
// A dialog's callback: logs a failed dialog, and answers the requests with the first chosen path,
// or none when the dialog was cancelled or failed.
void answer(ParkDialog dialog, const char *const *filelist) {
  if (filelist == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File dialog: %s", SDL_GetError());
  }
  requests().answer(dialog, filelist != nullptr ? filelist[0] : nullptr);
}

void SDLCALL onOpenChosen(void * /*userdata*/, const char *const *filelist, int /*filter*/) {
  answer(ParkDialog::Open, filelist);
}

void SDLCALL onSaveChosen(void * /*userdata*/, const char *const *filelist, int /*filter*/) {
  answer(ParkDialog::Save, filelist);
}
```

After the anonymous namespace, define the members:

```cpp
ParkDialogs::ParkDialogs(SDL_Window *window) : Window(window), Requests(requests()) {}

void ParkDialogs::press(ParkAction action) {
  // The callback may run before these return, so no lock is held while they run.
  const ParkDialog dialog = Requests.press(action);
  if (dialog == ParkDialog::Open) {
    SDL_ShowOpenFileDialog(onOpenChosen, nullptr, Window, PARK_FILTERS, 1, parksFolder(), false);
  } else if (dialog == ParkDialog::Save) {
    SDL_ShowSaveFileDialog(onSaveChosen, nullptr, Window, PARK_FILTERS, 1, parksFolder());
  }
}

bool ParkDialogs::dialogShowing() const { return Requests.dialogShowing(); }

FileRequest ParkDialogs::take() { return Requests.take(); }
```

confirmReplace is main.cpp's confirmReplace body with `Window` for its window parameter, and reportError is main.cpp's reportFileError body likewise.

Step 3: In src/app/CMakeLists.txt, add park_dialogs.cpp to tpj_app's sources, in sorted order after orbit_camera.cpp.

### Task 10: Compose the session in main.cpp

Files:
- Modify: `src/app/main.cpp`

Step 1: Delete from main.cpp, with their comments: resolvedNewPark, startingWorld, FileRequests, FileRequest, PARK_FILTERS, parksFolder, fileRequests, handOver, onOpenChosen, onSaveChosen, dialogShowing, startParkAction, takeFileRequest, reportFileError, confirmReplace, and useFileRequest.

Step 2: Replace `#include "app/park_file.h"` with `#include "app/park_dialogs.h"` and `#include "app/park_session.h"`, and remove `#include "sim/park_schema.h"` and `#include <mutex>`.

Step 3: Give drawPanels a `tpj::ParkDialogs &dialogs` parameter in place of `SDL_Window *window`. In it, pass `dialogs.dialogShowing()` to drawToolPanel, and replace `startParkAction(choice.Park, window);` with `dialogs.press(choice.Park);`. Change buildUi's signature to

```cpp
void buildUi(SDL_Window *window, tpj::ParkDialogs &dialogs, const tpj::World &world,
             const tpj::OrbitCamera &camera, ShownViews &shown, tpj::ToolState &tool,
             const tpj::Preview &preview, std::optional<tpj::InspectorSubject> &inspected)
```

keeping eight parameters, the limit .clang-tidy's readability-function-size sets: it takes dialogs and no longer takes the view. Make its first statement `const tpj::CameraView view = cameraView(camera);`, the view runLoop computes from the same camera, and pass dialogs to drawPanels in place of window.

Step 4: Change runLoop and runLoopLogged to take `tpj::ParkSession &session` in place of `tpj::World &world`. At the top of runLoop, after the existing locals, add `tpj::ParkDialogs dialogs(renderer.Window);` and `uint64_t generation = session.generation();`. Replace the block from `if (useFileRequest(renderer.Window, takeFileRequest(), world)) {` through its closing brace with:

```cpp
    session.useFileRequest(dialogs.take(), dialogs);
    // A replaced world resets what was built from, or held for, the old one.
    if (session.generation() != generation) {
      generation = session.generation();
      tpj::selectTool(tool, tool.Kind);
      drawn.reset();
      guestTick.reset();
      kept = {};
      inspected.reset();
    }
```

Delete the local `tpj::CommandQueue commands;`. Replace the tick loop's `tpj::stepWorld(world, commands);` with `session.step();`, pass `session.commands()` where useButtons took `commands`, and call buildUi as `buildUi(renderer.Window, dialogs, session.world(), camera, shown, tool, kept.Made, inspected);`. Replace every remaining `world` argument in runLoop with `session.world()`.

Step 5: In main, replace the startingWorld call and its check with:

```cpp
  tpj::OpenedPark start = tpj::startingPark(options.ParkPath, options.Ticks);
  if (!start.Park) {
    (void)fprintf(stderr, "%s\n", start.Error.c_str());
    return EXIT_FAILURE;
  }
```

Use `*start.Park` in the --hash branch. Before the renderer is created, add `tpj::ParkSession session(std::move(*start.Park));`, and pass `session` to runLoopLogged.

Step 6: Build the app and run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app tpj_app_tests && build/windows-debug/tpj_app_tests.exe`
Expected: the build succeeds with no warnings, and every test passes, the command line tests included.

### Task 11: Verify on both builds

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

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/park-session.bmp`
Expected: it exits with status 0, and build/park-session.bmp shows the park with its walkways, guests, the graph, and the food band, as before the change.

Step 4: Check the park buttons by hand. Run `build/windows-debug/ThemeParkJones.exe`, then:
- press Save park and save as build-check, with no extension;
- press Save park again with the same name, and choose Cancel at the replace prompt;
- press New park;
- press Open park and open parks/build-check.park.

Expected: the first save writes parks/build-check.park, the second asks to replace it, Cancel writes nothing, New park shows the new park, and Open shows the saved one. Delete parks/build-check.park afterward.

### Task 12: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Give the world one owner in a park session`.
