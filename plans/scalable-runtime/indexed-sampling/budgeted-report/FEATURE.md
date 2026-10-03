# Feature: Budgeted Report

## Summary

The runtime report measures play rather than a few seconds of it, and says plainly what is over budget. tpj_bench steps 3,600 ticks by default, two minutes of game time, and windows-debug launches step 300. A report keeps, for every stage, the worst call of any launch beside the median of the launches' medians. A new tpj_bench_report show command prints one report in microseconds with its columns aligned and marks every stage whose median exceeds the 33 ms a tick covers, and the script prints it at the end of every report. The app gains --frame-times, which writes each frame's cost, the time from its start to the next frame's, read from the frame clock's own performance counter readings, leaving out the first frame, which follows loading and does one-off work; the script launches the windowed app on every stress park with windows-release in every round, and tpj_bench_report turns its output into a launch, so the app's frames are summarized, compared, and marked like any stage. The feature ends with the milestone's before report.

## Acceptance criteria

The app's parts are tested; tpj_bench and tpj_bench_report are test utilities, checked by running them.

1. Each FrameStep's Nanoseconds is the whole nanoseconds between the reading advance was given and the previous reading, rounded down, and is never clamped: a frame whose Dt MAX_FRAME_SECONDS clamps still gives its full Nanoseconds. (test)
2. A FrameTimes keeps the values it is given from the third on, in the order given, and drops the first two: the first advance covers no frame, and the second covers the first frame. Its line is `frame-times`, followed by each kept value in decimal, in order, each after a single space, with no line feed. (test)
3. parseOptions sets FrameTimes exactly when --frame-times is given with a --frames value that is positive. --frame-times without --frames, or with a --frames value that is not positive, is refused like any invalid command line. (test)
4. `build/windows-release/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --frames 5 --frame-times` exits with status 0 and writes to standard output exactly one line beginning `frame-times`, holding 3 durations. (check)
5. `build/windows-release/tpj_bench.exe tests/parks/stress/winding-path.park` writes a park line with `ticks 3600` and a ticks stage with `count 3600`. (check)
6. `scripts/runtime-report.sh --launches 2 build/runtime-report/check.txt` writes a report holding, for each stress park, windows-release with every stage, windows-debug for every park but full.park, with ticks 300, and windows-release-app with one stage, frames, whose result is `frames 298`. Every stage line of the report ends with `worst <w>`, and w is at least the stage's greatest. The script ends by printing tpj_bench_report show of the report, then the `wrote` line. (check)
7. In that show output, every stage whose median exceeds 33,333,333 ns ends with `over-budget`, no other does, and the last line is `over-budget <n> of <m>`, m counting the stages and n those marked. (check)
8. `scripts/runtime-report.sh --compare build/runtime-report/check.txt build/runtime-report/check.txt` prints `clear 0 of <m>`, and compares the windows-release-app frames stages like any other. (check)

## Medium

None. The feature introduces, samples, and emits no fields or flows, and reads the world only through the public headers it already uses.

## Principle checks

- Principle 10: the app reads the performance counter only where the frame clock already does, and the frame times are kept by the app alone, and frame timing never reaches the simulation: Nanoseconds is computed from the readings advance is given and changes neither Dt nor Ticks (criterion 1 checks Dt still clamps). tpj_bench and tpj_bench_report stay the only other readers of time, and simulation code never reads it.
- Principle 1: the report derives everything it prints from the launches; show and the frames conversion keep no state.

Principles 2 to 6 and 8 are not touched: nothing in the simulation, the medium, or previews changes.

## Spec changes

src/app/SPEC.md, the paragraph beginning "main.cpp only composes": after "carrying the part of a tick left over to the next frame and dropping the whole ticks beyond the cap.", add: "Each FrameStep also gives Nanoseconds, the whole nanoseconds between the reading and the previous one, rounded down and never clamped, so a frame's full cost is known even when Dt is clamped."

src/app/SPEC.md, Command line, after the --frames and --capture paragraph, add: "--frame-times, given with a positive --frames, writes to standard output, once the last frame is drawn, the line of a FrameTimes (frame_times.h) given each frame's Nanoseconds as the frame starts, and a line feed. A frame's advance gives the time since the previous frame's, so it is the previous frame's whole cost. FrameTimes drops the first two values, the first covering no frame and the second the first frame, which follows loading and builds the park mesh and frames the camera once, and keeps the rest in order. Its line is `frame-times` followed by each kept value in decimal after a single space. So N frames give N - 2 durations, the costs of frames 2 to N - 1, and neither loading nor the first frame's one-off work is in any of them. It is how scripts/runtime-report.sh times the app (src/bench/SPEC.md)."

src/app/SPEC.md, the invalid command line paragraph: "An unknown option, ..., an --overlay value other than food, --frame-times without a --frames value that is positive, or --hash given with ..." (the new case inserted before --hash).

src/bench/SPEC.md:

