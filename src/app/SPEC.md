# app

The executable the player runs. It owns the window, the main loop, and input. It connects the simulation to the renderer and the tooling UI. It reaches the simulation only through its public headers (principle 6). It is the only place that composes platform resources, so the simulation runs without it (principle 10). Decision 0027 and cmake/layers.txt place and layer its components.

## Boundary

The app reads the world only through:

- the simulation's public queries
- the tools (tools/SPEC.md)
- the legible module's summaries, previews, and inspectors (legible/SPEC.md)
- the views (views/SPEC.md)
- the renderer's mesh builders (render/SPEC.md)

It changes the world in only three ways. It queues edits for the next cycle, it applies queued edits with commitCommands while the game is paused, and it replaces the world when a park is opened or a new one started. The library tpj_app_core holds the components that need no window. The command line, below, is a contract with scripts/runtime-report.sh and with the captures used to check rendering.

## Main loop

The simulation advances in fixed ticks. The player runs the park paused or at 1x, 2x, or 4x. Each frame steps as many ticks as its elapsed time times the speed covers, at most MAX_FRAME_TICKS times the speed. While paused, a frame steps no tick and no time is owed to the park, so unpausing steps no burst. A frame stops stepping once its ticks have taken MAX_STEP_SECONDS, always stepping at least one, and drops the rest, so a park whose ticks are slow at a high speed runs slower than the speed at a steady frame rate. The speed changes only how many ticks a frame steps, so the world at a given tick is the same at any speed. If the app stalls, it does not step a burst of ticks afterward. If a park's ticks cost more than real time, the park runs slower than real time at a steady frame rate. It does not fall further behind each frame.

A frame gathers input, hands the pointer's buttons to the tool, steps the simulation, moves the camera, builds the tooling UI, and draws. What the player sees on screen is always built from the world the next click acts on.

If the main loop throws, for example when one of the simulation's world invariant checks fails, the app logs `Main loop: <what>`, releases every resource it acquired, and exits with a nonzero status.

## The scene

The park's walkways and shop marks are rebuilt whenever an edit, an undo, or a redo applies, or a park is opened. The guests are rebuilt whenever the world ticks or an edit, an undo, or a redo applies, as one applied while paused may move them. Each seated guest keeps its seat from one rebuild to the next, and a click picks guests where they are drawn. Once its lists have grown, keeping the scene up to date frees no memory it allocated, whether or not the world ticked. That covers finding whether intent changed, the seated guests, and the guest mesh. The allocation check holds a stretch of full.park's ticks to it. The scene always shows the current world. When a world first has something to show, the camera frames the whole park. The scene is lit by the light at the world's time of day (render/SPEC.md, The light), unless the Light panel previews another time.

## Tools and the ghost

The Tools panel selects the tool. The tools are Look, Guest path, Backstage path, Place shop, Place depot, Place bench, Place toilet, Furnish, Move box, Edit path, Remove point, and Delete (tools/SPEC.md). Look is the tool at start. While a path is being drawn, the panel says to click the last point again to finish, and Cancel path drops the drawn points. Clicks over a panel never reach the tool. When the tool commits an edit, the edit applies at the next tick, or at once while paused. Once its lists have grown, a frame's tool work frees no memory it allocated while the pointer rests (tools/SPEC.md). That includes putting the tool's highlight, path marks, and handles in the frame's look, and building the ghost's marks again when the look changes, as when the pointer moves onto another box, point, handle, or side. The allocation check holds every tool on full.park to it, resting on paths, paving, boxes, and handles, and holding a drawn path, a dragged shop, and a dragged point.

The Tools panel has Undo and Redo buttons, each disabled while there is nothing to undo or redo. Ctrl+Z undoes, and Ctrl+Y or Ctrl+Shift+Z redoes, unless a text field has the keyboard. While a path is being drawn or the button holds an edit, undo and redo drop it, as Cancel path does, and change nothing else. Otherwise they apply at the next tick, with no ghost (decision 0029), in a tick of their own, so an edit committed while one waits applies at the tick after. While paused, they apply at the next frame instead, alone. While an edit or another undo or redo waits for its tick, undo and redo do nothing.

