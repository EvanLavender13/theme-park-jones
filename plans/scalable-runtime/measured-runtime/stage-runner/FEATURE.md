# Feature: Stage Runner

## Summary

stage-runner adds the headless runner that times each stage of the gameplay runtime on a park file, and the unit it needs to time the overlay exactly as the app builds it. A new unit, src/views, holds the food overlay as the app shows it: render's band shaded by legible's food availability, which neither render nor legible may join, since they share a layer. The app builds its overlay through it. A second new unit, src/bench, holds tpj_bench: given a park file, it loads and resolves it, times one resolution, a preview, the food overlay, the park mesh, the guest mesh, and a run of ticks, and writes each stage's median, least, and greatest time in nanoseconds beside the stage's result, a hash or a vertex count, so a build that skipped work would show it. parks/lowfps.park, a single 3 km guest path with one shop, moves to tests/parks/stress/winding-path.park as the first stress park. Timings are written, never checked: no test fails on one.

## Acceptance criteria

1. buildFoodAvailabilityOverlay(world) gives the mesh buildFoodOverlay gives for the world with foodAvailability(world, place).Value as its value function, vertex for vertex and index for index, and changes nothing in the world.
2. summarizeTimes of a non-empty list of durations gives its Count, its Least and Greatest, and its Median, the element at index (Count - 1) / 2 once the durations are sorted ascending, and throws std::invalid_argument for an empty list.
3. benchPark(loaded, ticks) gives its stages in the order resolution, preview, food-overlay, park-mesh, guest-mesh, and ticks, with preview present exactly when the resolved park holds a box. Every stage but ticks has the Count REPETITIONS, and ticks has the Count ticks. In every stage, Least is at most Median and Median at most Greatest.
4. Each stage's result is what its work gives with nothing timed. Resolution's is the hash of the loaded world resolved. Preview's is the hash of makeCandidate of the resolved park with a queue holding MoveBox of its lowest-keyed box to that box's own pose. Food-overlay's, park-mesh's, and guest-mesh's are the vertex counts of buildFoodAvailabilityOverlay, buildParkMesh, and buildGuestMesh of the resolved park. Ticks' is the hash of the resolved park after stepWorld ticks times. benchPark leaves the loaded world unchanged.
5. benchPark throws std::runtime_error, naming the box, when previewEdit of that MoveBox on the resolved park gives no candidate, which only a park that is not physically valid allows, and std::invalid_argument when ticks is 0.
6. parseBenchOptions reads `tpj_bench [--ticks N] FILE`, with N DEFAULT_TICKS without --ticks, and refuses, with a message naming the problem, an unknown option, which the message names, a --ticks with no value or one that is not a positive decimal count, no file, and more than one file.
7. tpj_bench FILE writes to standard output its park line, `park <path> ticks <t> warm-ups <w> repetitions <r>`, then one line per stage of benchPark in its order, `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`, with times in nanoseconds as decimal integers, the result name hash and the result 16 lowercase hexadecimal digits for resolution, preview, and ticks, and the result name vertices and the result in decimal for the meshes. It exits with status 0. For options parseBenchOptions refuses, a file it cannot read, a file loadWorld refuses with the park's schema, or a park benchPark throws for, it writes to standard error a message naming the problem, the file, the file and LoadError's message, or what benchPark threw, and exits with a nonzero status, writing nothing to standard output.
8. cmake/layers.txt declares views in a layer of its own directly above tools, render, legible, and scenarios, and bench in a layer of its own directly above views, both below app's layers, and the layer check passes on the tree. Its expected table in tests/checks/layer_check_test.cmake changes to match, the one existing test whose source changes.

## Medium

This feature introduces, samples, and emits no fields or flows. views reads a world only through render's buildFoodOverlay and legible's foodAvailability, which read it through the medium. bench reads a world only through public headers: loadWorld, resolveWorld, copyWorld, hashWorld, stepWorld, makeCandidate, parkBoxes, previewEdit, views' overlay, and render's mesh builders.

## Principle checks

