# Implementation Plan: Grouped App

## Goal

Move src/app's components and tests/app's tests into session/, input/, scene/, and ui/ by concern, and declare the four as layers inside app.

## Approach

The test pass updates the layer check's expected table. A script moves each component's header and source with git mv, renames the header's include guard for its new path, and rewrites every include of it under src and tests. The two CMakeLists.txt files then list the new paths, clang-format re-sorts the rewritten includes, and the tests move to the matching directories. cmake/layers.txt replaces its one app unit with three lines, and the layer check, the builds, and the existing tests confirm the move changed nothing.

## Placement

Decision 0027 places each behavior this feature adds:

- No behavior is added. Each component keeps its header, its library, and its owner. Only its directory changes: session/, input/, scene/, or ui/, by the concern it serves, or src/app's top for main.cpp, the Application, the platform owners, the options, and the frame clock.
- The order of the four concerns inside app: cmake/layers.txt, the one table of layers, as for the sim's submodules.

## Tasks

### Task 1: Describe the grouping in the app spec

Files:
- Modify: `src/app/SPEC.md`

Step 1: Add FEATURE.md's new paragraph after the line "The executable: owns the window, the main loop, and input, and connects the simulation to the renderer.", separated by a blank line, with its exact text.

Step 2: Give each moved header its directory wherever the spec names it.

Run:

```bash
cd /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones && python3 - <<'EOF'
import re
moves = {
    'session': ['park_session', 'park_file', 'park_file_requests', 'park_dialogs'],
    'input': ['input_map', 'cursor', 'orbit_camera', 'interaction', 'platform_input'],
    'scene': ['scene_sync', 'scene_uploads'],
    'ui': ['tooling_ui', 'graph_view'],
}
path = 'src/app/SPEC.md'
text = open(path, encoding='utf-8').read()
for directory, names in moves.items():
    for name in names:
        text = re.sub(r'(?<![\w/])' + name + r'\.h\b', directory + '/' + name + '.h', text)
open(path, 'w', encoding='utf-8').write(text)
EOF
grep -o "[a-z_/]*\.h\b" src/app/SPEC.md | sort -u
```

Expected: the list is application.h, frame_clock.h, input/cursor.h, input/input_map.h, input/interaction.h, input/orbit_camera.h, input/platform_input.h, options.h, platform.h, scene/scene_sync.h, scene/scene_uploads.h, session/park_dialogs.h, session/park_file.h, session/park_file_requests.h, session/park_session.h, ui/graph_view.h, and ui/tooling_ui.h.

### Task 2: Declare the layers inside app

Files:
- Modify: `cmake/layers.txt:16`

Step 1: Replace the table's last line, `app`, with:

```
app/session app/input
app/scene app/ui
app/*
```

### Task 3: Test pass

Dispatch the test-writer as implementing-features describes, with:
- Feature: plans/sound-architecture/composed-app/grouped-app/FEATURE.md
- Specs: src/app/SPEC.md, docs/conventions.md
- Public headers: cmake/layers.txt

Expected: tests/checks/layer_check_test.cmake's repository_layers case expects the new table, and the case "cmake/layers.txt declares the layers of the code, lowest first" passes. "layer check passes on the tree" passes only once Task 4 has moved the files.

### Task 4: Move the components

Files:
- Move: the 18 components' headers and sources under `src/app` that FEATURE.md's first criterion lists
- Modify: every file under `src/app` and `tests/app` that includes one of them
- Modify: `src/app/CMakeLists.txt:2-27`

Step 1: Move each component, rename its include guard, and rewrite its includes.

Run:

```bash
cd /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones
move() {
  dir=$1; shift
  mkdir -p src/app/$dir
  for name in "$@"; do
    git mv src/app/$name.h src/app/$dir/$name.h
    git mv src/app/$name.cpp src/app/$dir/$name.cpp
    upper=$(echo "$name" | tr a-z A-Z)
    updir=$(echo "$dir" | tr a-z A-Z)
    sed -i "s/\bTPJ_APP_${upper}_H\b/TPJ_APP_${updir}_${upper}_H/g" src/app/$dir/$name.h
    grep -rlF "\"app/$name.h\"" src tests | xargs -r sed -i "s|\"app/$name.h\"|\"app/$dir/$name.h\"|g"
  done
}
move session park_session park_file park_file_requests park_dialogs
move input input_map cursor orbit_camera interaction platform_input
move scene scene_sync scene_uploads
move ui tooling_ui debug_panel tool_panel inspector_window food_tooltip shop_context_tooltip graph_view
ls src/app
```

