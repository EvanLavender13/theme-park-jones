# Implementation Plan: Game Speed

## Goal

The player pauses or runs the park at 1x, 2x, or 4x from the Time panel or the keys, and edits, undo, and redo apply at once while paused, with the simulation unchanged.

## Approach

The sim gains commitCommands, makeCandidate's apply-and-resolve done to the world itself. The app gains a GameSpeed holding whether the park is paused and the speed it runs at. The frame clock multiplies the time it owes the park by the speed, and the park session applies waiting edits with commitCommands while paused. The scene treats a change of park intent like a tick: it rebuilds the guests and empties the kept preview, so a paused edit shows at once. This feature's RESEARCH.md gives the reasons.

## Placement

Decision 0027 places each behavior this feature adds:

- Applying commands without a tick: sim root, sim/world.h with cycle.cpp, beside stepWorld and makeCandidate. The tick cycle is the sim root's, and only it can set a world's pending resolution.
- The game's speed and pause: app input, a new component src/app/input/game_speed.h with game_speed.cpp. It is the player's choice, which the keys and the Time panel change, as input_map and interaction hold the player's other choices. cmake/layers.txt puts app/input below app/ui and app/*, so the Time panel and the application can both use it.
- Space, 1, 2, and 3: app input, src/app/input/input_map.h with input_map.cpp, which maps every other key.
- Ticks scaled by the speed: app, src/app/frame_clock.h with frame_clock.cpp, which already turns real time into ticks.
- Applying edits while paused: app session, src/app/session/park_session.h with park_session.cpp, which already applies edits and notes them in the history.
- Rebuilding guests and the preview after a paused edit: app scene, src/app/scene/scene_sync.h with scene_sync.cpp, which already decides what each frame rebuilds.
- The speed buttons: app UI, src/app/ui/time_panel.h with time_panel.cpp, the Time panel's own component.
- --speed: app, src/app/options.h with options.cpp, which reads every option.
- Holding the speed, passing it to the clock and the panel, and applying paused edits each frame: app, src/app/application.cpp. It already calls each frame's components in order, and this adds calls only, so it is composition. Nothing is added to src/app/main.cpp.

## Tasks

### Task 1: Record the simulation before the change

Files:
- Create, scratch: `build/game-speed/before.txt`

Step 1: Build tpj_scenarios on main as it is, and write its output for the checked-in parks.

Run: `mkdir -p build/game-speed && cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/*.park tests/parks/stress/winding-path.park > build/game-speed/before.txt; echo "status $?"`
Expected: a timing line for each scenario and each park on the terminal, then `status 0`.

### Task 2: Describe commitCommands in the sim spec

Files:
- Modify: `src/sim/SPEC.md` (The tick cycle)

Step 1: Add the paragraph FEATURE.md's Spec changes gives for src/sim/SPEC.md.

### Task 3: Describe the speeds and paused edits in the app, tools, and legible specs

Files:
- Modify: `src/app/SPEC.md` (Boundary, Main loop, The scene, Tools and the ghost, Tooling UI, Command line)
- Modify: `src/tools/SPEC.md` (History)
- Modify: `src/legible/SPEC.md` (Previews)
- Modify: `src/tools/history.h` (noteEdits' and undoEdit's comments)

Step 1: Make each edit FEATURE.md's Spec changes gives for src/app/SPEC.md, src/tools/SPEC.md, and src/legible/SPEC.md.

Step 2: In src/tools/history.h, change noteEdits' "Call it after each cycle that applied commands." to "Call it after commands were applied, by a cycle or by commitCommands.", and undoEdit's "Apply it alone in its cycle." to "Apply it alone, in its cycle or by commitCommands."

### Task 4: Declare commitCommands

Files:
- Modify: `src/sim/world.h`
- Modify: `src/sim/cycle.cpp`

Step 1: In src/sim/world.h, after makeCandidate's declaration, declare `void commitCommands(World &world, CommandQueue &commands);` with the comment: "Applies the queued commands in submission order and resolves, without stepping the systems, advancing Tick, or running the swaps, as makeCandidate does to its copy. Empties the queue. Changes nothing when the queue is empty and no resolution is pending. Throws std::invalid_argument, changing nothing, if a queued command's type is not registered." Add `friend void commitCommands(World &world, CommandQueue &commands);` to World's friend list, after makeCandidate's.

