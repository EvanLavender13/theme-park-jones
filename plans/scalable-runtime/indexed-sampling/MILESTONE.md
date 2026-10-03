# Milestone: Indexed Sampling

Slice: none

## Summary

indexed-sampling makes the full park hold one tick per frame in the app, and makes the runtime report say whether it does, over minutes of play. On windows-release a full-park tick costs about 42 ms, so the park can never run in real time, and the frame clock's tick cap only turns the 3 FPS spiral into steady slow motion at about 10 FPS. A Very Sleepy profile of the tick puts 19.5 of its 23.5 ms in guests sampling route distance while they walk: each walking guest samples every source's entries at its place on every pass of its walk, scanning each slot and allocating the sample's vectors, though between choices it needs only its target's entry or the entrances'. The milestone has walking guests read only the one source they follow, without allocating, and sample every source only when they choose. It also fixes the measuring that let the overrun pass unremarked: the report steps minutes of game time, marks every stage over its budget, and times the app's frames on every stress park. Then, since every sample still scans each source's entries, about one per network node, and the food overlay samples every source at every metre of guest path, each slot's entries are indexed by place, so a sample finds them by binary search. It comes before affordable-overlay because ticks run whenever the park runs, while the overlay costs only when shown.

## Acceptance criteria

- tpj_bench's ticks stage steps a run of sustained play, at least 3,600 ticks (two minutes of game time) on windows-release, and gives the median, spread, and worst tick. scripts/runtime-report.sh may step fewer ticks on windows-debug, the same in every report of that build.
- The report carries each stage's worst call across all launches beside its median and spread, so the worst tick, the hitch a player feels, survives summarizing and appears in both reports a comparison reads.
- The report states every stage's median against its budget, SIM_TICK_SECONDS (33 ms) for ticks and for each per-frame stage, and marks each stage over it, so an overrun cannot pass unremarked.
- The report times the app itself on each stress park with windows-release: the median and worst of its frame times over a fixed number of frames after loading, from the app's own frame clock, so what the player sees is in the report beside the headless stages.
- A walking guest reads route distance between choices through one source's entry, its target's or each entrance's, found without allocating, and samples every source only when it chooses. What it reads is exactly what sampling every source gave it, so its behavior is unchanged: every existing test passes unchanged, and tpj_scenarios's windows-debug output over the scenarios and tests/parks/*.park is identical, line for line, before and after the milestone, by tpj_scenarios --compare (decision 0030).
- On windows-release the full park holds one tick per frame in the app: the report's app frames median on the full park is below 33 ms, and its ticks median falls below 33 ms with the change marked clear against the milestone's before report.
- The milestone's report gives, before and after, the full park's sustained tick median and worst tick, the app's median and worst frame time on it, and the food overlay's build time, and says whether the overlay is still over budget, which affordable-overlay is gated on.
- No test, hook, or check fails on a timing. windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean. Linux is checked at release, not by the milestone (decision 0030).

## Medium

The milestone changes how guests read the medium, not what any field or flow carries. Route distance, the food offer, and hungry footfall are produced exactly as before by navigable-networks, plausible-operations, and believable-guests. The shared medium adds queries for one source's slot and its entry at a node, navigable-networks adds one source's route entry at a place, and the guests module reads route distance and the food offer through them. The report reads parks and the app only through tpj_bench's public headers and the app's command line.

## Dependencies

- measured-runtime: the stress parks, tpj_bench, and the report script. Met.
- The frame clock's tick cap, so the app's frames on the full park can be timed at all: met (66d00c6).
- affordable-sampling's stop places, which a node lookup compares with: met.

## Core feature

`budgeted-report`, because it is small and produces value on its own: its first report states, over two minutes of play, that the full park's tick is over budget and what the app's frame rate is, the measurement whose absence let the overrun go unnoticed. It is also the report targeted-route-samples is measured before and after with.

## Features

1. `budgeted-report`: tpj_bench steps sustained runs, the report keeps each stage's worst call across launches and marks every stage over its budget, and the script times the app's frames on every stress park on windows-release. Depends on: none. Done.
2. `targeted-route-samples`: one source's slot, its entry at a node, and its route entry at a place, each found without allocating, and walking guests reading route distance and the food offer through them, sampling every source only when they choose, with the report's comparison before and after. Depends on: feature 1. Done.
3. `place-indexed-entries`: each field slot's entries indexed by place, kept beside the entries, and every sample, and one source's entry at a node, finding entries through it by binary search instead of scanning, with each sample reusing its working lists across its sources. Depends on: feature 2, and a Windows profile putting most of the full park's food overlay build in route distance samples' slot scans.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Fewer samples per walk: walk reads route distance again at every node it passes in one tick. Gated on: a Windows profile after place-indexed-entries showing walking guests' route distance reads a large share of the tick.

## Open questions

None.

## Research notes

- A Very Sleepy profile of 600 full-park ticks on windows-release puts 19.5 of 23.5 ms a tick in sampleField of guest route distance under walk: about a third heap allocation and free, a third comparing places, and the rest loops and copies. An earlier gprof profile, blind to time in ntdll's heap, put 78% of the run in sampling's own code and led to an index by place that took the tick only from about 42 to 28 ms; it was discarded.
- After targeted-route-samples and the overlay's offers read one shop each, a Very Sleepy profile puts 84% of the full park's 103 ms overlay build in the route distance sample's own time: about 6,000 samples a build, each scanning 31 sources' entries, one per reached node.
- Between choices a walking guest uses one source's entry from a sample of every source, so reading that source alone removes about thirty slot scans and every allocation from each pass.
- The report's 300 ticks were ten seconds of game time with no budget beside them, which is how a 44 ms tick and the app's 3 FPS went unremarked.

Depth is in RESEARCH.md.
