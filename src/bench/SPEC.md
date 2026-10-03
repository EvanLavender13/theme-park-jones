# bench

The runtime runner (plans/scalable-runtime): a headless executable that times each stage of the gameplay runtime on a park file and writes each stage's result beside its times. The library tpj_bench_lib holds the runner's stages, the summary of times, and the options, and in report/ the reading, summarizing, and comparing of launches and reports. Two executables use it: tpj_bench times a park, and tpj_bench_report summarizes and compares what tpj_bench wrote. It links tpj_views, tpj_render, tpj_legible, tpj_scenarios_lib, and tpj_sim, and builds only with the windowed application, as render does. It reads a world only through public headers. It is the one reader of the clock that times runtime stages; it is not simulation code, and simulation code never reads the clock (principle 10). It is in a layer of its own above views and below app, so it includes nothing of app (decision 0027). It is a test utility: it is checked by running it, and has no tests.

## Times

A stage's times are durations in nanoseconds, each read from std::chrono::steady_clock immediately before and after one call of its work. summarizeTimes(durations) gives a TimeSummary: Count, the number of durations; Median, the element at index (Count - 1) / 2 once they are sorted ascending, so of an even count it is the lower of the two middle ones; Least; and Greatest. It throws std::invalid_argument for an empty list.

## Stages

benchPark(loaded, ticks) takes a loaded world and a count of ticks, changes nothing in the loaded world, and gives a list of StageResult, each a Name, the TimeSummary of its counted calls, a ResultKind, hash, vertices, or frames, and a Result, in this order. benchPark never gives frames, a count of frames; tpj_bench_report frames does (Frames). It throws std::invalid_argument when ticks is 0.

- resolution: each call resolves a copy of the loaded world with resolveWorld, the copy made before the clock is read. Its result is the hash of the resolved world, which the rest call the resolved park.
- preview, present only when parkBoxes of the resolved park is not empty: each call is previewEdit of the resolved park and MoveBox of the lowest-keyed box to that box's own pose, an edit accepted on a physically valid park that leaves its networks as they are, so the preview costs what the park itself costs to resolve. Its result is the hash of the preview's candidate. When previewEdit gives no candidate, benchPark throws std::runtime_error naming the box by its key in decimal.
- food-overlay: each call is buildFoodAvailabilityOverlay of the resolved park (views/SPEC.md). Its result is the mesh's vertex count.
- park-mesh: each call is buildParkMesh of the resolved park. Its result is the mesh's vertex count.
- guest-mesh: each call is buildGuestMesh of the resolved park. Its result is the mesh's vertex count.
- ticks: from a copy of the resolved park, each call is one stepWorld with no commands, ticks calls in sequence, so no timed tick includes the loaded park's first resolution. Its result is the hash of the copy after them, which equals the resolved park's after as many cycles with nothing timed.

Every stage but ticks first makes WARM_UPS calls, 2, untimed, and then REPETITIONS calls, 11, whose times it summarizes. Ticks has no warm-up, since each tick changes the world it steps, and summarizes all of its calls. Each call's output is kept until after the clock is read, so freeing it is never timed, and the result is computed from the last call's output after the clock is last read, and written out, so the work cannot be dropped as unused, and a build that timed less work would show a different result.

## Command line

tpj_bench [--ticks N] FILE reads FILE, loads it with makeParkSchema, and writes to standard output `park <path> ticks <t> warm-ups <w> repetitions <r>`, then one line per stage of benchPark of the loaded world and t: `stage <name> count <c> median <m> least <l> greatest <g> <result-name> <result>`. The path is FILE as given, t the ticks, w WARM_UPS, and r REPETITIONS, in decimal; times are nanoseconds in decimal; the result name is hash for resolution, preview, and ticks, with the result as 16 lowercase hexadecimal digits, vertices for the meshes, and frames for a Frames result, each of those two in decimal. Every line ends with a line feed, and standard output is in the C runtime's text mode, so on Windows each arrives as a carriage return and line feed. The lines are written only once benchPark has returned. N defaults to DEFAULT_TICKS, 3600, two minutes of game time, so a launch measures sustained play rather than seconds of it. It exits with status 0.

