# Implementation Plan: Need Overlays

## Goal

Give each guest need an overlay on the guest paving, chosen one at a time, discounted by one curve in game minutes, with 0 shown as the ramp's worst color.

## Approach

The availability library already takes a definition for a need. It gains a shared curve in game minutes and converts its walk and wait to game minutes, and a new needs.h gives the definition for each GuestNeed, replacing food.h. The renderer drops its near-white zero color, so 0 takes the ramp's bottom. The app replaces its food overlay flag with an optional GuestNeed throughout: the options, the shown views, the preview look, the Debug panel's Overlay row, and a tooltip for the shown need.

## Placement

Decision 0027 places each behavior this feature adds:

- The shared curve and game-minute times: legible, availability.h. The availability library owns how a relief is discounted.
- Each need's availability and its name: legible, a new src/legible/needs.h replacing food.h. It names the offers each need reads, which is legible's knowledge of the medium. The app and tpj_bench ask it for a need's definition.
- Showing 0 as the ramp's worst color: render, paving_overlay.h, renderer.cpp, and overlay.frag, which own the overlay's colors.
- Reading --overlay with a need's name: app, the options component (src/app/options.h).
- The Overlay row: app, the Debug panel (src/app/ui/debug_panel.h). The choice is held in ShownViews (src/app/ui/tooling_ui.h), as the checkbox's flag was.
- The tooltip for the shown need: app, a new src/app/ui/need_tooltip.h replacing food_tooltip.h. It is the same tooltip for any need.
- Shading the shown need's overlay: app, src/app/scene/scene_uploads.cpp, which composes the frame's meshes. It passes the need's definition to views' availabilityShades.

## Tasks

### Task 1: Record the before state

Files: none in the repository. Outputs go to build/need-overlays/.

Step 1: On the feature branch, before any change, build both presets.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release`
Expected: both finish with no warnings.

Step 2: Record the simulation's output.

Run: `mkdir -p build/need-overlays && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/need-overlays/scenarios-before.txt`
Expected: exit status 0.

Step 3: Capture the food overlay as it is.

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/noon.park --speed 0 --overlay food --capture build/need-overlays/food-before.bmp`
Expected: exit status 0.

Step 4: Start the runtime report in the background and let it finish. Don't stop it.

Run: `scripts/runtime-report.sh build/need-overlays/runtime-before.txt`
Expected: it writes the report. It takes about 20 minutes. Tasks 2 to 12 change no file it builds until their own builds, so run Tasks 2 to 4 while it runs and start Task 5 after it ends.

### Task 2: Describe the needs' availabilities in legible's spec

Files:
- Modify: `src/legible/SPEC.md`

Step 1: Make the edits FEATURE.md's Spec changes lists for src/legible/SPEC.md, word for word.

### Task 3: Describe the ramp and the shades in render's and views' specs

Files:
- Modify: `src/render/SPEC.md`
- Modify: `src/views/SPEC.md`

Step 1: Make the edits FEATURE.md's Spec changes lists for src/render/SPEC.md and src/views/SPEC.md, word for word.

### Task 4: Describe the Overlay row and --overlay in the app's spec

Files:
- Modify: `src/app/SPEC.md`

Step 1: Make the edits FEATURE.md's Spec changes lists for src/app/SPEC.md, word for word.

### Task 5: Give availability game minutes and the shared curve

Files:
- Modify: `src/legible/availability.h`
- Modify: `src/legible/availability.cpp`

Step 1: In availability.h, include sim/clock.h and sim/world.h. Replace REFERENCE_SPEED with `inline constexpr double REFERENCE_SPEED = WALK_SPEED * SIM_TICK_SECONDS * 60.0 / static_cast<double>(GAME_SECONDS_PER_TICK);`, commented "The meters guests walk in a game minute at their own speed, so an effective time is what a guest would feel, on the park's clock." After it, add DISCOUNT_CURVE as FEATURE.md gives it, commented "How much of a relief counts after an effective time in game minutes (decision 0020), for every need: half for each 20 minutes, and none from two hours."

Step 2: In AvailabilityContribution, rename `Shop` to `Source`. In its comment and in AvailabilityDefinition's, say the wait and the effective time are in game minutes.

Step 3: In availability.cpp, add to the anonymous namespace `double waitMinutes(uint64_t ticks)`, giving `static_cast<double>(ticks) * static_cast<double>(GAME_SECONDS_PER_TICK) / 60.0`, commented "An offer's wait, in game minutes." Use it in place of `static_cast<double>(offer.Wait) * SIM_TICK_SECONDS` in contributionOf and of `static_cast<double>(offer->Wait) * SIM_TICK_SECONDS` in addEdgeBends. Set `contribution.Source` in place of `contribution.Shop`. In the comments of discount and addBends, and in addBends's parameter name, say minutes in place of seconds.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: builds with no warnings.

### Task 6: Add each need's availability

