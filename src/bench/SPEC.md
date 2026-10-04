# bench

The runtime runner (plans/scalable-runtime): headless executables that time each stage of the gameplay runtime on a park file, and summarize and compare those timings, so a slowdown is seen before a player feels it. It is a test utility, checked by running it. Timings are reported, never gated on.

## Boundary

The library tpj_bench_lib holds the stages, the summaries, and the reports. tpj_bench times a park, and tpj_bench_report summarizes and compares what tpj_bench and the app's --frame-times wrote (app/SPEC.md). It reads a world only through public headers, links the views, the renderer, the legible module, the scenarios library, and the sim, and builds only with the windowed application. It sits in a layer of its own above views and below app (decision 0027). It is the one reader of the clock that times runtime stages; simulation code never reads the clock (principle 10). scripts/runtime-report.sh drives both executables, and their command lines and line formats below are its contract.

## Stages

benchPark times, on a loaded park, the stages a player waits on: resolution, a preview of an edit that leaves the park as it is (only when the park has a box), the food overlay, the park mesh, the guest mesh, and a run of ticks with no commands, in that order. It leaves the loaded world unchanged. Each stage but ticks is warmed up and then repeated, and ticks times each of its calls. Each stage gives the median, least, and greatest of its times in nanoseconds, and a result computed from its work, a hash or a vertex count. So the work cannot be optimized away, and a build that did different work shows a different result.

## tpj_bench

tpj_bench [--ticks N] FILE writes `park <path> ticks <t> warm-ups <w> repetitions <r>` and then one line per stage: `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`, where the result name is hash, vertices, or frames. N defaults to DEFAULT_TICKS, two minutes of game time, so a launch measures sustained play. Output is written only once every stage has run.

tpj_bench --full-park PATH [--warm-ticks N] writes the full park (scenarios/SPEC.md) to PATH, with the same text on every build.

A bad command line prints a message and the usage line `usage: tpj_bench [--ticks N] FILE | --full-park PATH [--warm-ticks N]`. Any failure, including a file it cannot read or load or a stage that throws, writes `tpj_bench: <message>` to standard error, nothing to standard output, and exits with a nonzero status.

## tpj_bench_report

tpj_bench_report reads text in which `build <name>` precedes each launch, one launch being what one run of tpj_bench wrote. Launches, not the repetitions inside one, carry the variance that matters, so each launch counts as one sample of its stage medians.

- summarize LAUNCHES writes a report: for each build and park, each stage's median, least, and greatest over its launches' medians, and its worst single call in any launch, so a hitch survives summarizing. Launches of one build and park that timed different work, including a result that differs and so would mean the simulation is not deterministic, are refused.
- compare BEFORE AFTER writes, for each build, park, and stage, both sides' times in microseconds, the change in the median, and `faster` or `slower` only when every launch on one side lies strictly beyond every launch on the other, and otherwise `unclear`. It marks `result-changed` when the two timed different work, and ends with `clear <n> of <m>`.
- show REPORT writes each stage's times in microseconds and marks `over-budget` each stage whose median exceeds one tick, STAGE_BUDGET_NANOSECONDS, since any stage that takes longer stalls play. It ends with `over-budget <n> of <m>`.
- frames PARK OUTPUT turns the app's frame-times line into a launch with one stage, frames, whose result is the number of frames timed.

A report reads back as written. The columns of compare and show line up, so splitting a line at runs of spaces gives its fields. Malformed input names the line it fails at. Any failure writes `tpj_bench_report: <message>` to standard error, nothing to standard output, and exits with a nonzero status.

## The script

scripts/runtime-report.sh [--launches N] REPORT builds the runners with windows-release and windows-debug, and in each of N rounds, 10 by default, launches tpj_bench on every park in tests/parks/stress/ with windows-release, again with windows-debug stepping fewer ticks and skipping full.park, which debug cannot step in reasonable time, and the windows-release app on every park. Running in rounds lets drift widen every park's spread alike rather than moving one park's median. It writes the summarized report to REPORT, prints it against the budget, and says how long it took. scripts/runtime-report.sh --compare BEFORE AFTER prints the comparison of two reports. Either stops with a message and a nonzero status on bad arguments, a missing report, no stress parks, or a failed build, launch, or summary.

## Stress parks

tests/parks/stress/ holds the parks only the runtime report reads, each named for what it holds. winding-path.park is a single guest path about 3 km long with one shop. full.park is the full park: 2,000 guests, 30 shops, and 4 km of guest path, a minute into play.