tpj_bench --full-park PATH [--warm-ticks N] writes saveWorld of makeFullPark(N) (scenarios/SPEC.md) to PATH, N being FULL_PARK_WARM_TICKS without --warm-ticks, with writeTextFile, which writes in binary mode, so every line ends with a line feed on every build. It writes nothing to standard output. It exits with status 0 once the file is written, or with a nonzero status after writing to standard error `tpj_bench: cannot write <path>` when the file cannot be written, and `tpj_bench: <what>` when makeFullPark throws.

parseBenchOptions(arguments, error), whose first argument is the program's name, reads each later argument that begins with -- as an option and any other as the file. It gives BenchOptions, the Ticks, the Park path, FullPark, the path --full-park names or empty, and WarmTicks, FULL_PARK_WARM_TICKS or the count --warm-ticks gives, or none with error set to a message naming the problem: an unknown option, named in the message; --ticks with no value, or one that is not a positive decimal count, which a count too large for 64 bits is not; --full-park with no value, with a file, or with --ticks; --warm-ticks with no value, one that is not a positive decimal count, or without --full-park; no file without --full-park; or more than one. A later --ticks, --warm-ticks, or --full-park replaces an earlier one. tpj_bench then writes the message and the usage line `usage: tpj_bench [--ticks N] FILE | --full-park PATH [--warm-ticks N]` to standard error and exits with a nonzero status. readTextFile(path) gives the file's text, read in binary mode, or none when it cannot be read, and tpj_bench then writes `tpj_bench: cannot read <path>` to standard error and exits with a nonzero status; when loadWorld throws LoadError, it writes `tpj_bench: cannot load <path>: <message>`, with LoadError's message, and exits with a nonzero status. When benchPark throws, it writes `tpj_bench: <what>` to standard error and exits with a nonzero status. None of these writes to standard output.

## Report

scripts/runtime-report.sh launches tpj_bench several times on every stress park with windows-release, and on every one but full.park with windows-debug, which cannot step the full park in reasonable time, and launches the app on every stress park with windows-release. tpj_bench_report turns the app's frame times into launches, summarizes the launches into a report, shows a report against the budget, and compares two reports. Launches, not the repetitions inside one, carry the variance that matters, so each launch is summarized by its stage medians, and launches are the samples a report and a comparison treat as independent (plans/scalable-runtime/measured-runtime/RESEARCH.md). tpj_bench_report reads no clock. Its arithmetic is on the times tpj_bench and the app wrote. Release decides what is slow, and debug is reported beside it. Nothing checks a time: a report and a comparison are read, never gated on.

### Reading lines

parseLaunches and parseReport read text as lines separated by line feeds. A carriage return at the end of a line is removed, and a line that is then empty is skipped. A line's fields are separated by single spaces, and none is empty. Counts, ticks, and times are unsigned decimal integers that fit their types, and a hash is 16 lowercase hexadecimal digits. Each reader throws ReportError, a std::runtime_error, at the first line that breaks its form. The message begins `line <n>: `, where n counts every line from 1, skipped ones included. A stage line has the form stageLine writes: `stage <name> count <c> median <m> least <l> greatest <g> hash <h>`, `... vertices <v>`, or `... frames <n>`. A report's stage line is a stage line followed by `worst <w>`, a time.

### Launches

A launch is what one run of tpj_bench wrote, after a line naming the build that ran it. It is `build <name>`, then tpj_bench's park line, `park <path> ticks <t> warm-ups <w> repetitions <r>`, then any number of stage lines. parseLaunches(text) gives the launches in order. Each is a Launch: Build, Park, Ticks, WarmUps, Repetitions, and Stages, a StageResult per stage line. A build line must be followed by a park line: any other line there is refused, and text ending right after a build line is refused at the build line. A park line anywhere else is refused, and so are a stage line before the first launch and a stage line that repeats a stage name within its launch. Text with no lines gives no launches.

### Summarizing