Files:
- Create: `src/legible/needs.h`
- Create: `src/legible/needs.cpp`
- Delete: `src/legible/food.h`
- Delete: `src/legible/food.cpp`
- Modify: `src/legible/CMakeLists.txt`

Step 1: In needs.h, under the guard TPJ_LEGIBLE_NEEDS_H, include legible/availability.h, sim/guests/guests.h, and <string_view>, and declare needAvailability and needAvailabilityName as FEATURE.md gives them. Comment them "The availability of a need: food from shops' food offers, drink from shops' drink offers, rest from benches' offers, and the toilet from toilet blocks' offers, each discounted by DISCOUNT_CURVE, with an overlay of it halfway up its ramp at one serving's relief with no walk and no wait." and "The need's name as the Overlay row and its tooltip show it: Food, Drink, Rest, or Toilet."

Step 2: In needs.cpp, define one AvailabilityDefinition for each need in an anonymous namespace, in NEEDS order, from `&sourceEntryAtNode<FoodOffer>` with MEAL_RELIEF, `&sourceEntryAtNode<DrinkOffer>` with DRINK_RELIEF, `&sourceEntryAtNode<RestOffer>` with REST_RELIEF, and `&sourceEntryAtNode<ToiletOffer>` with TOILET_RELIEF, each with DISCOUNT_CURVE. Include sim/medium/field.h, sim/operations/operations.h, and sim/operations/facilities.h. needAvailability and needAvailabilityName switch on the need.

Step 3: Delete food.h and food.cpp with `git rm`. In src/legible/CMakeLists.txt, replace `food.cpp` with `needs.cpp`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: builds with no warnings. The app and tpj_bench build again after Task 12.

### Task 7: Show 0 as the ramp's worst color

Files:
- Modify: `src/render/paving_overlay.h`
- Modify: `src/render/renderer.cpp`
- Modify: `src/render/shaders/overlay.frag`

Step 1: Remove OVERLAY_ZERO_COLOR and its comment from paving_overlay.h. In OVERLAY_RAMP's comment, say the dark purple is 0, the worst value.

Step 2: In renderer.cpp, remove `Zero` from RampUniforms and the line that sets it.

Step 3: In overlay.frag, remove `vec4 zero;` from the Ramp block. In rampColor, replace the zero branch with `float shown = value > 0.0 ? value : 0.0;`, commented "0, the worst value, and anything not above it take the ramp's bottom.", and scale `shown` in place of `value`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: builds with no warnings, the shader included.

### Task 8: Read --overlay with a need's name

Files:
- Modify: `src/app/options.h`
- Modify: `src/app/options.cpp`

Step 1: In options.h, include sim/guests/guests.h. Replace `bool ShowFoodOverlay = false;` with `std::optional<GuestNeed> Overlay;`, commented "The need whose overlay --overlay shows at start, if it was given." In parseOptions's comment, say --overlay food, drink, rest, or toilet starts with that overlay, and that it refuses an --overlay value other than those.

Step 2: In options.cpp, add to the anonymous namespace a constexpr array of the four names `food`, `drink`, `rest`, and `toilet`, in NEEDS order, commented "--overlay's names for the needs, in NEEDS order." In the --overlay branch, find argv[++i] in it. If found, set options.Overlay to the NEEDS entry at the same index; otherwise the option is invalid. In consistent, use `options.Overlay.has_value()` in place of `options.ShowFoodOverlay`. In the usage line, replace `[--overlay food]` with `[--overlay food|drink|rest|toilet]`.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: builds with no warnings.

### Task 9: Choose the overlay in the Debug panel

Files:
- Modify: `src/app/ui/debug_panel.h`
- Modify: `src/app/ui/debug_panel.cpp`
- Modify: `src/app/ui/tooling_ui.h`
- Modify: `src/app/scene/scene_sync.h`

Step 1: In tooling_ui.h and scene_sync.h, include sim/guests/guests.h and <optional>. In tooling_ui.h, replace ShownViews's `bool FoodOverlay = false;` with `std::optional<GuestNeed> Overlay;`. In scene_sync.h, replace PreviewLook's `bool FoodOverlay = false;` with `std::optional<GuestNeed> Overlay;`, and in its comment say "which need's overlay shows, if any" in place of "whether the food overlay shows". In syncOverlayShape's comment, say "the overlay's shape" in place of "the food overlay's shape".

Step 2: In debug_panel.h, include sim/guests/guests.h and <optional>, and change drawDebugPanel to the signature FEATURE.md gives. Its comment ends: "the Graph checkbox, which sets showGraph, and the Overlay row of radio buttons Off, Food, Drink, Rest, and Toilet, which sets overlay to no need or to that need."

Step 3: In debug_panel.cpp, replace the Food overlay checkbox with the text "Overlay", then on the same line a RadioButton "Off", marked while overlay has no value and setting it to none, then one RadioButton for each need in NEEDS, labeled with needAvailabilityName, marked while overlay holds it and setting it, each with ImGui::SameLine before it. Include legible/needs.h.

### Task 10: Show the tooltip for the shown need