The ghost shows what the tool's edit would do. It shows the candidate park's walkways and starved marks, the box or path point the tool marks, and the Inspector's subject, highlighted. While Edit path or Remove point is chosen and nothing is held, it shows every path point's marker in POINT_MARK_COLOR. While Edit path is chosen and nothing is held, it also shows every corner's radius handle in HANDLE_COLOR and every edge handle in EDGE_HANDLE_COLOR and every taper handle in TAPER_HANDLE_COLOR, and highlights the handle or the segment's sides under the pointer. Before a path tool's first click, it shows where that click would start the path, and marks it when the click would join a point or segment. While a click would finish the drawn path, it marks the last stop with a finish mark. It comes from a preview of the edit. The preview is made again whenever the park's intent changes or the edit changes, and, while an overlay is shown and the world accepts the edit, whenever the world ticks. While an overlay is shown, it is shaded again whenever the world ticks. The ghost, the overlay, and its tooltip therefore agree, the overlay updates every tick, and an edit applied while paused shows at once. While the pointer rests with no overlay shown, the preview isn't made again, since ticks don't change what the edit would derive. While the Inspector's subject is a guest, the ghost is built again whenever the guests are, so the guest's highlight follows it as it walks, sits, and stands up. Once their lists have grown, keeping the preview, the ghost, and the overlay up to date frees no memory they allocated while the edit and the tool's look stay the same, as they do while the pointer rests. The exception is an overlay shown while the world accepts the edit, where the preview is made again at each tick. The allocation check holds a stretch of full.park's ticks to it, with edits held still and with each need's overlay shown.

While the tool snaps, the guides of the snap in effect draw over the scene and behind the panels, each kind in its own color: a ring on a point, a line from a door, a solid line along a segment, a dashed ray from an anchor that runs on past the stop, and ticks at whole meters. The length of each segment measuredLengths gives shows at its middle, in meters to two decimals. A frame gives the tool the meters of ground a screen pixel spans at the pointer, averaged across and up the screen. Holding Ctrl turns snapping off while it is held, unless a text field has the keyboard.

While the tool is Look, clicking a guest, a shop, a bench, or a toilet block opens the Inspector on it. If the click hits nothing, or an entity with nothing to inspect such as a depot or the entrance, the Inspector stays as it was.

While Furnish is chosen, the Tools panel shows Kind, as radio buttons None, Benches, and Tables, Spacing, as a slider in meters, and Sides, as radio buttons One side and Both sides. They set what the tool lays. The choice starts at Benches on both sides and lasts while the app runs.

When the Inspector shows a shop, it has a Sells row of radio buttons, Food, Drink, and Both, with the shop's stock setting marked, and an Apply button. Choosing another setting marks it, and while the tool gives no edit of its own, the choice is the edit the ghost and the preview show, a SetShopStock. Apply commits it, and it applies at the next tick, or at once while paused. The choice stays marked until the shop has that setting, and Apply is disabled while the chosen setting is the shop's. Picking another subject, closing the Inspector, or replacing the world drops the choice.

## Park files

The Tools panel has New park, Open park, and Save park. They do nothing while a dialog is open. Open and Save show the system's dialogs, filtered to .park files. The dialogs start in the parks folder beside the executable. The build links that folder to the repository's parks directory, so saved parks are not lost in a clean build.

Save writes the world as it is. If an edit is queued but not yet applied, it is not in the file. If the name has no extension, .park is added. If the name with .park added matches an existing file, the app asks before replacing it.

New park and a successful open replace the world. They also drop any queued edits and the tool's drawn points, empty the undo and redo lists, close the Inspector, and frame the camera on the new park. If an open or save fails, the world stays as it was, and the app shows the reason in a message box and the log.

## Camera

The camera orbits a focus on the ground. Its controls are:

- Right drag orbits.
- Middle drag pans, keeping the ground under the cursor.
- The wheel zooms.
- W, A, S, and D move the focus relative to the view, at a speed that grows with distance.
- Q and E rotate around the focus.

Pitch, distance, and the focus stay within limits that keep the park in view. Mouse input never reaches the camera while the mouse is over a panel. Keys never reach the camera while they are typed into a text field.

## Tooling UI

The Debug panel shows the frame rate, the tick, the number of guests in the park, the camera, and the Graph checkbox, and the Overlay row of radio buttons Off, Food, Drink, Rest, and Toilet. With no saved layout it starts at the window's top right, clear of the Tools panel. Panel positions persist in imgui.ini in the working directory.

The Time panel shows the park's day and its time of day on a 24-hour clock in hours and minutes, such as `Day 1  09:00` (sim/SPEC.md, The clock). It follows every tick. Below the clock, radio buttons Pause, 1x, 2x, and 4x show the speed and choose it. Space pauses, and pressed again runs the park at the speed it ran at before. 1, 2, and 3 choose 1x, 2x, and 4x and unpause. The keys do nothing while a text field has the keyboard. The app starts at 1x, and keeps its speed when a park is opened or a new one started. With no saved layout the panel starts at the window's top center.