summarizeLaunches(launches) gives a report: one ReportPark per build and park among the launches, in the order each pair first appears. Each holds Build and Park; Launches, the number of its launches; their Ticks, WarmUps, and Repetitions; and one StageResult per stage of its launches, in their order. Each stage has its Kind and Result, and as Times the summarizeTimes of the stage's Median in each of the launches. So a report stage's Median is the median of the launches' medians, the lower of the middle two for an even count, as summarizeTimes takes it. Its Least and Greatest are the launches' spread, and its Count is the number of launches. Each report stage also has Worst, the greatest of the launches' Greatest for the stage: the slowest single call of any launch, so a hitch survives summarizing even where the medians hide it. Launches of one build and park must have done the same work. They must have the same Ticks, WarmUps, and Repetitions, the same stage names in the same order, and the same Kind and Result in each stage. Otherwise summarizeLaunches throws ReportError naming the build and park, and the stage when one stage's result differs. A result that differs between launches of one build would mean the simulation is not deterministic (principle 10), so it is refused rather than summarized. No launches give an empty report.

### Report text

reportText(report) writes each ReportPark as `park <build> <path> launches <n> ticks <t> warm-ups <w> repetitions <r>`, followed by its stages, each as stageLine writes it followed by ` worst <w>`, every line ending with a line feed. parseReport(text) reads that form back, so parseReport(reportText(report)) equals the report. A stage line before the first park line is refused. So is a park line that repeats a build and path already read, and a stage line that repeats a stage name within its park.

### Comparing

compareReports(before, after) gives one StageComparison for each build, park, and stage name found in either report. Those of before come first, in its order, and then those found only in after, in after's order. Each holds Build, Park, and Stage, and Before and After, the stage as each report holds it, with its Worst, which the comparison text does not write, either one empty when the stage is not in that report. When both are present:

- Change is Faster when After's Greatest is less than Before's Least, Slower when After's Least is greater than Before's Greatest, and Unclear otherwise. A change is clear only when every launch of one report lies strictly beyond every launch of the other. With ten launches on each side of an unchanged stage, the chance of that is 2 in 184,756.
- ResultChanged is true when their Kind or Result differs, which means the two reports timed different work.

When either is empty, Change is Unclear and ResultChanged is false.

### Comparison text

comparisonText(comparisons) writes one line per comparison, then `clear <n> of <m>`, every line ending with a line feed. m counts the comparisons with both stages, and n those of them whose Change is not Unclear. The fields of a comparison with both stages are, in order:

1. the build, park, and stage;
2. `faster`, `slower`, or `unclear`;
3. `before`, then Before's median, `[` joined to its least, and its greatest joined to `]`;
4. `after`, then After's three times in the same form;
5. the change;
6. `result-changed`, only when ResultChanged.

One with a single stage has the build, park, and stage, then `only-before` or `only-after`, then that stage's three times in the same form. Times are in microseconds: the nanoseconds divided by 1,000, written with one decimal place as std::format's `{:.1f}` writes them. The change is (After's median − Before's median) / Before's median × 100, written as `{:+.1f}` writes it, followed by `%`, or `n/a` when Before's median is 0. On every line but the last, each field but the line's last is followed by enough spaces to reach the width of the widest field in that position on any of those lines, and then one more. So the columns line up, and splitting a line at runs of spaces gives its fields. For example:

    windows-release tests/parks/stress/winding-path.park food-overlay faster  before 2240.4 [2198.3 2301.9] after 512.7 [498.1 530.2] -77.1%
    windows-release tests/parks/stress/winding-path.park ticks        unclear before 34.1   [33.8   34.9]   after 34.0  [33.6  35.0]  -0.3%
    clear 1 of 2

### Showing

showText(report) writes one line per stage of each park, in the report's order, then `over-budget <n> of <m>`, every line ending with a line feed. A stage's fields are, in order: the build, park, and stage; its median, `[` joined to its least, and its greatest joined to `]`, in microseconds as comparisonText writes times; `worst`, then the worst in microseconds; and `over-budget` when the median exceeds STAGE_BUDGET_NANOSECONDS, 33,333,333, the whole nanoseconds of SIM_TICK_SECONDS. Every stage is held to one tick's time, since any single stage that takes longer stalls play. m counts the stages and n those marked. Columns are aligned as comparisonText aligns them, on every line but the last.

### Frames