Files:
- Create: `src/app/ui/need_tooltip.h`
- Create: `src/app/ui/need_tooltip.cpp`
- Delete: `src/app/ui/food_tooltip.h`
- Delete: `src/app/ui/food_tooltip.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Move food_tooltip.h and food_tooltip.cpp to need_tooltip.h and need_tooltip.cpp with `git mv`. Rename the guard to TPJ_APP_UI_NEED_TOOLTIP_H, add sim/guests/guests.h to the header's includes, and rename the function to drawNeedTooltip, with the signature FEATURE.md gives and the comment "Draws a tooltip at the cursor attributing the need's availability at the overlay place of the point under it to its facilities, and nothing when there is no point or it has no overlay place in the shape. Call between ImGui::NewFrame and ImGui::Render."

Step 2: In need_tooltip.cpp, include legible/needs.h in place of legible/food.h. Take `availabilityAt(world, needAvailability(need), *place)`. Title the tooltip with needAvailabilityName and the value, as `%s %.3f`. When there are no contributions, show "No supplied facility reachable". Name the first column "Facility", show `contribution.Source`, and show Wait and Time as `%.1f min`.

Step 3: In src/app/CMakeLists.txt, replace `ui/food_tooltip.cpp` with `ui/need_tooltip.cpp`.

### Task 11: Shade and hover the shown need

Files:
- Modify: `src/app/ui/tooling_ui.cpp`
- Modify: `src/app/ui/tooling_ui.h`
- Modify: `src/app/scene/scene_uploads.cpp`
- Modify: `src/app/application.cpp`

Step 1: In tooling_ui.cpp, include app/ui/need_tooltip.h in place of app/ui/food_tooltip.h. Pass `shown.Overlay` to drawDebugPanel. Replace `if (Shown.FoodOverlay)` and its drawFoodTooltip call with `if (Shown.Overlay)` and `drawNeedTooltip(previewedWorld(world, preview), *Shown.Overlay, overlayShape, ...)`, keeping the point argument. In tooling_ui.h's comment on build, say "the shown need's tooltip at the cursor while an overlay is" in place of "the food tooltip at the cursor while the overlay is".

Step 2: In scene_uploads.cpp, include legible/needs.h in place of legible/food.h. Replace `!look.FoodOverlay ||` and `FOOD_AVAILABILITY` with `!look.Overlay ||` and `needAvailability(*look.Overlay)`.

Step 3: In application.cpp, construct the ShownViews with `options.Overlay` in place of `options.ShowFoodOverlay`. Pass `Ui.shown().Overlay.has_value()` to both syncOverlayShape calls, and `Ui.shown().Overlay` to the PreviewLook.

### Task 12: Time food's overlay in tpj_bench

Files:
- Modify: `src/bench/stages.cpp`

Step 1: Include legible/needs.h in place of legible/food.h, and pass `needAvailability(GuestNeed::Hunger)` in place of FOOD_AVAILABILITY.

Step 2: Build everything.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 13: Check --overlay's refusals

Step 1: Check each refusal.

Run: `build/windows-debug/ThemeParkJones.exe --overlay water; echo $?; build/windows-debug/ThemeParkJones.exe --park tests/parks/noon.park --overlay drink --hash; echo $?`
Expected: twice, the usage line including `[--overlay food|drink|rest|toilet]`, then `1`.

### Task 14: Capture the overlays

Files: none in the repository. Outputs go to build/need-overlays/.

Step 1: Build the whole release preset.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

Step 2: With build/windows-release/ThemeParkJones.exe and `--park tests/parks/noon.park --speed 0`, capture each overlay to build/need-overlays/<name>.bmp: `--overlay food`, `--overlay drink`, `--overlay rest`, and `--overlay toilet`.
Expected: each exits with status 0.

Step 3: Read the captures and food-before.bmp.
Expected: toilet.bmp is in the ramp's best colors around the toilet block, and drink.bmp around the drink shop, with its worst around the food shop. rest.bmp is best around the bench. food.bmp is best around the food shop. No capture shows a near white on the paving where food-before.bmp shows one.

### Task 15: Confirm the feature

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports no findings.

Step 2: Compare the simulation with Task 1's output.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/need-overlays/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/need-overlays/scenarios-before.txt build/need-overlays/scenarios-after.txt; echo $?`
Expected: `0`

Step 3: Time the overlay, in the background, and don't stop it.

Run: `scripts/runtime-report.sh build/need-overlays/runtime-after.txt && scripts/runtime-report.sh --compare build/need-overlays/runtime-before.txt build/need-overlays/runtime-after.txt`
Expected: the comparison prints the food-overlay stage before and after. Report both medians.

Step 4: After the report ends, rebuild the whole release preset, then ask the user to play criteria 2, 3, and 4 in build/windows-release/ThemeParkJones.exe.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

### Task 16: Commit

Step 1: Stage only this feature's paths and commit with the commit-hygiene skill, for example "Legible: Add an overlay for each guest need".