The Hours panel shows the park's opening hours on a 24-hour clock, as Opens and Closes, each with - and + buttons, and an Apply button below them. Each button moves its time to the quarter hour before or after. Opens runs from 00:00 to 23:45, and Closes from 00:15 to 24:00, which is midnight. A + on Opens that brings it to or past Closes moves Closes to a quarter hour after it, and a - on Closes that brings it to or before Opens moves Opens to a quarter hour before it. The panel shows the player's choice once they press a button, and the park's hours otherwise. While the tool and the Inspector give no edit of their own, a choice that differs from the park's hours is the edit the ghost and the preview show, a SetOpeningHours. Apply commits it, and it applies at the next tick, or at once while paused. The choice stays until the park has those hours, and Apply is disabled while the choice is the park's hours. Replacing the world drops the choice. Below Apply the panel draws the arrival curve from 00:00 to 24:00 for the hours it shows, the guests per game hour each entrance admits, at a fixed height scale (legible/SPEC.md, Arrivals). A vertical line marks the park's time of day. The curve follows the player's choice at once, before Apply, whatever edit the ghost shows. With no saved layout the panel starts at the window's top center, below the Time panel.

The Departures panel shows how many guests have left the park since it last opened, one row for each reason, Hunger, Thirst, Tiredness, Toilet, and Closing, and their Total (sim/guests/SPEC.md). It follows every tick. With no saved layout it starts at the window's top right, below the Debug panel.

The Light panel has a Preview checkbox and an Hour slider that shows a time of day on a 24-hour clock in hours and minutes. While Preview is unchecked, the slider shows the park's time of day and follows it. Dragging the slider checks Preview. While Preview is checked, the scene is lit by the light at the slider's time, and the slider stays where it was put. Unchecking Preview lights the scene by the park's time of day again. The preview changes only the light. The clock, the tick, and the world are the same with it as without it. Opening a park or starting a new one keeps the preview. With no saved layout the panel starts at the window's bottom right.

The Graph checkbox draws the networks' lines and nodes over the scene and behind the panels. The Overlay row shows one need's availability on the guest paving of the previewed park, or none while Off is chosen (legible/SPEC.md, views/SPEC.md). Food shows how well shops that sell food serve each spot, Drink shops that sell drink, Rest benches, and Toilet toilet blocks. The overlay's shape is built again only when the previewed park's path points or segments change. While an overlay is shown, hovering shaded paving shows its need's value at the overlay place of the point under the cursor at the overlay's height (render/SPEC.md) and, for each facility that contributes, its relief, route distance, wait and effective time in game minutes, and term. Hovering anywhere else shows no tooltip. If no supplied facility of the need is reachable, the tooltip says so. While the preview places a shop, a tooltip shows the hungry footfall at its door and its supply route, or says that it has no supply route.

The Inspector shows its subject's title and rows. For a guest, it also shows the guest's choices, one row for each need and one each for carrying on and heading home, with their chances, and heading home's terms (legible/SPEC.md, Inspectors). It says when the subject has left the park. It updates every tick, stays open when the tool changes, and closes with its close button. With no saved layout it starts at the window's bottom left.

## Command line

- --frames N exits after N frames.
- --capture PATH writes the last frame to PATH as a BMP and exits. It exits after 3 frames unless --frames says otherwise.
- --frame-times, with a positive --frames, writes one line at exit. The line is `frame-times`, then the duration in nanoseconds of each frame from the second to the next to last, each after a single space. With N frames, the line holds N - 2 durations, and neither loading nor the first frame's one-off work is among them. scripts/runtime-report.sh times the app with it (bench/SPEC.md).

With --capture or --frame-times, the window is hidden. It never shows or takes focus, and every frame is still drawn at the window's size, so captures and timings run while the player uses the computer. A hidden window's frames don't wait for the screen, so --frame-times then gives what each frame costs the app, not the display's refresh.
- --graph checks the Graph checkbox at start, and --overlay food, drink, rest, or toilet chooses that need's overlay at start, so a capture shows them.
- --light-hour H starts the app with the Light panel's Preview checked and its slider at H:00, so a capture shows that hour's light on any world. H is a whole hour from 0 to 23.
- --park PATH starts from that park file instead of a new park.
- --speed N starts the app paused when N is 0, and at 1x, 2x, or 4x when N is 1, 2, or 4.
- --ticks N steps N cycles before the first frame.
- --hash writes `tick <t> hash <h>` after the ticks. The tick and hash match the last line tpj_scenarios writes for the same file and tick count. It then exits with status 0 without opening a window.

The app prints the usage and exits with a nonzero status for any of these:

- an unknown option
- a missing or malformed value
- a --speed value other than 0, 1, 2, or 4
- a --light-hour value that is not a whole hour from 0 to 23
- --frame-times without a positive --frames
- --hash with any option that needs a window

If a park file cannot be read or loaded, the app exits with a nonzero status and a message naming the file.
