# Milestone: Measured Runtime

Slice: none

## Summary

measured-runtime builds scalable-runtime's foundation: parks the size the game means to support, a headless runner that times each stage of the gameplay runtime on them, and a report that gives each build's medians and spreads and tells a real change from noise. It comes first because every later milestone of the capability is gated on what its report shows, and nothing yet measures the runtime on a full park: the food overlay's cost on a long path went unseen until it was played, and no park with 2,000 guests exists to show what it costs with 30 shops. The runner times the functions the app itself calls, so a speedup it reports is one the player gets. The first report on the full park decides whether affordable-overlay goes ahead.

## Acceptance criteria

- tests/parks/stress/ holds the stress parks, and neither the cross-build check nor the integration tests read any file in it: both read only tests/parks/*.park. An incident park is named for what it holds, never for how it played, since a fix leaves the park and not the symptom. parks/lowfps.park, a single 3 km guest path with one shop, moves there as winding-path.park, the first incident park.
- The guests module has a public function, addGuest(world, place, stayUntil), that creates a guest at a place on the guest network whose stay ends at the tick given, with its hunger and hunger rate drawn as an arriving guest's are. Arrivals create their guests through it, so a guest comes into being one way. src/sim/guests/SPEC.md describes it. No code outside the guests module writes a guest's components (principle 6).
- tests/parks/stress/full.park is the full park generator's output for its seed and sizes. It is physically valid, and holds, in the 256 m park, about 2,000 guests created through addGuest, 30 shops of which none is starved, supplied by a few depots over backstage paths, and about 4 km of guest path with about 100 junctions. The generator steps the park before saving it, so it is saved as a park mid-play, with guests spread along its paths, visits queued at shops, and supplies in transit, and every guest's stay ends after the longest run the runner makes of it, so the park holds about 2,000 guests through every run. The generator is simulation code in tpj_scenarios_lib, built with its floating-point flags, and tpj_bench writes its park to a path it is given, so generating again from the same seed and sizes gives the same text on every build. A test loads every stress park with the park's schema, so a change to what a save holds fails until the parks are made again.
- tpj_bench, given a park file, loads and resolves it and times, through public headers only: stepping a number of ticks, with the median, the spread, and the worst tick; one resolution of the loaded park; making the candidate of an edit accepted on the park that leaves its networks as they are, so the candidate costs what the park itself costs; building the food overlay; and building the park mesh and the guest mesh. No timed tick includes the loaded park's first resolution. A repeated stage runs and discards warm-up repetitions before the counted ones.
- tpj_bench writes each stage's result beside its timing, such as the world's hash after stepping and each mesh's vertex count, so no build can time less work than another and drop it unseen. The hash after stepping equals the hash of the loaded, resolved park stepped as many ticks with nothing timed.
- A new unit, src/views, joins render's mesh builders with legible's explanations into what the app shows, starting with the food overlay: the mesh buildFoodOverlay gives, shaded by foodAvailability's value. It has its own layer in cmake/layers.txt, above tools, render, legible, and scenarios and below app, so decision 0027 holds unchanged. The app builds its overlay through it, and tpj_bench times that same function, so a change to how the app builds the overlay shows in the report.
- tpj_bench and its library are a unit of cmake/layers.txt, src/bench, in a layer of its own above views and below app's, so the layer check refuses any include of app from it, since nothing depends on app (decision 0027). The layer check passes. tpj_bench is the one reader of the clock that times stages on the stress parks, beside tpj_scenarios' existing ticks-per-second line, and simulation code never reads it (principle 10).
- scripts/runtime-report.sh, run from WSL at the repository root, builds tpj_bench with windows-release and windows-debug, launches it a number of times on every stress park with each build, ten by default, and writes a report: for each park, build, and stage, the median of the launches' medians, and their least and greatest as its spread. It prints how long the report took. Release decides what is slow, and debug is reported beside it.
- The script compares two reports, and marks a stage's change clear only when every launch of one report lies beyond every launch of the other for that park and build. The summarizing and comparing are functions of src/bench, tested through its headers. Two reports of an unchanged tree mark no change clear on the stress parks, shown in the feature's report.
- No test, hook, or check fails on a timing.
- The milestone changes no observable behavior: existing tests pass unchanged, a --capture of a park with --overlay food shows the same scene as before, and the cross-build check's output is identical.
- The milestone's report includes the first report of every stress park on both builds, and says whether the food overlay is costly on windows-release on the full park, the condition affordable-overlay depends on.

## Medium

This milestone introduces, samples, and emits no fields or flows. The runner reads parks only through the public headers the app uses: loading and resolving, stepping, makeCandidate, views' overlay function, and render's mesh builders. The generator changes a world only through park commands and addGuest, so the guests module alone writes its guests' state.

## Dependencies

- scalable-runtime's CAPABILITY.md: approved.
- Decision 0028, engineering capabilities: met.
- Park files and loading, which save every guest's state: met.
- The layer table and its check: met.
- The windows-release preset: met.
- parks/lowfps.park, untracked, which becomes winding-path.park: present, and it loads and steps with today's schema.

## Core feature

stage-runner, because it produces numbers on its own. On winding-path.park and the parks already checked in, it shows each stage's cost on both builds as soon as it lands, before the full park exists, and it is what every other feature feeds.

## Features

1. `stage-runner`: src/views with the food overlay's build, which the app calls in place of its own, and src/bench with tpj_bench, which loads a park and times each runtime stage, that build among them, with its result beside it. parks/lowfps.park moves to tests/parks/stress/winding-path.park. Depends on: none.
2. `runtime-report`: summarizing launches into medians and spreads, comparing two reports by non-overlap in src/bench, and scripts/runtime-report.sh building both presets and launching the runner on every stress park. Depends on: feature 1.
3. `created-guests`: addGuest in the guests module, with arrivals creating their guests through it, and its spec, compared before and after by the report. Depends on: feature 2.
4. `full-park`: the full park's generator in tpj_scenarios_lib, written by tpj_bench, tests/parks/stress/full.park made by it, the test that loads every stress park, and the first report on the full park. Depends on: features 1 and 3.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Paired runs: the script builds two commits side by side and alternates their launches, so a drift in the machine cannot pass for a change. Gated on: a comparison that sequential reports leave unclear.
- Worst calls across launches: the report gives, beside each stage's median and spread, the slowest call of any launch, so the worst tick, the hitch a player feels, survives summarizing. Gated on: a stage whose worst tick matters while its median does not.
- A readable view of one report: the script prints a report in microseconds with its columns aligned, as the comparison does, rather than leaving the nanoseconds of the file to be read. Gated on: reading single reports often enough that comparing one with itself is a nuisance.
- Upkeep over ticks: the runner drives the scene sync over a run of ticks and times what keeping the meshes current costs per tick, not one build of each. Gated on: a milestone whose change is how often a mesh is rebuilt, as affordable-overlay's is.

## Open questions

- How long a report takes on windows-debug with 2,000 guests. Resolved by the first report on the full park; if it takes too long to wait for, debug launches step fewer ticks than release ones, the same in every report of that build.

## Research notes

- Launches, not repetitions inside one, carry the variance that matters, so each launch is summarized by its median and launches are compared as samples. A change is clear only when two reports' launches do not overlap, which with ten launches each marks almost nothing clear in an unchanged tree, where a significance test at 5% per stage would, across many stages and parks.
- The script runs a report's launches in rounds, every park once on each build per round, so drift during a report widens every park's spread alike, and in a fixed order, so a position effect is the same in the two reports a comparison reads (runtime-report/RESEARCH.md).
- The clock is steady_clock, backed by QueryPerformanceCounter on MinGW-w64, never high_resolution_clock, which libstdc++ makes the wall clock. Repetitions are fixed counts after discarded warm-ups, and each result is written out so the optimizer keeps the work.

Depth is in RESEARCH.md.
