# Feature: Grouped App

## Summary

grouped-app moves src/app's components out of one flat directory into four by concern, and tests/app's tests with them:
- session/ holds the park session and the park file flow;
- input/ holds input mapping, the cursor, the orbit camera, and the interaction;
- scene/ holds the scene sync;
- ui/ holds the tooling UI and its panels.

Each directory holds a concern's window-free components beside its platform edge. main.cpp, the Application, the platform owners, the options, and the frame clock stay at the top. cmake/layers.txt declares the four directories as layers inside app, session and input below scene and ui, which sit below the top, so the layer check enforces an order the includes already follow. Nothing but paths changes.

## Acceptance criteria

- Each app component's header and source are in its concern's directory under src/app:
  - session/: park_session, park_file, park_file_requests, and park_dialogs;
  - input/: input_map, cursor, orbit_camera, interaction, and platform_input;
  - scene/: scene_sync and scene_uploads;
  - ui/: tooling_ui, debug_panel, tool_panel, inspector_window, food_tooltip, shop_context_tooltip, and graph_view.

  src/app's top level holds only main.cpp, application, platform, options, frame_clock, sanitizer_support.cpp, CMakeLists.txt, and SPEC.md.
- Each test in tests/app is in the directory under tests/app that matches its component's directory under src/app. command_line_test.cpp and the tests of components at src/app's top stay at tests/app's top.
- cmake/layers.txt declares app/session and app/input as one layer, app/scene and app/ui as the layer above it, and app/* above them, all above tools, render, legible, and scenarios. The layer check passes on the tree, and its test of the repository's table, in tests/checks/layer_check_test.cmake, expects this table.
- Each moved header's include guard is named for its new path under src, as docs/conventions.md says.
- The app's behavior is unchanged:
  - Every existing test passes. A test's source changes only in where its file lives and in the paths of the app headers it includes, apart from the layer check's expected table.
  - A --capture of a park with --graph and --overlay food shows the same scene and the same Debug panel lines.
  - The private header check passes.

The feature adds no behavior. Its test pass only updates the layer check's expected table for the new layers, and the layer check, the builds, and the existing tests check every other criterion.

## Medium

None. Files move, and nothing reads or writes the park differently.

## Principle checks

None. No simulation code changes, and the layers inside app only narrow what app files may include.

## Spec changes

src/app/SPEC.md, a new paragraph after its first line, "The executable: owns the window, the main loop, and input, and connects the simulation to the renderer.":

> Its components are grouped by concern. session/ holds the park session and the park file flow, input/ the input mapping, the cursor, the orbit camera, and the interaction, scene/ the scene sync, and ui/ the tooling UI and its panels, each beside its platform edge. main.cpp, the Application, the platform owners, the options, and the frame clock sit at the top. cmake/layers.txt layers them inside app: session and input, then scene and ui, then the top.

src/app/SPEC.md, wherever it names a moved header, the name gains its directory:
- session/: park_session.h, park_file.h, park_file_requests.h, park_dialogs.h;
- input/: input_map.h, cursor.h, orbit_camera.h, interaction.h, platform_input.h;
- scene/: scene_sync.h, scene_uploads.h;
- ui/: tooling_ui.h, graph_view.h.

## Files affected

- Modify: src/app/SPEC.md, cmake/layers.txt, src/app/CMakeLists.txt, tests/app/CMakeLists.txt
- Move: the 18 components' headers and sources the first criterion lists, from src/app to their directories, each moved header's include guard renamed
- Modify: every file under src/app and tests/app that includes a moved header, for the header's new path
- Modify (test pass): tests/checks/layer_check_test.cmake
- Move: tests/app/park_session_test.cpp, park_file_test.cpp, and park_file_requests_test.cpp to tests/app/session/; cursor_test.cpp, input_map_test.cpp, interaction_test.cpp, and orbit_camera_test.cpp to tests/app/input/; scene_sync_test.cpp to tests/app/scene/

## Dependencies

- composed-entry, after which every component this moves exists: met.
- cmake/check_layers.cmake's placing of a file in the most specific unit and of tests/app's files as the app's tests: met.

## Out of scope

- Splitting the tooling UI per panel, a milestone deepening candidate.
- Grouping any other module's directory.

## Open questions

None.