Step 2: In src/sim/cycle.cpp, after makeCandidate, define a stub that only empties the queue: `void commitCommands(World &world, CommandQueue &commands) { (void)world; commands.clear(); }`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build succeeds with no warnings.

### Task 5: Run the test pass

Dispatch the test-writer as implementing-features says, with FEATURE.md, src/sim/SPEC.md, and src/sim/world.h. When it returns, build tpj_sim_tests and run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: the build succeeds. The changed test fails because a world committed with the stub doesn't equal its candidate, and every other test passes.

### Task 6: Apply commands without a tick

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: Replace the stub. Call requireRegistered(world, commands). If the queue isn't empty, call applyCommands(world, commands), set world.ResolvePending to true, and clear the queue. Then, if world.ResolvePending, call resolveWorld(world). Open the body with TPJ_PROFILE_ZONE() as makeCandidate does.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 7: Hold the game's speed

Files:
- Create: `src/app/input/game_speed.h`
- Create: `src/app/input/game_speed.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: In src/app/input/game_speed.h, with the include guard TPJ_APP_INPUT_GAME_SPEED_H, in namespace tpj, declare:

- `enum class SpeedAction : uint8_t { None, TogglePause, RunNormal, RunDouble, RunQuadruple };` with the comment "What the player asked of the game's speed this frame: to pause or unpause, or to run at 1x, 2x, or 4x."
- `class GameSpeed` with the comment "Whether the park is paused, and the speed it runs at, or will run at when unpaused: 1, 2, or 4 ticks for each tick of real time." Its members:
  - `explicit GameSpeed(uint32_t speed);` "Paused at 1x when speed is 0, and otherwise running at speed, which is 1, 2, or 4."
  - `[[nodiscard]] uint32_t multiplier() const;` "The ticks the park steps for each tick of real time: 0 while paused."
  - `[[nodiscard]] bool paused() const;`
  - `[[nodiscard]] uint32_t running() const;` "The speed it runs at, or will when unpaused."
  - `void pause();` "Pauses, keeping the speed it ran at."
  - `void run(uint32_t speed);` "Unpauses at speed, which is 1, 2, or 4."
  - `void use(SpeedAction action);` "TogglePause pauses, or unpauses at the speed it ran at. RunNormal, RunDouble, and RunQuadruple run at 1, 2, and 4. None changes nothing."
  - Private members `uint32_t Running = 1;` and `bool Paused = false;`.

Step 2: Define them in src/app/input/game_speed.cpp.

Step 3: Add `input/game_speed.cpp` to tpj_app_core's sources in src/app/CMakeLists.txt, in alphabetical order after `input/cursor.cpp`.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build succeeds with no warnings.

### Task 8: Map Space, 1, 2, and 3

Files:
- Modify: `src/app/input/input_map.h`
- Modify: `src/app/input/input_map.cpp` (mapEvent)

Step 1: In input_map.h, include `app/input/game_speed.h`, and add `SpeedAction Speed = SpeedAction::None;` to FrameInput after History, with the comment "What the player asked of the game's speed with Space, 1, 2, or 3." Add to mapEvent's comment: "While ImGui doesn't want the keyboard, a press of Space with no Ctrl held asks to pause or unpause, and a press of 1, 2, or 3 asks to run at 1x, 2x, or 4x. Their repeats don't count."

Step 2: In mapEvent's SDL_EVENT_KEY_DOWN branch, when `!keyboardWanted`, `!event.key.repeat`, and Ctrl isn't held, set input.Speed from event.key.key: SDLK_SPACE to TogglePause, SDLK_1 to RunNormal, SDLK_2 to RunDouble, and SDLK_3 to RunQuadruple. Keep the Ctrl branch for undo and redo as it is.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build succeeds with no warnings.

### Task 9: Scale a frame's ticks by the speed

Files:
- Modify: `src/app/frame_clock.h`
- Modify: `src/app/frame_clock.cpp`

Step 1: Change advance's declaration to `FrameStep advance(uint64_t counter, uint32_t multiplier);`, and its comment to: "For the reading at a frame's start: the seconds since the last reading, at most MAX_FRAME_SECONDS, and as many SIM_TICK_SECONDS ticks as the time not yet stepped then holds, at most MAX_FRAME_TICKS times the multiplier, keeping the part of a tick left over and dropping the whole ticks beyond the cap. The time a frame owes the park is its seconds times the multiplier, so a multiplier of 0, while paused, owes none." Change MAX_FRAME_TICKS's comment to say it caps a frame's ticks at 1x, and the cap grows with the speed.

Step 2: In frame_clock.cpp, add `step.Dt * static_cast<double>(multiplier)` to Unstepped in place of step.Dt, and compare step.Ticks with `MAX_FRAME_TICKS * multiplier` in place of MAX_FRAME_TICKS.

Step 3: In src/app/application.cpp, pass 1 as the multiplier for now, so the app builds.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 10: Add the speed buttons to the Time panel

Files:
- Modify: `src/app/ui/time_panel.h`
- Modify: `src/app/ui/time_panel.cpp`
- Modify: `src/app/ui/tooling_ui.h`
- Modify: `src/app/ui/tooling_ui.cpp`

Step 1: Change drawTimePanel to `void drawTimePanel(const ParkTime &time, GameSpeed &speed);`, including `app/input/game_speed.h`, and add to its comment: "Below the clock, radio buttons Pause, 1x, 2x, and 4x, with the current one marked, pause the speed or run it at theirs."

Step 2: In time_panel.cpp, after the clock's text, draw `ImGui::RadioButton("Pause", speed.paused())`, then on the same line one for each of 1, 2, and 4, labeled "1x", "2x", and "4x", marked when `!speed.paused() && speed.running() == N`. A press of Pause calls speed.pause(), and a press of one of the others calls speed.run(N).

Step 3: Add a `GameSpeed &speed` parameter to ToolingUi::build after `interaction`, and to drawPanels, and pass it to drawTimePanel. Add "the Time panel's speed buttons, which set the speed," to build's comment.

Step 4: In src/app/application.cpp, add a `GameSpeed Speed{1};` member to Application in application.h, after Clock, and pass it to Ui.build, so the app builds.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 11: Read --speed

Files:
- Modify: `src/app/options.h`
- Modify: `src/app/options.cpp`

Step 1: Add `uint32_t Speed = 1;` to Options after FrameTimes, with the comment "The speed to start at: 0 for paused at 1x, or 1, 2, or 4." Add to parseOptions' comment: "--speed N starts at speed N", and add "a --speed value other than 0, 1, 2, or 4" and --speed to the options --hash refuses.

Step 2: In parseOptions, read `--speed` with a value through parseCount into a uint64_t, valid only when it is 0, 1, 2, or 4, and store it in options.Speed. Note that it was given, and refuse --hash with it as with --frames. Add `[--speed N]` to the usage line after `[--ticks N]`.

Step 3: Initialize Application's Speed from options.Speed in its constructor.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --speed 3; echo "status $?"; build/windows-debug/ThemeParkJones.exe --speed 1 --hash; echo "status $?"`
Expected: the build succeeds with no warnings, and each run prints the usage, with `[--speed N]` in it, then `status 1`.