The app's --frame-times line (app/SPEC.md) becomes a launch through framesLaunch(park, text). Of text's lines, read as parseLaunches reads lines, exactly one must have frame-times as its first field, with at least one more field, each a time. framesLaunch gives the text `park <park> ticks 0 warm-ups 1 repetitions <k>` and `stage frames count <k> median <m> least <l> greatest <g> frames <k>`, each ending with a line feed, with k the number of durations and the times their summarizeTimes. An app launch steps no fixed number of ticks, so its ticks are 0, and its first frame, which follows loading, is its one warm-up. framesLaunch throws ReportError when text has no frame-times line, more than one, or one with no durations or a field that is not a time.

### Command line

tpj_bench_report summarize LAUNCHES reads the file LAUNCHES and writes reportText(summarizeLaunches(parseLaunches(text))) to standard output. tpj_bench_report compare BEFORE AFTER reads both files with parseReport and writes comparisonText(compareReports(before, after)). tpj_bench_report show REPORT reads REPORT with parseReport and writes showText of it. tpj_bench_report frames PARK OUTPUT reads the file OUTPUT and writes framesLaunch(PARK, its text); PARK is written as given, not read. parseReportOptions(arguments, error), whose first argument is the program's name, gives ReportOptions, a Command, summarize, compare, show, or frames, and its Paths. Otherwise it gives none, with error set to a message naming the problem: no command, an unknown command, which the message names, or a count of arguments other than one for summarize and show and two for compare and frames.

Standard output is in the C runtime's text mode. tpj_bench_report writes it only once all of it is made, and writes nothing there on failure. Each failure exits with a nonzero status, after writing to standard error:

- options parseReportOptions refuses: `tpj_bench_report: <message>` and the usage line `usage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER | show REPORT | frames PARK OUTPUT`;
- a file readTextFile cannot read: `tpj_bench_report: cannot read <path>`;
- a ReportError while reading or summarizing a file: `tpj_bench_report: <path>: <message>`;
- any other exception, which only a failure to allocate can cause: `tpj_bench_report: <what>`.

### The script

scripts/runtime-report.sh [--launches N] REPORT runs from WSL. It builds tpj_bench, tpj_bench_report, and tpj_app with windows-release and tpj_bench with windows-debug, first configuring a preset whose build directory holds no CMakeCache.txt. It launches tpj_bench on every file matching tests/parks/stress/*.park, in name order, N times on each build, 10 by default, with no options on windows-release and with --ticks 300 on windows-debug, the same in every report, except that windows-debug skips tests/parks/stress/full.park, which it cannot step in reasonable time. It runs the launches in rounds: in each round, every park runs once with windows-release, then every park but full.park once with windows-debug, and then the app, build/windows-release/ThemeParkJones.exe --park <park> --frames 300 --frame-times, on every park. Each app launch's output goes through tpj_bench_report frames of the park, and is appended after `build windows-release-app`. So each park's launches span the whole report, and any drift during it widens the spread rather than moving one park's median. Each launch's output goes to build/runtime-report/launches.txt after `build <preset>`, with carriage returns removed. Once the last launch is done, tpj_bench_report summarize of that file writes the report, without carriage returns, to REPORT, through REPORT.partial renamed into place. The script then prints tpj_bench_report show of REPORT, and ends by printing `runtime-report: wrote <REPORT> in <m>m <ss>s`, the time since it started, builds included. scripts/runtime-report.sh --compare BEFORE AFTER builds tpj_bench_report with windows-release and prints its comparison of the two reports, handing it their paths through wslpath -w. Either stops with a message and a nonzero status when:

- the arguments are not one of these forms;
- N is not a positive integer;
- a report to compare is missing;
- there is no stress park;
- a build fails;
- a launch fails, naming its build, park, and round, the app's and its conversion's included;
- summarizing fails.

Stress parks live in tests/parks/stress/, outside the tests/parks/*.park files that the cross-build check and the integration tests read. Each is named for what it holds. winding-path.park is a single guest path about 3 km long with one shop, the park on which the food overlay first played slowly. full.park is the full park, the save tpj_bench --full-park writes of makeFullPark (scenarios/SPEC.md): 2,000 guests, 30 shops, and 4 km of guest path, a minute into play. Only windows-release times it.
