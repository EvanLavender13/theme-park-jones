# bench

The runtime runner (plans/scalable-runtime). Its headless executables time each stage of the gameplay runtime on a park file, then summarize and compare those timings. The aim is to see a slowdown before a player feels it. It is a test utility, checked by running it. Timings are reported, never gated on.

## Boundary

The library tpj_bench_lib holds the stages, the summaries, and the reports. tpj_bench times a park. tpj_bench_report summarizes and compares what tpj_bench and the app's --frame-times wrote (app/SPEC.md).

It reads a world only through public headers. It links the views, the renderer, the legible module, the scenarios library, and the sim, and it builds only with the windowed application. It sits in a layer of its own, above views and below app (decision 0027). It is the only reader of the clock that times runtime stages; simulation code never reads the clock (principle 10).

scripts/runtime-report.sh drives both executables. Their command lines and line formats below are its contract.

## Stages

benchPark times the stages a player waits on, on a loaded park, in this order:

- resolution
- a preview of an edit that leaves the park as it is (only when the park has a box)
- the food overlay
- the park mesh
- the guest mesh
- a run of ticks with no commands

It leaves the loaded world unchanged. Each stage except ticks is warmed up and then repeated. Ticks times each of its calls. Each stage gives the median, least, and greatest of its times in nanoseconds. It also gives a result computed from its work, either a hash or a vertex count. The result keeps the work from being optimized away, and a build that did different work shows a different result.

## tpj_bench

tpj_bench [--ticks N] FILE writes `park <path> ticks <t> warm-ups <w> repetitions <r>`, then one line per stage: `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`. The result name is hash, vertices, or frames. N defaults to DEFAULT_TICKS, which is two minutes of game time, so a launch measures sustained play. Output is written only after every stage has run.

tpj_bench --full-park PATH [--warm-ticks N] writes the full park (scenarios/SPEC.md) to PATH. The text is the same on every build.

A bad command line prints a message and the usage line `usage: tpj_bench [--ticks N] FILE | --full-park PATH [--warm-ticks N]`. Any failure writes `tpj_bench: <message>` to standard error, writes nothing to standard output, and exits with a nonzero status. Failures include a file it cannot read or load and a stage that throws.

## tpj_bench_report

tpj_bench_report reads text in which `build <name>` comes before each launch. A launch is what one run of tpj_bench wrote. The variance that matters is between launches, not between the repetitions inside one launch, so each launch counts as one sample of its stage medians.

- summarize LAUNCHES writes a report. For each build and park, it gives each stage's median, least, and greatest over its launches' medians. It also gives the stage's worst single call in any launch, so a hitch survives summarizing. It refuses launches of one build and park that timed different work. That includes a result that differs, which would mean the simulation is not deterministic.
- compare BEFORE AFTER writes, for each build, park, and stage, both sides' times in microseconds and the change in the median. It writes `faster` or `slower` only when every launch on one side lies strictly beyond every launch on the other, and `unclear` otherwise. It marks `result-changed` when the two sides timed different work. It ends with `clear <n> of <m>`.
- show REPORT writes each stage's times in microseconds. It marks `over-budget` each stage whose median exceeds one tick, STAGE_BUDGET_NANOSECONDS, because a stage that takes longer stalls play. It ends with `over-budget <n> of <m>`.
- frames PARK OUTPUT turns the app's frame-times line into a launch with one stage, frames. The stage's result is the number of frames timed.

A report reads back as written. The columns of compare and show line up, so splitting a line at runs of spaces gives its fields. Malformed input names the line it fails at. Any failure writes `tpj_bench_report: <message>` to standard error, writes nothing to standard output, and exits with a nonzero status.

## The script

scripts/runtime-report.sh [--launches N] REPORT builds the runners with windows-release and windows-debug. It then runs N rounds, 10 by default. Each round launches:

- tpj_bench on every park in tests/parks/stress/ with windows-release
- tpj_bench again with windows-debug, stepping fewer ticks and skipping full.park, which debug cannot step in reasonable time
- the windows-release app on every park, first without and then with the food overlay shown, so the report shows what rebuilding the overlay at every tick costs a player who shows it

Running in rounds lets drift widen every park's spread alike, rather than moving one park's median. The script writes the summarized report to REPORT, prints it against the budget, and says how long it took.

scripts/runtime-report.sh --compare BEFORE AFTER prints the comparison of two reports. Either form stops with a message and a nonzero status on bad arguments, a missing report, no stress parks, or a failed build, launch, or summary.

## Stress parks

tests/parks/stress/ holds the parks that only the runtime report reads. Each is named for what it holds. winding-path.park is a single guest path about 3 km long with one shop. full.park is the full park: 2,000 guests, 30 shops, and 4 km of guest path, a minute into play.
