# Milestone: Indexed Sampling

Slice: none

## Summary

indexed-sampling brings the full park's tick under the 33 ms it covers, and makes the runtime report say so, or say it does not, over minutes of play. On windows-release a full-park tick costs about 44 ms, so the park can never run in real time, and the frame clock's tick cap only turns the 3 FPS spiral into steady slow motion at about 10 FPS. A Windows profile puts three quarters of the time in guests sampling route distance: each sample scans every entry of every source's slot, one entry per node per shop, so a tick costs guests times shops times network size. The milestone indexes each slot's entries by place, so a sample looks up the few entries at its place instead of scanning, and what it returns does not change. It also fixes the measuring that let the overrun pass unremarked: the report steps minutes of game time, marks every stage over its budget, and times the app's frames on every stress park. It comes before affordable-overlay because ticks run whenever the park runs, while the overlay costs only when shown, and because it is the scalable-runtime deepening candidate this profile met the gate of.

## Acceptance criteria

- tpj_bench's ticks stage steps a run of sustained play, at least 3,600 ticks (two minutes of game time) on windows-release, and gives the median, spread, and worst tick. scripts/runtime-report.sh may step fewer ticks on windows-debug, the same in every report of that build.
- The report carries each stage's worst call across all launches beside its median and spread, so the worst tick, the hitch a player feels, survives summarizing and appears in both reports a comparison reads.
- The report states every stage's median against its budget, SIM_TICK_SECONDS (33 ms) for ticks and for each per-frame stage, and marks each stage over it, so an overrun cannot pass unremarked.
- The report times the app itself on each stress park with windows-release: the median and worst of its frame times over a fixed number of frames after loading, from the app's own frame clock, so what the player sees is in the report beside the headless stages.
- A sample finds each source's entries at its place through an index of that source's entries ordered by place: a stop place is an exact lookup, and the inside of an edge a range on one carrier. Its cost grows with the number of sources and the logarithm of their entries, not with their entries. src/sim/medium/SPEC.md says so.
- sampleField, sampleResolvedField, and fieldValue give exactly what src/sim/medium/SPEC.md defines, in the same order, for every field, network, and place. Every existing test passes unchanged, and tpj_scenarios's windows-debug output over the scenarios and tests/parks/*.park is identical, line for line, before and after the milestone, by tpj_scenarios --compare (decision 0030).
- The index is derived state with one owner. It is never saved, hashed, or compared, so saves and hashes are unchanged, and a test checks that the index a world keeps equals one built afresh from its entries after resolutions, steps, swaps, copies, and loads (principle 1).
- On windows-release the full park's tick median falls below 33 ms, shown by the report's comparison before and after the index, with the change marked clear.
- The milestone's report gives, before and after, the full park's sustained tick median and worst tick, the app's median and worst frame time on it, and the food overlay's build time, and says whether the overlay is still over budget, which affordable-overlay is gated on.
- No test, hook, or check fails on a timing. windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean. Linux is checked at release, not by the milestone (decision 0030).

## Medium

The milestone changes how the medium answers samples, not what any field or flow carries. Route distance, the food offer, and hungry footfall are produced and sampled exactly as before by navigable-networks, plausible-operations, and believable-guests, and faster. The index is internal to the shared medium's field storage and adds no query other modules read. The report reads parks and the app only through tpj_bench's public headers and the app's command line.

## Dependencies

- measured-runtime: the stress parks, tpj_bench, and the report script. Met.
- The frame clock's tick cap, so the app's frames on the full park can be timed at all: met (66d00c6).
- affordable-sampling's stop places, which the index's lookups use at nodes: met.

## Core feature

`budgeted-report`, because it is small and produces value on its own: its first report states, over two minutes of play, that the full park's tick is over budget and what the app's frame rate is, the measurement whose absence let the overrun go unnoticed. It is also the report place-indexed-entries is measured before and after with.

## Features

1. `budgeted-report`: tpj_bench steps sustained runs, the report keeps each stage's worst call across launches and marks every stage over its budget, and the script times the app's frames on every stress park on windows-release. Depends on: none.
2. `place-indexed-entries`: each field slot's entries indexed by place, kept by the medium beside the entries and rebuilt whenever they change, and sampling at nodes and inside edges through it, with the report's comparison before and after. Depends on: feature 1.

## Deepening candidates

- Targeted route samples: between nodes a walking guest needs only its target's or home's entry, not every source's, so it could sample one source. Gated on: the report showing guest sampling still a large share of the tick after the index.
- Fewer samples per walk: walk samples again at every node it passes in one tick. Gated on: the same.

## Open questions

None.

## Research notes

- A Windows gprof profile of 600 full-park ticks puts 78% of the run in sampleSlots for guest route distance, about 2,760 samples a tick at 7 µs each, each reaching about 31 sources; the scan of every entry of every slot is the cost.
- An index ordered by place answers both node lookups and the inside of an edge, and is independent of the network sampled on; indexes by node or edge, hash maps, and reordering the slot itself are rejected.
- The report's 300 ticks were ten seconds of game time with no budget beside them, which is how a 44 ms tick and the app's 3 FPS went unremarked.

Depth is in RESEARCH.md.
