# app

The executable: owns the window, the main loop, and input, and connects the simulation to the renderer.

## Main loop

The app owns the Dear ImGui context and its SDL3 platform backend, created before the renderer and destroyed after it. Each frame gathers input, advances the simulation by as many fixed ticks as the elapsed time covers, updates the camera, builds the tooling UI, and draws. Elapsed time per frame is clamped to 0.25 s so a stall does not trigger a burst of ticks.

## Park

The app starts from makeNewPark(1), or from the park file --park names, loaded with makeParkSchema, and resolves it. Before the first frame it steps the world --ticks cycles with no commands. Whenever the world's intent, as parkEntrances, parkPaths, and parkBoxes give it, differs from the intent last drawn, it builds the park mesh and gives it to the renderer. When it builds the first mesh, and the mesh has vertices, it frames the camera on the mesh's bounds: the focus moves to their center, and the distance becomes their half diagonal divided by the sine of half the vertical field of view, so a sphere around them fits the view, within the camera's distance limits.

## Tools

The Tools panel selects the tool (tools/SPEC.md): Look, which is ToolKind None and the tool at start, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete. While the tool has drawn points, the panel says to click the last point again to finish, and a Cancel path button selects the same tool again, which drops them. Each frame, the left button's press and release reach the tool before the frame's ticks, so they act on the world and pointer the ghost on screen was built from: a press only when ImGui does not want the mouse, and a release always. Then, after the camera moves, the app gives the tool the ground position under the cursor, from groundAtCursor with the cursor's position in the window, or none while ImGui wants the mouse. An edit a release gives is queued with queueEdit, and the ticks step the world with that queue, so it applies at the next tick. Whenever the tool's edit or highlight differs from the one last drawn, or the park mesh was rebuilt, the app builds the ghost mesh, buildGhostMesh's for the edit followed by appendEntity of the highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh.

## Camera

An orbit camera around a focus point on the ground.

- Right mouse drag orbits: horizontal changes yaw, vertical changes pitch.
- Middle mouse drag pans the focus across the ground, scaled by distance so the ground moves at a steady rate under the cursor.
- The mouse wheel zooms by a constant factor per step.
- W, A, S, and D move the focus relative to the view direction, at a speed proportional to distance.
- Q and E rotate the view around the focus.

Pitch stays between about 10 and 85 degrees, distance between 4 and 400 m, and the focus inside the park's square bounds. Mouse input ImGui wants (a cursor over a panel) and keyboard input ImGui wants (a focused text field) do not reach the camera.

## Tooling UI

A Debug panel shows the frame rate, the simulation tick, and the camera focus and distance. ImGui docking is enabled.

## Command line

--frames N exits after N frames. --capture PATH writes the last frame to PATH as a BMP and exits after 3 frames unless --frames says otherwise. Together they make the app usable for automated visual checks.

--park PATH starts from that park file. --ticks N, a decimal count, steps N cycles before the first frame, and N is 0 without it. --hash writes `tick <t> hash <h>` to standard output after them, with t the world's tick in decimal and h hashWorld's value as 16 lowercase hexadecimal digits, as tpj_scenarios writes them for the same file and ticks, and exits with status 0 without opening a window.

An unknown option, an option missing its value, a --ticks value that is not a decimal count, or --hash given with --frames or --capture, whatever their values, prints the usage and exits with a nonzero status. A park file that cannot be read or loaded exits with a nonzero status after writing to standard error a message naming the file and, for a load error, LoadError's message.