- Stages: ResultKind gains Frames, a count of frames, written `frames <n>` in decimal, and the sentence listing a StageResult's fields says hash, vertices, or frames. benchPark never gives Frames; tpj_bench_report frames does.
- Command line: "N defaults to DEFAULT_TICKS, 3600, two minutes of game time, so a launch measures sustained play rather than seconds of it."
- Reading lines: a stage line's result name may also be frames, with a decimal count. A report's stage line is a stage line followed by `worst <w>`, a time.
- Summarizing: each report stage also has Worst, the greatest of the launches' Greatest for that stage: the slowest single call of any launch, so a hitch survives summarizing even when the medians hide it.
- Report text: each stage is written as stageLine writes it followed by ` worst <w>`. parseReport reads it back, and still gives parseReport(reportText(report)) equal to the report.
- Comparing: unchanged, but a StageComparison's Before and After are report stages, each with its Worst, said where the section names them; comparisonText writes what it wrote before.
- A new section, Showing: showText(report) writes one line per stage of each park, in the report's order, then `over-budget <n> of <m>`, every line ending with a line feed. A stage's fields are the build, park, and stage; its median, `[` joined to its least, and its greatest joined to `]`, in microseconds as comparisonText writes times; `worst` and the worst in microseconds; and `over-budget` when the median exceeds STAGE_BUDGET_NANOSECONDS, 33,333,333, the whole nanoseconds of SIM_TICK_SECONDS, which every stage is held to since any single stage that takes longer than one tick's time stalls play. m counts the stages and n those marked. Columns are aligned as comparisonText aligns them, on every line but the last.
- A new section, Frames: framesLaunch(park, text) reads the app's output: its lines, as parseLaunches reads lines, must hold exactly one line whose first field is frame-times, and every other field of that line a time; at least one is required. It gives the launch text `park <park> ticks 0 warm-ups 1 repetitions <k>` and `stage frames count <k> median <m> least <l> greatest <g> frames <k>`, each ending with a line feed, k being the number of durations and the times their summarizeTimes. A frames launch steps no fixed ticks, so its ticks are 0, and its first frame, which follows loading, is its one warm-up. It throws ReportError otherwise.
- Command line: tpj_bench_report show REPORT writes showText(parseReport(text)). tpj_bench_report frames PARK OUTPUT writes framesLaunch(PARK, the text of OUTPUT); PARK is a path written as given, not a file read. parseReportOptions accepts show with one file and frames with two arguments, its Command being summarize, compare, show, or frames, the Report section says tpj_bench_report also shows a report and turns the app's frame times into a launch, and the usage line becomes `usage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER | show REPORT | frames PARK OUTPUT`.
- The script: it also builds tpj_app with windows-release. windows-debug launches pass --ticks 300. In each round, after the windows-debug launches, it launches build/windows-release/ThemeParkJones.exe --park <park> --frames APP_FRAMES --frame-times on every stress park, APP_FRAMES being 300, and appends `build windows-release-app` and tpj_bench_report frames of the park and the app's output to the launches. A failed app launch or conversion stops the script, naming the park and round. After writing the report, it prints tpj_bench_report show of it, then the `wrote` line.

src/scenarios/SPEC.md: "A guest the generator adds stays 20 minutes past the save, 10 times tpj_bench's default run, ..." replacing "120 times".

## Files affected

- Create: src/app/frame_times.h, src/app/frame_times.cpp
- Modify: src/app/frame_clock.h, src/app/frame_clock.cpp, src/app/CMakeLists.txt, src/app/options.h, src/app/options.cpp, src/app/application.h, src/app/application.cpp, src/app/SPEC.md
- Modify: src/bench/stages.h, src/bench/stages.cpp, src/bench/SPEC.md
- Modify: src/bench/report/internal/lines.cpp, src/bench/report/report.h, src/bench/report/report.cpp, src/bench/report/comparison.h, src/bench/report/comparison.cpp, src/bench/report/report_options.h, src/bench/report/report_options.cpp, src/bench/report/main.cpp
- Create: src/bench/report/internal/columns.h, src/bench/report/internal/columns.cpp, src/bench/report/show.h, src/bench/report/show.cpp, src/bench/report/frames.h, src/bench/report/frames.cpp
- Modify: src/bench/CMakeLists.txt, scripts/runtime-report.sh, src/scenarios/SPEC.md
- Tests (test pass): tests/app/frame_clock_test.cpp, tests/app/frame_times_test.cpp, tests/app/options_test.cpp

## Dependencies

- The frame clock's tick cap (66d00c6), so the app's frames on the full park finish in reasonable time.
- measured-runtime's tpj_bench, tpj_bench_report, and scripts/runtime-report.sh.

## Out of scope

- Marking stages over budget in comparisonText: a comparison is read with show of its after report.
- A budget other than one tick's time for any stage, such as 16.7 ms for a 60 FPS frame.
- Timing the app on windows-debug.
- Timing a frame's parts inside the app; the headless stages already time them one by one.

## Open questions

None.
