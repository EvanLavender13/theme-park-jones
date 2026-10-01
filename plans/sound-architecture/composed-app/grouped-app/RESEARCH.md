# Research: grouped-app

## How is src/app grouped?

After composed-entry, src/app holds 22 components in 46 files in one directory. The headers' includes of one another fall into four concerns and a top:
- the session: park_session, park_file, park_file_requests, and park_dialogs, its platform edge, which include only each other;
- the input: input_map, cursor, orbit_camera, interaction, and platform_input, its platform edge. input_map includes orbit_camera for CameraInput, interaction includes input_map for PointerButtons, and platform_input includes cursor and input_map. None includes the session;
- the scene: scene_sync and scene_uploads, its platform edge, which includes orbit_camera;
- the UI: tooling_ui and the panels, tooltips, and graph view it calls, which include the input's interaction, orbit_camera, cursor, and platform_input, and the session's park_dialogs, but nothing of the scene;
- the top: main, the Application, the platform owners, the options, and the frame clock. Only the Application and main include the frame clock and the options.

So the four concerns layer with no include against the order: session and input as siblings, then scene and UI as siblings, then the top. Declaring them in cmake/layers.txt, as the sim's submodules are, lets the layer check keep it so, and keeps a sibling from reaching into another. The layer check already places a file in the most specific unit holding it, takes a module's highest layer for its tests, and counts every file under tests/app as the app's tests, so the table needs four lines and no change to the check. Evan chose this grouping, with the layers declared.

Each directory mixes a concern's window-free core and its platform edge, as sim's directories mix public and internal headers, so the libraries still split by file: tpj_app_core takes the cores, and tpj_app the edges, the panels, and the top.

Rejected: the same directories with app left one unit in the table — the order the includes already follow would go unchecked. Splitting by library into a window-free directory and a platform directory — each would still hold ten or more components, and a concern's core and its edge would sit apart.

Sources: src/app headers' includes; cmake/check_layers.cmake place and may_include; cmake/layers.txt, the sim's submodules.

## Do the tests move with their components?

docs/conventions.md says tests mirror src/, and tests/sim keeps its submodules' tests in matching directories. So each app test moves to the directory of the component it tests, and the tests of the top's components and the command line stay at tests/app's top. A moved test changes only in where its file lives and in the paths of the app headers it includes, which is the move itself and no change to what it checks.

Rejected: leaving tests/app flat — it would not mirror src/app, against the conventions.

Sources: docs/conventions.md, Tests; tests/sim's layout.
