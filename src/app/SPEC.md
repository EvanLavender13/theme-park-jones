# app

The executable the player runs. It owns the window, the main loop, and input, and connects the simulation to the renderer and the tooling UI. It keeps the simulation behind its public headers (principle 6) and is the only place that composes platform resources, so the simulation runs without it (principle 10). Its components are placed and layered under decision 0027 and cmake/layers.txt.

## Boundary

The app reads the world only through the simulation's public queries, the tools (tools/SPEC.md), the legible module's summaries, previews, and inspectors (legible/SPEC.md), the views (views/SPEC.md), and the renderer's mesh builders (render/SPEC.md). It changes the world only by queueing edits for the next cycle and by replacing it when a park is opened or a new one started. The library tpj_app_core holds its components that need no window. Its command line, below, is a contract with scripts/runtime-report.sh and with captures used to check rendering.

## Main loop

The simulation advances in fixed ticks: each frame steps as many ticks as the elapsed time covers, at most MAX_FRAME_TICKS. A stall does not set off a burst of ticks, and a park whose ticks cost more than real time runs slower than real time at a steady frame rate rather than falling further behind each frame.

A frame gathers input, hands the pointer's buttons to the tool, steps the simulation, moves the camera, builds the tooling UI, and draws. What the player sees on screen is always built from the world the next click acts on.

When the main loop throws, such as on a world invariant the simulation checks, the app logs `Main loop: <what>`, releases every resource it acquired, and exits with a nonzero status.

## The scene

The park's walkways and shop marks are rebuilt whenever the park's intent changes or a park is opened, and the guests whenever the world ticks, so the scene always shows the current world. When a world first has something to show, the camera frames the whole park.

## Tools and the ghost

The Tools panel selects the tool: Look, which is the tool at start, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete (tools/SPEC.md). While a path is being drawn, the panel says to click the last point again to finish, and Cancel path drops the drawn points. Clicks over a panel never reach the tool. An edit the tool commits applies at the next tick.

The ghost shows what the tool's edit would do: the candidate park's walkways and starved marks, the box the tool marks, and the Inspector's subject, highlighted. It comes from a preview of the edit that is made again whenever the world ticks or the edit changes, so the ghost, the food overlay, and the food tooltip agree and follow every tick.

While the tool is Look, a click on a guest or a shop opens the Inspector on it. A click on nothing, or on an entity with nothing to inspect, such as a depot or the entrance, leaves the Inspector as it was.

## Park files

The Tools panel has New park, Open park, and Save park, which do nothing while a dialog is open. Open and Save show the system's dialogs, filtered to .park files and starting in the parks folder beside the executable, which the build links to the repository's parks directory so saved parks survive a clean build.

Save writes the world as it is, so an edit queued but not yet applied is not in the file. A name without an extension gets .park, and when that names an existing file, the app asks before replacing it.

New park and a successful open replace the world, drop any queued edits and the tool's drawn points, close the Inspector, and frame the camera on the new park. A failed open or save leaves the world as it was and shows the reason in a message box and the log.

## Camera

An orbit camera around a focus on the ground. Right drag orbits, middle drag pans so the ground stays under the cursor, the wheel zooms, W, A, S, and D move the focus relative to the view at a speed that grows with distance, and Q and E rotate around the focus. Pitch, distance, and the focus stay within limits that keep the park in view. Mouse input over a panel and keys typed into a text field never reach the camera.

## Tooling UI

The Debug panel shows the frame rate, the tick, and the camera, and lists each shop's stock, queue, orders, and what limits it, the guests and their mean hunger, and how many are waiting and how many meals have been eaten (legible/SPEC.md, Park summary). With no saved layout it starts at the window's top right, clear of the Tools panel. Panel positions persist in imgui.ini in the working directory.

The Graph checkbox draws the networks' lines and nodes over the scene and behind the panels. The Food overlay checkbox shows food availability on the ground for the previewed park (views/SPEC.md). While the overlay is shown, hovering the ground shows the food value there and, for each shop that contributes, the terms that make it up, or that no supplied shop is reachable. While the preview places a shop, the tooltip also shows the hungry footfall at its door and its supply route, or that it has none.

The Inspector shows its subject's title and rows, and for a guest the options it is choosing between with their chances (legible/SPEC.md, Inspectors), and says when the subject has left the park. It follows every tick, survives a change of tool, and closes with its close button. With no saved layout it starts at the window's bottom left.

## Command line

- --frames N exits after N frames.
- --capture PATH writes the last frame to PATH as a BMP and exits, after 3 frames unless --frames says otherwise.
- --frame-times, with a positive --frames, writes once at exit the line `frame-times` followed by, each after a single space, the duration in nanoseconds of each frame from the second to the next to last, so N frames give N - 2 durations and neither loading nor the first frame's one-off work is among them. scripts/runtime-report.sh times the app with it (bench/SPEC.md).
- --graph and --overlay food check those boxes at start, so a capture shows them. food is the only overlay.
- --park PATH starts from that park file instead of a new park.
- --ticks N steps N cycles before the first frame.
- --hash writes `tick <t> hash <h>` after the ticks, as tpj_scenarios writes them for the same file and ticks, and exits with status 0 without opening a window.

An unknown option, a missing or malformed value, --frame-times without a positive --frames, or --hash with any option that needs a window prints the usage and exits with a nonzero status. A park file that cannot be read or loaded exits with a nonzero status and a message naming the file.