### Task 12: Apply edits while paused

Files:
- Modify: `src/app/session/park_session.h`
- Modify: `src/app/session/park_session.cpp`

Step 1: Declare `void applyWithoutTick();` after step(), with the comment: "While paused: applies the waiting undo or redo alone, when one waits, or else the queued commands, which it empties, with commitCommands and no cycle. The history then notes what changed. Does nothing when nothing waits or is queued."

Step 2: Define it as step() is defined, with commitCommands in place of stepWorld, and returning early with no change when no undo or redo waits and the queue is empty.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build succeeds with no warnings.

### Task 13: Rebuild guests and the preview when intent changes

Files:
- Modify: `src/app/scene/scene_sync.h`
- Modify: `src/app/scene/scene_sync.cpp`
- Modify: `src/legible/preview.h` (keepPreview's comment)

Step 1: In SceneSync::syncWorld, when the intent differs from the last call's, also set meshes.Guests and empty the kept preview (`Kept = {};`). Change syncWorld's comment to: "Otherwise the park mesh when the world's intent differs from the last call's, and the guest mesh when its intent or its tick does. When the intent differs, the kept preview is emptied, so an edit applied without a tick is previewed afresh."

Step 2: In src/legible/preview.h, change keepPreview's comment from "A world changes only as it ticks, so the kept preview is previewEdit's for as long as it is kept for one world: empty it when the world is replaced." to "The kept preview is previewEdit's only while the world changes just by ticking: empty it when the world is replaced, or changed without a tick, as edits applied while paused change it."

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build succeeds with no warnings.

### Task 14: Run the park at the player's speed

Files:
- Modify: `src/app/application.cpp` (runFrames)

Step 1: After gatherInput, call `Speed.use(input.Speed);`. Pass `Speed.multiplier()` to Clock.advance in place of 1.

Step 2: After the loop that steps the frame's ticks, call `Session.applyWithoutTick();` when `Speed.paused()`. Change the comment above the loop to: "The simulation advances in fixed ticks regardless of frame rate, as many as the speed asks, and queued edits apply at the next, or at once while paused (principle 10)."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 15: Check the speeds by capture and play

Files:
- Create, scratch: `build/game-speed/` captures, crops, and frame time lines

Step 1: Capture main-street.park after 600 frames at 1x and at 4x, and crop the clock.

Run: `for s in 1 4; do build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --speed $s --frames 600 --capture build/game-speed/speed$s.bmp; done; python3 -c "from PIL import Image; [Image.open(f'build/game-speed/speed{s}.bmp').crop((700,8,900,90)).resize((400,164)).save(f'build/game-speed/speed{s}.png') for s in (1, 4)]"`
Expected: speed1.png's clock shows a few minutes past 09:00, and speed4.png's about four times as many. Both show the radio buttons, with 1x and 4x marked. If a crop misses the panel, crop again where the full capture shows it.

Step 2: Time full.park at 1x and at 4x with windows-release.

Run: `cmake.exe --build --preset windows-release --target tpj_app && for s in 1 4; do build/windows-release/ThemeParkJones.exe --park tests/parks/stress/full.park --speed $s --frames 600 --frame-times --capture build/game-speed/full$s.bmp > build/game-speed/full$s.txt; done; python3 -c "
import statistics
for s in (1, 4):
    t = sorted(int(x) / 1e6 for x in open(f'build/game-speed/full{s}.txt').read().split()[1:])
    print(s, 'median', round(statistics.median(t), 2), 'ms, slowest 5% from', round(t[int(len(t) * 0.95)], 2), 'ms')"`
Expected: a median and a 95th percentile line for each speed. Look at the clock in each capture: 4x's has gone further than 1x's. Report both lines and both clocks. A slow 4x is not a failure, since the criterion asks for a steady frame rate, not 4x itself. If the slowest 5% at 4x is far above its median, report it.

Step 3: Play warm-main-street.park.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm-main-street.park`
Expected: Space pauses, the clock and guests stop, and the camera, tools, and ghost keep working. Narrowing the main street with Edit path while paused moves the guests standing past its new edges onto the narrowed paving at once, and the ghost of a second edit starts from the edited street. Ctrl+Z restores the width at once, and Ctrl+Y narrows it again. 1, 2, and 3 run at 1x, 2x, and 4x, and the clock speeds up to match. Report what was seen, and leave the full check to the user's play at the milestone's close.

### Task 16: Confirm the simulation is unchanged

Files:
- Create, scratch: `build/game-speed/after.txt`

Step 1: Write the same output with this feature's tpj_scenarios, and compare.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/*.park tests/parks/stress/winding-path.park > build/game-speed/after.txt 2>/dev/null && build/windows-debug/tpj_scenarios.exe --compare build/game-speed/before.txt build/game-speed/after.txt; echo "status $?"`
Expected: `status 0`.

### Task 17: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 18: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/sim/world.h, src/sim/cycle.cpp, src/sim/SPEC.md, tests/sim/park_world_checks_test.cpp, src/app/, src/legible/preview.h, src/legible/SPEC.md, src/tools/SPEC.md, src/tools/history.h, and plans/deterministic-simulation/park-clock/game-speed/. Commit them by pathspec with the commit-hygiene skill, subject `App: Pause and run the park at 1x, 2x, or 4x`. If the format hook rewraps a file after the commit, add it and amend the commit before merging. Merge into main as implementing-features says, and rebuild windows-release.
