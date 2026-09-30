# app

The executable: owns the window, the main loop, and input, and connects the simulation to the renderer.

## Main loop

The app owns the Dear ImGui context and its SDL3 platform backend, created before the renderer and destroyed after it. Each frame gathers input, advances the simulation by as many fixed ticks as the elapsed time covers, updates the camera, builds the tooling UI, and draws. Elapsed time per frame is clamped to 0.25 s so a stall does not trigger a burst of ticks.

## Park

The app starts from makeNewPark(1), resolved, or from the park file --park names, opened with openParkFile. Before the first frame it steps the world --ticks cycles with no commands. Whenever the world's intent, as parkEntrances, parkPaths, and parkBoxes give it, differs from the intent last drawn, it builds the park mesh and gives it to the renderer. The mesh holds the walkways of the world's networks, which derive from intent alone (sim/routes/SPEC.md) and are resolved in every world the app holds, and the starved marks of its shops, which change only when resolution publishes route distance (sim/operations/SPEC.md), so only after a change of intent or an opened park. So the mesh is rebuilt whenever they change, and the ghost, rebuilt with it, shows its candidate's walkways and starved marks. Each frame, after its ticks, when the world's tick differs from the tick the guests were last drawn at, or the world has been replaced since, it builds buildGuestMesh for the world and gives it to the renderer with setGuestMesh, so guests move as the world steps. When it builds the first mesh, and the mesh has vertices, it frames the camera on the mesh's bounds: the focus moves to their center, and the distance becomes their half diagonal divided by the sine of half the vertical field of view, so a sphere around them fits the view, within the camera's distance limits.

## Tools

The Tools panel selects the tool (tools/SPEC.md): Look, which is ToolKind None and the tool at start, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete. While the tool has drawn points, the panel says to click the last point again to finish, and a Cancel path button selects the same tool again, which drops them. Each frame, the left button's press and release reach the tool before the frame's ticks, so they act on the world and pointer the ghost on screen was built from: a press only when ImGui does not want the mouse, and a release always. Then, after the camera moves, the app gives the tool the ground position under the cursor, from groundAtCursor with the cursor's position in the window, or none while ImGui wants the mouse. An edit a release gives is queued with queueEdit, and the ticks step the world with that queue, so it applies at the next tick. Whenever the tool's edit or highlight differs from the one last drawn, or the park mesh was rebuilt, the app builds the ghost mesh, buildGhostMesh's for the edit followed by appendEntity of the highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh.

## Park files

The library tpj_park_files, in park_file.h, reads and writes park files apart from the window, so the app's tests call it. openParkFile reads a file whole, loads its text with makeParkSchema, and resolves the world. When the file cannot be read it gives no world and the message `Cannot read <path>: <reason>`, with SDL's reason, and when loadWorld refuses the text, `Cannot load <path>: <message>`, with the LoadError's message. saveParkFile writes saveWorld's text to a path, replacing any file there, and gives an empty message, or `Cannot save <path>: <reason>` when it cannot. withParkExtension gives a path unchanged when its file name, the part after its last / or \, holds a '.', and otherwise the path followed by .park.

The Tools panel has New park, Open park, and Save park buttons, which do nothing while a dialog is showing. Open park and Save park show SDL's open and save dialogs for the window, filtered to .park files and starting in the parks folder beside the executable. The build links that folder to the repository's parks directory when it configures, a junction on Windows, which needs no privilege, and a symbolic link elsewhere, so parks saved there live in the source tree and survive a clean build. A dialog's callback, which may run on another thread, only hands the first chosen path to the main loop under a lock. A cancelled dialog hands nothing, and a failed one logs SDL's error. New park and a chosen path are acted on at the start of the next frame, before its buttons reach the tool. Saving writes the world as it is to withParkExtension of the path, so an edit queued but not yet applied is not in the file. When that added the extension and a file already exists there, the app first asks whether to replace it, since the dialog asked only about the name as typed, and saves nothing unless the player chooses Replace. New park and a successful open replace the world with makeNewPark(1), resolved, or openParkFile's world. They empty the command queue, select the current tool again so it drops any hold and drawn points, and frame the camera on the new park's mesh as at start. A failed open or save leaves the world as it was, and shows its message in an error message box and the log.

## Camera

An orbit camera around a focus point on the ground.

- Right mouse drag orbits: horizontal changes yaw, vertical changes pitch.
- Middle mouse drag pans the focus across the ground, scaled by distance so the ground moves at a steady rate under the cursor.
- The mouse wheel zooms by a constant factor per step.
- W, A, S, and D move the focus relative to the view direction, at a speed proportional to distance.
- Q and E rotate the view around the focus.

Pitch stays between about 10 and 85 degrees, distance between 4 and 400 m, and the focus inside the park's square bounds. Mouse input ImGui wants (a cursor over a panel) and keyboard input ImGui wants (a focused text field) do not reach the camera.

## Tooling UI

A Debug panel shows the frame rate, the simulation tick, and the camera focus and distance, and a Graph checkbox, off at start unless --graph is given. While it is checked, each frame, after the panels are built, the app draws buildGraphOverlay for the world, the frame's CameraView, and ImGui's display size on ImGui's background draw list, which lies over the scene and behind every panel: each line in its color, GRAPH_LINE_THICKNESS thick, and then each node as a filled circle of its radius and color (render/SPEC.md). Below the checkbox, each frame, the panel lists each shop box of parkBoxes, in key order, as a line `Shop <key>: stock <Stock>, queue <Queue>, on order <OnOrder>, <limit>` from its shopRecord, each number in decimal and limit its limitingFactorName. Below them, after a separator, each frame, the panel shows `Guests <n>, mean hunger <m>`, with n the number of parkGuests in decimal and m the mean of their records' Hunger to two decimals, or `Guests 0` when there are none. Below that it shows `Waiting <w>, meals eaten <e>`, with w the number of parkGuests whose records show them waiting and e the meals units consumed with the cause eaten (sim/guests/SPEC.md), each in decimal. With no saved layout, the Debug panel starts at the window's top right corner, so its shop lines never run under the Tools panel at the left. ImGui keeps each panel where the player last left it in imgui.ini, in the working directory, and a saved position wins. ImGui docking is enabled.

## Command line

--frames N exits after N frames. --capture PATH writes the last frame to PATH as a BMP and exits after 3 frames unless --frames says otherwise. Together they make the app usable for automated visual checks.

--graph checks the Debug panel's Graph checkbox at start, so a capture shows the networks.

--park PATH starts from that park file. --ticks N, a decimal count, steps N cycles before the first frame, and N is 0 without it. --hash writes `tick <t> hash <h>` to standard output after them, with t the world's tick in decimal and h hashWorld's value as 16 lowercase hexadecimal digits, as tpj_scenarios writes them for the same file and ticks, and exits with status 0 without opening a window.

An unknown option, an option missing its value, a --ticks value that is not a decimal count, or --hash given with --frames, --capture, or --graph, whatever their values, prints the usage and exits with a nonzero status. A park file that cannot be read or loaded exits with a nonzero status after writing to standard error a message naming the file and, for a load error, LoadError's message.