Expected: ls prints CMakeLists.txt, SPEC.md, application.cpp, application.h, frame_clock.cpp, frame_clock.h, input, main.cpp, options.cpp, options.h, platform.cpp, platform.h, sanitizer_support.cpp, scene, session, and ui.

Step 2: In src/app/CMakeLists.txt, tpj_app_core's sources become:

```cmake
add_library(tpj_app_core STATIC
    frame_clock.cpp
    input/cursor.cpp
    input/input_map.cpp
    input/interaction.cpp
    input/orbit_camera.cpp
    options.cpp
    scene/scene_sync.cpp
    session/park_file.cpp
    session/park_file_requests.cpp
    session/park_session.cpp)
```

and tpj_app's sources become:

```cmake
add_executable(tpj_app
    application.cpp
    input/platform_input.cpp
    main.cpp
    platform.cpp
    scene/scene_uploads.cpp
    session/park_dialogs.cpp
    ui/debug_panel.cpp
    ui/food_tooltip.cpp
    ui/graph_view.cpp
    ui/inspector_window.cpp
    ui/shop_context_tooltip.cpp
    ui/tool_panel.cpp
    ui/tooling_ui.cpp)
```

Step 3: Re-sort the rewritten includes.

Run: `find src/app tests/app -name '*.h' -o -name '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 4: Check that no include names a moved header by its old path, and build.

Run: `grep -rnE '"app/(park_session|park_file|park_file_requests|park_dialogs|input_map|cursor|orbit_camera|interaction|platform_input|scene_sync|scene_uploads|tooling_ui|debug_panel|tool_panel|inspector_window|food_tooltip|shop_context_tooltip|graph_view)\.h"' src tests; cmake.exe --build --preset windows-debug --target tpj_app_tests`
Expected: grep prints nothing, and the build succeeds with no warnings.

### Task 5: Move the tests

Files:
- Move: `tests/app/park_session_test.cpp`, `tests/app/park_file_test.cpp`, `tests/app/park_file_requests_test.cpp`, `tests/app/cursor_test.cpp`, `tests/app/input_map_test.cpp`, `tests/app/interaction_test.cpp`, `tests/app/orbit_camera_test.cpp`, `tests/app/scene_sync_test.cpp`
- Modify: `tests/app/CMakeLists.txt`

Step 1: Move them.

Run:

```bash
cd /mnt/c/Users/EvanUhhh/source/repos/ThemeParkJones/tests/app
mkdir -p session input scene
git mv park_session_test.cpp park_file_test.cpp park_file_requests_test.cpp session/
git mv cursor_test.cpp input_map_test.cpp interaction_test.cpp orbit_camera_test.cpp input/
git mv scene_sync_test.cpp scene/
ls
```

Expected: ls prints CMakeLists.txt, command_line_test.cpp, frame_clock_test.cpp, input, options_test.cpp, scene, and session.

Step 2: In tests/app/CMakeLists.txt, tpj_app_tests's sources become:

```cmake
add_executable(tpj_app_tests
    command_line_test.cpp
    frame_clock_test.cpp
    input/cursor_test.cpp
    input/input_map_test.cpp
    input/interaction_test.cpp
    input/orbit_camera_test.cpp
    options_test.cpp
    scene/scene_sync_test.cpp
    session/park_file_requests_test.cpp
    session/park_file_test.cpp
    session/park_session_test.cpp)
```

Step 3: Build and run the app's tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe | tail -2`
Expected: the build succeeds with no warnings, and every test passes.

Step 4: Check that the tests changed only in their place and their include paths.

Run: `git diff -M HEAD --stat -- tests/app ':!tests/app/CMakeLists.txt' && git diff -M HEAD -- tests/app ':!tests/app/CMakeLists.txt' | grep -E '^[-+][^-+]' | grep -v '#include "app/'`
Expected: the stat lists the eight moved tests and the remaining tests whose includes changed, and grep prints nothing, so every changed line is an include of an app header.

### Task 6: Verify on both builds

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes, "layer check passes on the tree" and "private header check passes on the tree" among them.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Step 2: Check the scene.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/grouped-app.bmp; echo $?`
Expected: it prints 0, and build/grouped-app.bmp shows the same scene as before, with the Debug panel's `Shop 7: stock 6, queue 6, on order 16, service rate`, `Guests 35, mean hunger 0.54`, and `Waiting 7, meals eaten 17`.

Step 3: Check the app by hand. Run `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park`, press Open park and open a park, then close the window.
Expected: the park opens and the app exits at once.

### Task 7: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Group the app's components into directories by concern`.
