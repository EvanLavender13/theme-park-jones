# Feature: Runtime Report

## Summary

runtime-report turns tpj_bench's single launches into a report and tells a real change from noise. A new tool, tpj_bench_report, in src/bench/report/, reads a stream of launches, each tpj_bench's output after a line naming the build that ran it. It summarizes the launches of each build and park into the median of their medians, with the least and greatest as the spread. It refuses launches of one build and park whose results differ, since they did not do the same work. It writes the report as text and reads it back, and it compares two reports. A stage's change is clear only when every launch of one report lies strictly beyond every launch of the other. The comparison prints times in microseconds and ends with how many changes were clear. scripts/runtime-report.sh builds tpj_bench with windows-release and windows-debug. It launches tpj_bench ten times by default on every stress park with each build, in rounds, writes the report, and prints how long it took. It also compares two reports. It is a test utility: it has no tests, and every criterion is checked by running it. Nothing checks a timing.

## Acceptance criteria

tpj_bench_report and the script are test utilities. They have no tests. The implementer checks each criterion by running them, and the feature's report gives the output.

1. Two reports of the unchanged tree, made by `scripts/runtime-report.sh`, each give a park line for every stress park on each build with `launches 10`, and stage lines carrying the results tpj_bench wrote. Each stage's results are the same on both builds. Their comparison by `scripts/runtime-report.sh --compare` ends with `clear 0 of <m>`, with m above 0.
2. In the first report, one stage of one park and build has as its median the lower middle of that stage's ten launch medians in build/runtime-report/launches.txt once sorted, and as its least and greatest their smallest and largest.
3. Compare the first report against a copy with one stage's three times edited so they all lie below that stage's least. The comparison marks that stage faster, marks every other stage unclear, and ends with `clear 1 of <m>`. Edited instead to overlap, the stage is unclear. With its result edited, the stage is marked result-changed.
4. Edit one launch's result in a copy of build/runtime-report/launches.txt. `tpj_bench_report summarize` of the copy then exits with a nonzero status and a message naming the build and the park, and writes nothing to standard output.
5. The script refuses `--launches 0` and a missing report to compare, each with a message and a nonzero status. A report run ends by printing how long it took.

## Medium

This feature introduces, samples, and emits no fields or flows. tpj_bench_report reads no world: its inputs are the lines tpj_bench writes.

## Principle checks

- Principle 10: the simulation is deterministic. summarizeLaunches refuses launches of one build and park whose results differ (criterion 4). If a run were nondeterministic, its hash would differ between launches, and the report would refuse it rather than summarize it. tpj_bench_report reads no clock. Every number it reports is one tpj_bench wrote.
- No timing gates: nothing fails on a time. Reports and comparisons are read, never gated on.
- Principle 6: tpj_bench_report includes only bench's own headers. The private header check covers report/internal/.

No other principle applies: the feature touches no world, field, flow, or view.

## Spec changes

src/bench/SPEC.md, the first paragraph's sentence "The library tpj_bench_lib holds the stages, the summary of times, and the options." becomes:

> The library tpj_bench_lib holds the runner's stages, the summary of times, and the options, and in report/ the reading, summarizing, and comparing of launches and reports. Two executables use it: tpj_bench times a park, and tpj_bench_report summarizes and compares what tpj_bench wrote.

src/bench/SPEC.md, Command line: "readParkFile(path) gives the file's text" becomes "readTextFile(path) gives the file's text, read in binary mode,".

src/bench/SPEC.md, a new section after Command line:

