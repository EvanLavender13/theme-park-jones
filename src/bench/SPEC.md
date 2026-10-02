# bench

The runtime runner (plans/scalable-runtime): a headless executable that times each stage of the gameplay runtime on a park file and writes each stage's result beside its times. The library tpj_bench_lib holds the stages, the summary of times, and the options. It links tpj_views, tpj_render, tpj_legible, and tpj_sim, and builds only with the windowed application, as render does. It reads a world only through public headers. It is the one reader of the clock that times runtime stages; it is not simulation code, and simulation code never reads the clock (principle 10). It is in a layer of its own above views and below app, so it includes nothing of app (decision 0027). No test checks a time.

## Times

A stage's times are durations in nanoseconds, each read from std::chrono::steady_clock immediately before and after one call of its work. summarizeTimes(durations) gives a TimeSummary: Count, the number of durations; Median, the element at index (Count - 1) / 2 once they are sorted ascending, so of an even count it is the lower of the two middle ones; Least; and Greatest. It throws std::invalid_argument for an empty list.

## Stages

benchPark(loaded, ticks) takes a loaded world and a count of ticks, changes nothing in the loaded world, and gives a list of StageResult, each a Name, the TimeSummary of its counted calls, a ResultKind, hash or vertices, and a Result, in this order. It throws std::invalid_argument when ticks is 0.

- resolution: each call resolves a copy of the loaded world with resolveWorld, the copy made before the clock is read. Its result is the hash of the resolved world, which the rest call the resolved park.
- preview, present only when parkBoxes of the resolved park is not empty: each call is previewEdit of the resolved park and MoveBox of the lowest-keyed box to that box's own pose, an edit accepted on a physically valid park that leaves its networks as they are, so the preview costs what the park itself costs to resolve. Its result is the hash of the preview's candidate. When previewEdit gives no candidate, benchPark throws std::runtime_error naming the box by its key in decimal.
- food-overlay: each call is buildFoodAvailabilityOverlay of the resolved park (views/SPEC.md). Its result is the mesh's vertex count.
- park-mesh: each call is buildParkMesh of the resolved park. Its result is the mesh's vertex count.
- guest-mesh: each call is buildGuestMesh of the resolved park. Its result is the mesh's vertex count.
- ticks: from a copy of the resolved park, each call is one stepWorld with no commands, ticks calls in sequence, so no timed tick includes the loaded park's first resolution. Its result is the hash of the copy after them, which equals the resolved park's after as many cycles with nothing timed.

Every stage but ticks first makes WARM_UPS calls, 2, untimed, and then REPETITIONS calls, 11, whose times it summarizes. Ticks has no warm-up, since each tick changes the world it steps, and summarizes all of its calls. Each call's output is kept until after the clock is read, so freeing it is never timed, and the result is computed from the last call's output after the clock is last read, and written out, so the work cannot be dropped as unused, and a build that timed less work would show a different result.

## Command line

tpj_bench [--ticks N] FILE reads FILE, loads it with makeParkSchema, and writes to standard output `park <path> ticks <t> warm-ups <w> repetitions <r>`, then one line per stage of benchPark of the loaded world and t: `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`. The path is FILE as given, t the ticks, w WARM_UPS, and r REPETITIONS, in decimal; times are nanoseconds in decimal; the result name is hash for resolution, preview, and ticks, with the result as 16 lowercase hexadecimal digits, and vertices for the meshes, with the result in decimal. Every line ends with a line feed, and standard output is in the C runtime's text mode, so on Windows each arrives as a carriage return and line feed. The lines are written only once benchPark has returned. N defaults to DEFAULT_TICKS, 300. It exits with status 0.

parseBenchOptions(arguments, error), whose first argument is the program's name, reads each later argument that begins with -- as an option and any other as the file. It gives BenchOptions, the Ticks and the Park path, or none with error set to a message naming the problem: an unknown option, named in the message; --ticks with no value, or one that is not a positive decimal count, which a count too large for 64 bits is not; no file; or more than one. A later --ticks replaces an earlier one. tpj_bench then writes the message and the usage line `usage: tpj_bench [--ticks N] FILE` to standard error and exits with a nonzero status. readParkFile(path) gives the file's text, or none when it cannot be read, and tpj_bench then writes `tpj_bench: cannot read <path>` to standard error and exits with a nonzero status; when loadWorld throws LoadError, it writes `tpj_bench: cannot load <path>: <message>`, with LoadError's message, and exits with a nonzero status. When benchPark throws, it writes `tpj_bench: <what>` to standard error and exits with a nonzero status. None of these writes to standard output.

Stress parks live in tests/parks/stress/, outside the tests/parks/*.park files that the cross-build check and the integration tests read. Each is named for what it holds. winding-path.park is a single guest path about 3 km long with one shop, the park on which the food overlay first played slowly.
