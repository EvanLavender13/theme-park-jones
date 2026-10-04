# app

The executable the player runs. It owns the window, the main loop, and input. It connects the simulation to the renderer and the tooling UI. It reaches the simulation only through its public headers (principle 6). It is the only place that composes platform resources, so the simulation runs without it (principle 10). Decision 0027 and cmake/layers.txt place and layer its components.

## Boundary

The app reads the world only through:

- the simulation's public queries
- the tools (tools/SPEC.md)
- the legible module's summaries, previews, and inspectors (legible/SPEC.md)
- the views (views/SPEC.md)
- the renderer's mesh builders (render/SPEC.md)

It changes the world in only two ways. It queues edits for the next cycle, and it replaces the world when a park is opened or a new one started. The library tpj_app_core holds the components that need no window. The command line, below, is a contract with scripts/runtime-report.sh and with the captures used to check rendering.

## Main loop

The simulation advances in fixed ticks. Each frame steps as many ticks as the elapsed time covers, at most MAX_FRAME_TICKS. A stall does not set off a burst of ticks. If a park's ticks cost more than real time, the park runs slower than real time at a steady frame rate. It does not fall further behind each frame.

A frame gathers input, hands the pointer's buttons to the tool, steps the simulation, moves the camera, builds the tooling UI, and draws. What the player sees on screen is always built from the world the next click acts on.

If the main loop throws, for example when one of the simulation's world invariant checks fails, the app logs `Main loop: <what>`, releases every resource it acquired, and exits with a nonzero status.

## The scene

The park's walkways and shop marks are rebuilt whenever the park's intent changes or a park is opened. The guests are rebuilt whenever the world ticks. So the scene always shows the current world. When a world first has something to show, the camera frames the whole park.

## Tools and the ghost

The Tools panel selects the tool. The tools are Look, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete (tools/SPEC.md). Look is the tool at start. While a path is being drawn, the panel says to click the last point again to finish, and Cancel path drops the drawn points. Clicks over a panel never reach the tool. An edit the tool commits applies at the next tick.

The ghost shows what the tool's edit would do. It shows the candidate park's walkways and starved marks, the box the tool marks, and the Inspector's subject, highlighted. It comes from a preview of the edit. The preview is made again whenever the world ticks or the edit changes, so the ghost, the food overlay, and the food tooltip agree and follow every tick.

While the tool is Look, clicking a guest or a shop opens the Inspector on it. Clicking nothing, or an entity with nothing to inspect such as a depot or the entrance, leaves the Inspector as it was.

## Park files

The Tools panel has New park, Open park, and Save park. They do nothing while a dialog is open. Open and Save show the system's dialogs, filtered to .park files. The dialogs start in the parks folder beside the executable. The build links that folder to the repository's parks directory, so saved parks survive a clean build.

Save writes the world as it is, so an edit that is queued but not yet applied is not in the file. A name without an extension gets .park. If the name with .park added matches an existing file, the app asks before replacing it.

New park and a successful open replace the world. They also drop any queued edits and the tool's drawn points, close the Inspector, and frame the camera on the new park. A failed open or save leaves the world as it was and shows the reason in a message box and the log.

## Camera

The camera orbits a focus on the ground. Its controls are:

- Right drag orbits.
- Middle drag pans, keeping the ground under the cursor.
- The wheel zooms.
- W, A, S, and D move the focus relative to the view, at a speed that grows with distance.
- Q and E rotate around the focus.

Pitch, distance, and the focus stay within limits that keep the park in view. Mouse input over a panel and keys typed into a text field never reach the camera.

## Tooling UI

The Debug panel shows the frame rate, the tick, and the camera. It lists each shop's stock, queue, orders, and what limits it. It also lists the guests and their mean hunger, how many are waiting, and how many meals have been eaten (legible/SPEC.md, Park summary). With no saved layout it starts at the window's top right, clear of the Tools panel. Panel positions persist in imgui.ini in the working directory.

The Graph checkbox draws the networks' lines and nodes over the scene and behind the panels. The Food overlay checkbox shows food availability on the ground for the previewed park (views/SPEC.md). While the overlay is shown, hovering the ground shows the food value there and, for each shop that contributes, the terms that make up the value. If no supplied shop is reachable, the tooltip says so. While the preview places a shop, the tooltip also shows the hungry footfall at its door and its supply route, or says that it has no supply route.

The Inspector shows its subject's title and rows. For a guest, it also shows the options the guest is choosing between, with their chances (legible/SPEC.md, Inspectors). It says when the subject has left the park. It follows every tick, survives a change of tool, and closes with its close button. With no saved layout it starts at the window's bottom left.

## Command line

- --frames N exits after N frames.
- --capture PATH writes the last frame to PATH as a BMP and exits. It exits after 3 frames unless --frames says otherwise.
- --frame-times, with a positive --frames, writes one line at exit. The line is `frame-times`, then the duration in nanoseconds of each frame from the second to the next to last, each after a single space. So N frames give N - 2 durations, and neither loading nor the first frame's one-off work is among them. scripts/runtime-report.sh times the app with it (bench/SPEC.md).
- --graph and --overlay food check those boxes at start, so a capture shows them. food is the only overlay.
- --park PATH starts from that park file instead of a new park.
- --ticks N steps N cycles before the first frame.
- --hash writes `tick <t> hash <h>` after the ticks. The tick and hash match the last line tpj_scenarios writes for the same file and tick count. It then exits with status 0 without opening a window.

The app prints the usage and exits with a nonzero status for any of these:

- an unknown option
- a missing or malformed value
- --frame-times without a positive --frames
- --hash with any option that needs a window

If a park file cannot be read or loaded, the app exits with a nonzero status and a message naming the file.
