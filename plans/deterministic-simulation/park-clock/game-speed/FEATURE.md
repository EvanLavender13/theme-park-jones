# Feature: Game Speed

## Summary

The player can pause the park or run it at 1x, 2x, or 4x. The Time panel gains a radio button for each, and Space pauses and unpauses, while 1, 2, and 3 choose 1x, 2x, and 4x. A faster speed steps more ticks in each frame and never changes what a tick does, so the park at a given tick is the same at any speed (principle 10). While paused, no tick steps, but the camera, the tools, the ghost, and the panels keep working. Edits, undo, and redo apply at once through a new sim call, commitCommands, which applies and resolves queued commands without a tick, as makeCandidate does to a copy. The world after a paused edit is the world its preview showed (principle 8). The ghost, the overlay, and the guests are drawn again after it. `--speed N` starts the app at a speed, so captures and timings can choose one.

## Acceptance criteria

1. The Time panel has radio buttons Pause, 1x, 2x, and 4x below the clock, with the current one marked. Space pauses, and pressed again runs the park at the speed it ran at before. 1, 2, and 3 choose 1x, 2x, and 4x, and unpause. The keys do nothing while a text field has the keyboard. The app starts at 1x unless --speed says otherwise, and keeps its speed when the player opens a park or starts a new one. At 2x the clock advances two game minutes each real second, and at 4x four. (play; and captures of main-street.park after `--frames 600` at `--speed 1` and at `--speed 4`, whose clocks show about four times as many minutes gone at 4x)
2. A frame steps the ticks that its real time times the speed covers, at most MAX_FRAME_TICKS times the speed. While paused, no real time is owed to the park, so unpausing steps no burst of ticks. The speed reaches nothing but how many ticks each frame steps, so the world at a given tick is the same at any speed. On tests/parks/stress/full.park at 4x, the app keeps a steady frame rate, and runs the park as fast as its ticks allow. (review; `--speed 4` and `--speed 1` with `--frames 600 --frame-times --capture` on full.park with windows-release, reporting each run's median and slowest 5% of frame times and the clock its capture shows; and play)
3. While paused, no tick steps: the clock stops, and guests and shipments stand still. The camera, the tools, the ghost, the food overlay, the Inspector, and the panels keep working. Unpausing goes on from the same tick. (play)
4. An edit, an undo, or a redo made while paused applies at once, without a tick. The world after it equals the candidate its preview showed (principle 8). The ghost, the food overlay, and the tooltips then show previews of the edited park, and guests the edit moved are drawn where they now stand. (the listed test, and play: pause warm-main-street.park, narrow the main street under its guests with Edit path, and see the guests drawn on the narrowed paving at once; then undo and redo)
5. `--speed N` starts the app at speed N, where 0 means paused at 1x. Any N other than 0, 1, 2, or 4, and --speed with --hash, print the usage and exit with a nonzero status. (`ThemeParkJones.exe --speed 3; echo $?` and `ThemeParkJones.exe --speed 1 --hash; echo $?`)
6. The simulation is unchanged: saves, hashes, and every park's ticks are as they were. (`tpj_scenarios --compare` on windows-debug output from before and after the feature)
7. src/sim/SPEC.md describes commitCommands. src/app/SPEC.md describes the speeds, the Time panel's buttons, the keys, --speed, and edits, undo, and redo applying at once while paused, and the scene and preview following them. src/legible/SPEC.md says to empty the kept preview when the world changes without a tick. (review)

## Tests

- In tests/sim/park_world_checks_test.cpp, the test that each chosen edit's candidate equals the world that queuing the edit for the cycle gives also commits each chosen edit with commitCommands. Setup: warm.park as a cycle leaves it, the world from which the test already makes each chosen edit's candidate. Change: commit the chosen edit's queued commands to a copy of that world with commitCommands, with no cycle. Expected: the committed world equals the edit's candidate, and its tick is the tick of the world it was committed to. This is the park world checks' standard that a candidate equals the world committing its commands gives, extended to the second way of committing.

## Medium

None. The feature adds no fields or flows. commitCommands is a new entry point to the tick cycle (decision 0025's candidate resolution, applied to the world itself). The app's speed reaches only how many ticks each frame steps.

## Spec changes

src/sim/SPEC.md:

- In The tick cycle, after "stepWorld(world) is the same cycle with no commands.", add:

    commitCommands(world, commands) applies the queued commands in submission order and resolves, without stepping the systems, advancing World::Tick, or running the swaps. It empties the queue. The world then equals the candidate makeCandidate would have made from it with those commands, so if a cycle has just left it, it equals the world that queuing the commands for that cycle would have given. If the queue is empty and no resolution is pending, nothing changes. If a queued command's type isn't registered, it throws std::invalid_argument and changes nothing. The app uses it to apply the player's edits while the game is paused.

src/app/SPEC.md:

- In Boundary, replace "It changes the world in only two ways. It queues edits for the next cycle, and it replaces the world when a park is opened or a new one started." with:

    It changes the world in only three ways. It queues edits for the next cycle, it applies queued edits with commitCommands while the game is paused, and it replaces the world when a park is opened or a new one started.

- In Main loop, replace "Each frame steps as many ticks as the elapsed time covers, at most MAX_FRAME_TICKS." with:

    The player runs the park paused or at 1x, 2x, or 4x. Each frame steps as many ticks as its elapsed time times the speed covers, at most MAX_FRAME_TICKS times the speed. While paused, a frame steps no tick and no time is owed to the park, so unpausing steps no burst. The speed changes only how many ticks a frame steps, so the world at a given tick is the same at any speed.

- In The scene, replace "The guests are rebuilt whenever the world ticks." with:

    The guests are rebuilt whenever the world ticks or the park's intent changes, as an edit applied while paused changes it.

- In Tools and the ghost, replace "When the tool commits an edit, the edit applies at the next tick." with:

    When the tool commits an edit, the edit applies at the next tick, or at once while paused.

- In Tools and the ghost, replace "Otherwise they apply at the next tick, with no ghost (decision 0029), in a tick of their own, so an edit committed while one waits applies at the tick after." with:

    Otherwise they apply at the next tick, with no ghost (decision 0029), in a tick of their own, so an edit committed while one waits applies at the tick after. While paused, they apply at the next frame instead, alone.

- In Tools and the ghost, replace "The preview is made again whenever the world ticks or the edit changes, so the ghost, the food overlay, and the food tooltip agree and update every tick." with:

    The preview is made again whenever the world ticks, the park's intent changes, or the edit changes, so the ghost, the food overlay, and the food tooltip agree, update every tick, and show an edit applied while paused at once.

- In Tooling UI, replace the Time panel's paragraph with:

    The Time panel shows the park's day and its time of day on a 24-hour clock in hours and minutes, such as `Day 1  09:00` (sim/SPEC.md, The clock). It follows every tick. Below the clock, radio buttons Pause, 1x, 2x, and 4x show the speed and choose it. Space pauses, and pressed again runs the park at the speed it ran at before. 1, 2, and 3 choose 1x, 2x, and 4x and unpause. The keys do nothing while a text field has the keyboard. The app starts at 1x, and keeps its speed when a park is opened or a new one started. With no saved layout the panel starts at the window's top center.

- In Command line, add after the --park bullet:

    - --speed N starts the app paused when N is 0, and at 1x, 2x, or 4x when N is 1, 2, or 4.

- In Command line, in the list of what prints the usage, add "- a --speed value other than 0, 1, 2, or 4" after "- a missing or malformed value". The bullet "--hash with any option that needs a window" already covers --speed with --hash, since --speed needs a window.

src/tools/SPEC.md, in History:

- Replace "noteEdits takes the world after a cycle that applied commands." with "noteEdits takes the world after commands were applied, by a cycle or by commitCommands (sim/SPEC.md)."
- Replace "If its command applies alone in its cycle, noteEdits finds the key each restored record took." with "If its command applies alone, in its cycle or by commitCommands, noteEdits finds the key each restored record took."

src/legible/SPEC.md, in Previews, replace "If a caller replaces the world, the caller must empty the kept preview." with:

    If a caller replaces the world, or changes it without a tick, as edits applied while paused change it, the caller must empty the kept preview.

## Files affected

- Modify: src/sim/world.h (commitCommands, and World's friend list), src/sim/cycle.cpp, src/sim/SPEC.md
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)
- Create: src/app/input/game_speed.h, src/app/input/game_speed.cpp
- Modify: src/app/input/input_map.h, src/app/input/input_map.cpp (Space, 1, 2, and 3)
- Modify: src/app/frame_clock.h, src/app/frame_clock.cpp (ticks scaled by the speed)
- Modify: src/app/session/park_session.h, src/app/session/park_session.cpp (applying edits without a tick)
- Modify: src/app/scene/scene_sync.h, src/app/scene/scene_sync.cpp (guests and the kept preview follow intent)
- Modify: src/app/ui/time_panel.h, src/app/ui/time_panel.cpp, src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp (the speed buttons)
- Modify: src/app/options.h, src/app/options.cpp (--speed)
- Modify: src/app/application.h, src/app/application.cpp (holding the speed and applying paused edits; composition only)
- Modify: src/app/CMakeLists.txt (adds input/game_speed.cpp to tpj_app_core)
- Modify: src/app/SPEC.md, src/legible/preview.h (keepPreview's comment), src/legible/SPEC.md
- Modify: src/tools/SPEC.md, src/tools/history.h (noteEdits' and undoEdit's comments)

## Dependencies

- time-of-day: the clock and the Time panel. Merged.
- path-network's undo and redo. Merged.

## Out of scope

- Speeds above 4x, and skipping the closed hours. They are the milestone's deepening candidates.
- Saving the speed in a park file. The speed is the player's view of the park, not the park.
- Adding 4x to scripts/runtime-report.sh. The criterion's run reports it once.

## Open questions

None.