- Principle 10: the simulation runs independently of the runner. The hash after the timed ticks equals the hash of the resolved park stepped as many times with nothing timed (criterion 4), so reading the clock between ticks changes nothing a tick does. Simulation code never reads the clock: tpj_bench_lib does, and it is not simulation code.
- Principle 1: everything visible is derived and the park is never edited by its display. benchPark leaves the loaded world unchanged, and buildFoodAvailabilityOverlay changes nothing in the world it shades (criteria 1 and 4).
- Principle 8: the preview the runner times is the exact candidate. Its result hash equals makeCandidate's for the same edit (criterion 4).
- Principle 6: bench and views include only public headers, which the private header check covers on the tree.

## Spec changes

src/views/SPEC.md, new:

> # views
>
> What the app shows, joined from render's mesh builders and legible's explanations. render depends on no module but sim and core, and legible on sim alone, and the two share a layer, so neither may join them; views sits in the layer above both and below app (decision 0027). The library tpj_views links tpj_render and tpj_legible, builds only with the windowed application, as render does, and builds on the CPU, so it is tested without a GPU. It takes a world by const reference and changes nothing.
>
> ## Food overlay
>
> buildFoodAvailabilityOverlay(world) gives buildFoodOverlay of the world with, as its value function, foodAvailability's Value at each place in the world (render/SPEC.md, legible/SPEC.md). The app builds its food overlay with it, and tpj_bench times it, so what is timed is what is shown.

src/bench/SPEC.md, new:

