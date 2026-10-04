# scenarios

The scenario runner: a headless executable that runs synthetic scenarios and park files and prints what every build must compute identically (decisions 0022 and 0036). It also generates the checked-in parks that are made rather than drawn. It is a test utility, checked by running it.

## Boundary

The library tpj_scenarios_lib holds the scenarios, the runner's output, and the park generators, and links tpj_sim alone. It is simulation code: it builds with the simulation's floating-point flags, the sim symbol check runs on its archive, it draws only through sim/draw.h and computes e^x and ln x only through sim/sim_math.h, and only main reads the clock. It reads the math and draw tables in tests/sim/support. The executable tpj_scenarios adds only its main, and every preset builds it. scripts/cross-build-check.sh depends on its output and on --compare's exit statuses, below.

## Scenarios

registeredScenarios lists the synthetic scenarios, each named, seeded, and run in a fixed order. Between them they exercise keyed draws, sim math, entity creation and destruction, derived entities, the medium's fields and flows, random park edits with the guests and shops they affect, and a food shop whose supply is cut and restored, so that every part of the simulation whose arithmetic or ordering could differ between builds shows up in the compared output.

## Output

tpj_scenarios [--ticks n] [file...] writes, for each scenario and then each park file, a line after resolution and after every one of n cycles, then the sim math table's results, then the keyed draws' results. A scenario's line is `scenario <name> tick <t> hash <h>` and a file's `file <path> tick <t> hash <h>`, with h the world's hash as 16 lowercase hexadecimal digits. Two builds that simulate identically write identical output. Timings go to standard error, so they never enter the compared output. It exits with a nonzero status and a message for an unknown option, a malformed --ticks, or a file it cannot read or load.

tpj_scenarios --compare left right exits with status 0 when the two outputs hold the same lines. Otherwise it prints the first line where they differ, from each side, and exits with status 1. It exits with status 2 when it is not given two files or cannot read one. Work that must leave the simulation unchanged shows it with this comparison.

## Slice parks

tpj_scenarios --slice-parks fed warm cut makes the boxes-and-tubes slice's parks from fed: warm, the park after WARM_TICKS cycles, long enough that a guest waits at the shop and a shipment is in transit, and cut, warm one cycle later with its backstage paths deleted, so the shop's supply is gone from the first tick. tests/parks/fed.park, warm.park, and cut.park are its output, and running it again picks up a change to the simulation's rules. It writes no file when it fails.

## The full park

makeFullPark makes the stress park the size the game means to support: about 4 km of guest path in a grid of 100 junctions, 30 shops each joined to the guest network and to a backstage row, three depots, and 2,000 guests spread evenly along the paths, stepped a minute into play by default (FULL_PARK_WARM_TICKS). Every shop is reachable and supplied, and it throws std::logic_error naming the edit or the shop when that would not hold, so any park it gives is valid and none of its shops is starved. Its save is the same text on every build. tpj_bench --full-park writes it to tests/parks/stress/full.park (bench/SPEC.md).

## Cross-build check

scripts/cross-build-check.sh builds tpj_scenarios with windows-debug, windows-release, and linux-release, runs each over every tests/parks/*.park file, and fails on the first line where a release build's output differs from windows-debug's. scripts/release-check.sh runs it at release (decision 0030).