> ## Report
>
> scripts/runtime-report.sh launches tpj_bench several times on every stress park with windows-release and windows-debug. tpj_bench_report summarizes the launches into a report and compares two reports. Launches, not the repetitions inside one, carry the variance that matters, so each launch is summarized by its stage medians, and launches are the samples a report and a comparison treat as independent (plans/scalable-runtime/measured-runtime/RESEARCH.md). tpj_bench_report reads no clock. Its arithmetic is on the times tpj_bench wrote. Release decides what is slow, and debug is reported beside it. Nothing checks a time: a report and a comparison are read, never gated on.
>
> ### Reading lines
>
> parseLaunches and parseReport read text as lines separated by line feeds. A carriage return at the end of a line is removed, and a line that is then empty is skipped. A line's fields are separated by single spaces, and none is empty. Counts, ticks, and times are unsigned decimal integers that fit their types, and a hash is 16 lowercase hexadecimal digits. Each reader throws ReportError, a std::runtime_error, at the first line that breaks its form. The message begins `line <n>: `, where n counts every line from 1, skipped ones included. A stage line has the form stageLine writes: `stage <name> count <c> median <m> least <l> greatest <g> hash <h>` or `... vertices <v>`.
>
> ### Launches
>
> A launch is what one run of tpj_bench wrote, after a line naming the build that ran it. It is `build <name>`, then tpj_bench's park line, `park <path> ticks <t> warm-ups <w> repetitions <r>`, then any number of stage lines. parseLaunches(text) gives the launches in order. Each is a Launch: Build, Park, Ticks, WarmUps, Repetitions, and Stages, a StageResult per stage line. A build line must be followed by a park line: any other line there is refused, and text ending right after a build line is refused at the build line. A park line anywhere else is refused, and so are a stage line before the first launch and a stage line that repeats a stage name within its launch. Text with no lines gives no launches.
>
> ### Summarizing
>
> summarizeLaunches(launches) gives a report: one ReportPark per build and park among the launches, in the order each pair first appears. Each holds Build and Park; Launches, the number of its launches; their Ticks, WarmUps, and Repetitions; and one StageResult per stage of its launches, in their order. Each stage has its Kind and Result, and as Times the summarizeTimes of the stage's Median in each of the launches. So a report stage's Median is the median of the launches' medians, the lower of the middle two for an even count, as summarizeTimes takes it. Its Least and Greatest are the launches' spread, and its Count is the number of launches. Launches of one build and park must have done the same work. They must have the same Ticks, WarmUps, and Repetitions, the same stage names in the same order, and the same Kind and Result in each stage. Otherwise summarizeLaunches throws ReportError naming the build and park, and the stage when one stage's result differs. A result that differs between launches of one build would mean the simulation is not deterministic (principle 10), so it is refused rather than summarized. No launches give an empty report.
>
> ### Report text
>
> reportText(report) writes each ReportPark as `park <build> <path> launches <n> ticks <t> warm-ups <w> repetitions <r>`, followed by its stages as stageLine writes them, every line ending with a line feed. parseReport(text) reads that form back, so parseReport(reportText(report)) equals the report. A stage line before the first park line is refused. So is a park line that repeats a build and path already read, and a stage line that repeats a stage name within its park.
>
> ### Comparing
>
> compareReports(before, after) gives one StageComparison for each build, park, and stage name found in either report. Those of before come first, in its order, and then those found only in after, in after's order. Each holds Build, Park, and Stage, and Before and After, the stage's StageResult in each report, either one empty when the stage is not in that report. When both are present:
>
> - Change is Faster when After's Greatest is less than Before's Least, Slower when After's Least is greater than Before's Greatest, and Unclear otherwise. A change is clear only when every launch of one report lies strictly beyond every launch of the other. With ten launches on each side of an unchanged stage, the chance of that is 2 in 184,756.
> - ResultChanged is true when their Kind or Result differs, which means the two reports timed different work.
>
> When either is empty, Change is Unclear and ResultChanged is false.
>
> ### Comparison text
>
> comparisonText(comparisons) writes one line per comparison, then `clear <n> of <m>`, every line ending with a line feed. m counts the comparisons with both stages, and n those of them whose Change is not Unclear. The fields of a comparison with both stages are, in order:
>
> 1. the build, park, and stage;
> 2. `faster`, `slower`, or `unclear`;
> 3. `before`, then Before's median, `[` joined to its least, and its greatest joined to `]`;
> 4. `after`, then After's three times in the same form;
> 5. the change;
> 6. `result-changed`, only when ResultChanged.
>
> One with a single stage has the build, park, and stage, then `only-before` or `only-after`, then that stage's three times in the same form. Times are in microseconds: the nanoseconds divided by 1,000, written with one decimal place as std::format's `{:.1f}` writes them. The change is (After's median − Before's median) / Before's median × 100, written as `{:+.1f}` writes it, followed by `%`, or `n/a` when Before's median is 0. On every line but the last, each field but the line's last is followed by enough spaces to reach the width of the widest field in that position on any of those lines, and then one more. So the columns line up, and splitting a line at runs of spaces gives its fields. For example:
>
>     windows-release tests/parks/stress/winding-path.park food-overlay faster  before 2240.4 [2198.3 2301.9] after 512.7 [498.1 530.2] -77.1%
>     windows-release tests/parks/stress/winding-path.park ticks        unclear before 34.1   [33.8   34.9]   after 34.0  [33.6  35.0]  -0.3%
>     clear 1 of 2
>
> ### Command line
>
> tpj_bench_report summarize LAUNCHES reads the file LAUNCHES and writes reportText(summarizeLaunches(parseLaunches(text))) to standard output. tpj_bench_report compare BEFORE AFTER reads both files with parseReport and writes comparisonText(compareReports(before, after)). parseReportOptions(arguments, error), whose first argument is the program's name, gives ReportOptions, a Command, summarize or compare, and its Paths. Otherwise it gives none, with error set to a message naming the problem: no command, an unknown command, which the message names, or a count of files other than one for summarize and two for compare.
>
> Standard output is in the C runtime's text mode. tpj_bench_report writes it only once all of it is made, and writes nothing there on failure. Each failure exits with a nonzero status, after writing to standard error:
>
> - options parseReportOptions refuses: `tpj_bench_report: <message>` and the usage line `usage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER`;
> - a file readTextFile cannot read: `tpj_bench_report: cannot read <path>`;
> - a ReportError while reading or summarizing a file: `tpj_bench_report: <path>: <message>`;
> - any other exception, which only a failure to allocate can cause: `tpj_bench_report: <what>`.
>
> ### The script
>
> scripts/runtime-report.sh [--launches N] REPORT runs from WSL. It builds tpj_bench and tpj_bench_report with windows-release and tpj_bench with windows-debug, first configuring a preset whose build directory holds no CMakeCache.txt. It launches tpj_bench with no options on every file matching tests/parks/stress/*.park, in name order, N times on each build, 10 by default. It runs the launches in rounds: in each round, every park runs once with windows-release and then every park once with windows-debug. So each park's launches span the whole report, and any drift during it widens the spread rather than moving one park's median. Each launch's output goes to build/runtime-report/launches.txt after `build <preset>`, with carriage returns removed. Once the last launch is done, tpj_bench_report summarize of that file writes the report, without carriage returns, to REPORT, through REPORT.partial renamed into place. The script ends by printing `runtime-report: wrote <REPORT> in <m>m <ss>s`, the time since it started, builds included. scripts/runtime-report.sh --compare BEFORE AFTER builds tpj_bench_report with windows-release and prints its comparison of the two reports, handing it their paths through wslpath -w. Either stops with a message and a nonzero status when:
>
> - the arguments are not one of these forms;
> - N is not a positive integer;
> - a report to compare is missing;
> - there is no stress park;
> - a build fails;
> - a launch fails, naming its build, park, and round;
> - summarizing fails.

CLAUDE.md, Build and test, after the sentence about scripts/cross-build-check.sh:

> scripts/runtime-report.sh times the gameplay runtime on the stress parks with both Windows builds, and compares two reports (src/bench/SPEC.md); timings are reported, never gated.

## Files affected

- Create: src/bench/report/report_error.h, src/bench/report/launches.h, src/bench/report/launches.cpp, src/bench/report/report.h, src/bench/report/report.cpp, src/bench/report/comparison.h, src/bench/report/comparison.cpp, src/bench/report/report_options.h, src/bench/report/report_options.cpp, src/bench/report/internal/lines.h, src/bench/report/internal/lines.cpp, src/bench/report/main.cpp
- Create: scripts/runtime-report.sh
- Move: src/bench/park_file.h and src/bench/park_file.cpp to src/bench/text_file.h and src/bench/text_file.cpp, with readParkFile renamed readTextFile
- Modify: src/bench/stages.h (StageResult gains equality), src/bench/main.cpp, src/bench/CMakeLists.txt, src/bench/SPEC.md (whose first paragraph's "No test checks a time." becomes "It is a test utility: it is checked by running it, and has no tests."), CLAUDE.md, tests/CMakeLists.txt
- Modify: .claude/skills/planning-features/SKILL.md, .claude/skills/implementing-features/SKILL.md, .claude/agents/test-writer.md, and CLAUDE.md, so a test utility gets no test pass and no tests
- Delete: tests/bench/, stage-runner's tests of tpj_bench, since a test utility gets no tests. tests/CMakeLists.txt no longer adds it.

## Dependencies

- stage-runner: tpj_bench, stageLine, parkLine, StageResult, and summarizeTimes, met.
- At least one stress park: tests/parks/stress/winding-path.park, met.
- The windows-release and windows-debug presets, and wslpath in WSL: met.

## Out of scope

- Paired runs, building two trees and alternating their launches: the milestone's paired-runs candidate.
- The slowest call across launches, such as the worst tick, in the report: the milestone's worst-calls candidate.
- A readable view of one report: the milestone's readable-view candidate. Comparing a report with itself shows its times in microseconds meanwhile.
- Fewer ticks for debug launches: the milestone's open question, settled by full-park's first report.
- Checking the machine's conditions, such as its power plan or load: the research leaves them to the person running the report.

## Open questions

None.