> # bench
>
> The runtime runner (plans/scalable-runtime): a headless executable that times each stage of the gameplay runtime on a park file and writes each stage's result beside its times. The library tpj_bench_lib holds the stages, the summary of times, and the options. It links tpj_views, tpj_render, tpj_legible, and tpj_sim, and builds only with the windowed application, as render does. It reads a world only through public headers. It is the one reader of the clock that times runtime stages; it is not simulation code, and simulation code never reads the clock (principle 10). It is in a layer of its own above views and below app, so it includes nothing of app (decision 0027). No test checks a time.
>
> ## Times
>
> A stage's times are durations in nanoseconds, each read from std::chrono::steady_clock immediately before and after one call of its work. summarizeTimes(durations) gives a TimeSummary: Count, the number of durations; Median, the element at index (Count - 1) / 2 once they are sorted ascending, so of an even count it is the lower of the two middle ones; Least; and Greatest. It throws std::invalid_argument for an empty list.
>
> ## Stages
>
> benchPark(loaded, ticks) takes a loaded world and a count of ticks, changes nothing in the loaded world, and gives a list of StageResult, each a Name, the TimeSummary of its counted calls, a ResultKind, hash or vertices, and a Result, in this order. It throws std::invalid_argument when ticks is 0.
>
> - resolution: each call resolves a copy of the loaded world with resolveWorld, the copy made before the clock is read. Its result is the hash of the resolved world, which the rest call the resolved park.
> - preview, present only when parkBoxes of the resolved park is not empty: each call is previewEdit of the resolved park and MoveBox of the lowest-keyed box to that box's own pose, an edit accepted on a physically valid park that leaves its networks as they are, so the preview costs what the park itself costs to resolve. Its result is the hash of the preview's candidate. When previewEdit gives no candidate, benchPark throws std::runtime_error naming the box by its key in decimal.
> - food-overlay: each call is buildFoodAvailabilityOverlay of the resolved park (views/SPEC.md). Its result is the mesh's vertex count.
> - park-mesh: each call is buildParkMesh of the resolved park. Its result is the mesh's vertex count.
> - guest-mesh: each call is buildGuestMesh of the resolved park. Its result is the mesh's vertex count.
> - ticks: from a copy of the resolved park, each call is one stepWorld with no commands, ticks calls in sequence, so no timed tick includes the loaded park's first resolution. Its result is the hash of the copy after them, which equals the resolved park's after as many cycles with nothing timed.
>
> Every stage but ticks first makes WARM_UPS calls, 2, untimed, and then REPETITIONS calls, 11, whose times it summarizes. Ticks has no warm-up, since each tick changes the world it steps, and summarizes all of its calls. Each call's output is kept until after the clock is read, so freeing it is never timed, and the result is computed from the last call's output after the clock is last read, and written out, so the work cannot be dropped as unused, and a build that timed less work would show a different result.
>
> ## Command line
>
> tpj_bench [--ticks N] FILE reads FILE, loads it with makeParkSchema, and writes to standard output `park <path> ticks <t> warm-ups <w> repetitions <r>`, then one line per stage of benchPark of the loaded world and t: `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`. The path is FILE as given, t the ticks, w WARM_UPS, and r REPETITIONS, in decimal; times are nanoseconds in decimal; the result name is hash for resolution, preview, and ticks, with the result as 16 lowercase hexadecimal digits, and vertices for the meshes, with the result in decimal. Every line ends with a line feed, and standard output is in the C runtime's text mode, so on Windows each arrives as a carriage return and line feed. The lines are written only once benchPark has returned. N defaults to DEFAULT_TICKS, 300. It exits with status 0.
>
> parseBenchOptions(arguments, error), whose first argument is the program's name, reads each later argument that begins with -- as an option and any other as the file. It gives BenchOptions, the Ticks and the Park path, or none with error set to a message naming the problem: an unknown option, named in the message; --ticks with no value, or one that is not a positive decimal count, which a count too large for 64 bits is not; no file; or more than one. A later --ticks replaces an earlier one. tpj_bench then writes the message and the usage line `usage: tpj_bench [--ticks N] FILE` to standard error and exits with a nonzero status. readParkFile(path) gives the file's text, or none when it cannot be read, and tpj_bench then writes `tpj_bench: cannot read <path>` to standard error and exits with a nonzero status; when loadWorld throws LoadError, it writes `tpj_bench: cannot load <path>: <message>`, with LoadError's message, and exits with a nonzero status. When benchPark throws, it writes `tpj_bench: <what>` to standard error and exits with a nonzero status. None of these writes to standard output.
>
> Stress parks live in tests/parks/stress/, outside the tests/parks/*.park files that the cross-build check and the integration tests read. Each is named for what it holds. winding-path.park is a single guest path about 3 km long with one shop, the park on which the food overlay first played slowly.

src/render/SPEC.md, Food overlay: "which the app makes from foodAvailability's Value (legible/SPEC.md)" becomes "which views makes from foodAvailability's Value (views/SPEC.md, legible/SPEC.md)".

src/app/SPEC.md, Tooling UI: "it gives the renderer setOverlayMesh of buildFoodOverlay for previewedWorld of the world and the kept preview, shaded by foodAvailability's Value at each place in it, while the checkbox is checked" becomes "it gives the renderer setOverlayMesh of buildFoodAvailabilityOverlay for previewedWorld of the world and the kept preview (views/SPEC.md), while the checkbox is checked".

docs/conventions.md, Tests: "and checked-in park files in tests/parks/." becomes "and checked-in park files in tests/parks/, with the parks only tpj_bench runs in tests/parks/stress/."

## Files affected

- Create: src/views/CMakeLists.txt, src/views/SPEC.md, src/views/food_overlay.h, src/views/food_overlay.cpp
- Create: src/bench/CMakeLists.txt, src/bench/SPEC.md, src/bench/timing.h, src/bench/timing.cpp, src/bench/stages.h, src/bench/stages.cpp, src/bench/options.h, src/bench/options.cpp, src/bench/park_file.h, src/bench/park_file.cpp, src/bench/main.cpp
- Modify: CMakeLists.txt, cmake/layers.txt, src/app/CMakeLists.txt, src/app/scene/scene_uploads.cpp, src/app/SPEC.md, src/render/SPEC.md, docs/conventions.md
- Move: parks/lowfps.park to tests/parks/stress/winding-path.park
- The test pass creates tests/views/ and tests/bench/ with their CMakeLists.txt, adds them to tests/CMakeLists.txt, and changes the expected table in tests/checks/layer_check_test.cmake.

## Dependencies

- The layer table and its check: met.
- render's buildFoodOverlay, buildParkMesh, and buildGuestMesh, legible's foodAvailability and previewEdit, and sim's loadWorld, resolveWorld, copyWorld, hashWorld, stepWorld, and makeCandidate: met.
- parks/lowfps.park, untracked in the working tree: present, and it loads and steps with today's schema.

## Out of scope

- Summarizing launches, comparing reports, and the script that builds both presets and launches tpj_bench: runtime-report.
- The full park and the test that loads every stress park: full-park.
- Timing how often the app rebuilds a mesh over a run of ticks: the milestone's upkeep-over-ticks candidate.
- Timing drawing on the GPU: scalable-runtime's drawing-measured candidate.
- Choosing the preview's edit from what the player is drawing: the preview times the park's own resolution, which every edit pays.

## Open questions

None.
