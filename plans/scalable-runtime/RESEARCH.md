# Research: scalable-runtime

## How do simulation games measure their runtime at scale?

Both games with public tooling measure from saved worlds, headless, on the shipped build. Factorio's --benchmark loads a save, runs a chosen number of ticks with nothing rendered, as fast as it can, and prints the total time and the average, least, and greatest time per tick. --benchmark-runs repeats it, and the community keeps shared benchmark maps so that numbers compare across changes and machines. OpenRCT2 has separate commands for the simulation and for rendering a park (benchsimulate, benchgfx), reports through Google Benchmark's console, JSON, or CSV output with repetition and aggregation flags, and keeps standard parks for it: an official test park holding every ride type, and a community park known to be expensive to render.

Three things carry over. A save is the unit of a benchmark, so a park that was slow once stays reproducible. The simulation and the frame are measured apart, since one runs headless and the other needs a window. And the report gives a distribution per tick, not only a total, because a rare slow tick is what a player feels as a hitch.

This project already has most of the pieces. tpj_scenarios runs park files headless and prints ticks per second to standard error, away from the compared output. The app's --park, --ticks, and --hash run the simulation without a window. The meshes, the overlay, and candidate worlds are built on the CPU through public headers, so they can be timed without a GPU. Only drawing needs a window, and the app's --frames and --capture already run it unattended.

Rejected: profiling only by hand with perf or Tracy when something feels slow. It is how affordable-sampling found its cause, and remains the tool for finding why, but it leaves no repeatable number, and the overlay's cost went unseen until a player-built park showed it. Rejected: measuring only inside the running app. Drawing needs it, but the simulation and the CPU-built meshes can be timed headless, which is scriptable and runs where no window can.

Sources: https://forums.factorio.com/viewtopic.php?t=52931 — --benchmark and --benchmark-ticks; https://mulark.github.io/tests/test-000001/test-000001.html — community benchmark method and shared maps; https://github.com/OpenRCT2/OpenRCT2/wiki/Benchmarking-&-stress-testing-OpenRCT2 — benchgfx, standard test parks, Google Benchmark reporting; https://manpages.ubuntu.com/manpages/stonking/man6/openrct2.6.html — benchsimulate and its options; src/scenarios/SPEC.md, tpj_scenarios timing on standard error.

## How large is a full park?

Planet Coaster's players report its frame rate falling mainly with guest count, and many cap their parks near 7,000 guests to stay playable. Planet Coaster 2 enforces a hard cap of 6,000. Those parks are far larger in area than this project's 256 m square. Guests are also where this project's own profile, taken before affordable-sampling, put three quarters of a tick's time (affordable-sampling's RESEARCH.md). So a full park's target is set by guests in the low thousands, with path length, junctions, and shops scaled to a densely built 256 m park. The winding-path.park incident shows path length alone can matter, through the overlay, independent of guests.

Sources: https://steamcommunity.com/app/493340/discussions/0/1290690669225215538/ — players capping guests for playability; https://forums.frontier.co.uk/threads/warning-hard-limit-for-6000-guests-and-parks-are-smaller-than-planco1.629146/ — Planet Coaster 2's 6,000 guest cap; plans/shared-medium/affordable-sampling/RESEARCH.md.

## How can timings be trusted without failing on them?

A timing varies between runs of one build on one machine. Separate process launches can differ by tens of percent, so repeating inside one process understates the spread. The benchmarking literature recommends at least ten invocations, medians rather than means, and measuring the noise floor, the same command run repeatedly with nothing changed, before reading any difference. Even with care, changes under about 5% are rarely distinguishable from noise. Comparing a before and an after is most reliable when the two are run as pairs on the same machine in alternating order.

That suits a report rather than a gate. A timing threshold in a test turns noise into failures; a per-test limit has already failed a push here under sanitizer load. A report that prints medians and spread for each stage, and a before-and-after comparison that marks a change as clear only when it exceeds the measured noise, informs a decision without blocking work. affordable-sampling already asked for exactly that: a feature's report gives its timings before and after.

Rejected: tests or hooks that fail when a stage exceeds a budget, including explained-food's former candidate of a frame budget check. Noisy on shared machines and under sanitizers, they fail work that changed nothing. Rejected: a single run per measurement. One launch cannot show the spread between launches, so a difference cannot be told from noise.

Sources: https://stefan-marr.de/2020/07/is-this-noise-or-does-this-mean-something-benchmarking/ — invocations, medians, the 5% floor; https://github.com/Sophia-Thickums/radv-bench-methodology — measure the noise floor first; https://github.com/lxsmnsyc/seroval/pull/220 — paired, alternating before-and-after runs reported rather than gated.
