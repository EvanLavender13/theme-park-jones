# Feature: Full Park

## Summary

full-park makes the stress park the size the game means to support, and gives the first report on it. makeFullPark, in tpj_scenarios_lib, builds it from the new park's template with park commands:

- a 10 × 10 grid of guest paths, 4 km of path crossing at 100 junctions, joined to the entrance;
- five backstage rows, each serving six shops, 30 in all;
- a backstage spine down the west margin, joining the rows, with three depots on it.

It resolves the park and adds 2,000 guests through addGuest, spread evenly along the guest network. Each one stays 20 minutes past the save. It then steps a minute of play, so the park is saved with guests spread, visits queued at shops, and supplies in transit. makeFullPark refuses to give a park in which a command was refused or a shop is starved. `tpj_bench --full-park PATH` writes the park's save, and tests/parks/stress/full.park is that file, about 1 MB. The generator and the park are test utilities: they have no tests, and every criterion is checked by running them. The feature's report gives the first runtime report of both stress parks on both builds. It also says how long the food overlay takes on windows-release on the full park, which decides whether affordable-overlay goes ahead.

## Acceptance criteria

makeFullPark, tpj_bench, and full.park are test utilities. They have no tests. The implementer checks each criterion by running the commands, and the feature's report gives their output.

1. `build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park` exits with status 0. So every command makeFullPark applied was accepted, the park is physically valid, and no shop is starved.
2. tpj_bench --full-park from windows-debug and from linux-debug each write a file identical to full.park, byte for byte, so generating again gives the same text on every build.
3. full.park holds:
   - 21 guest paths and 6 backstage paths;
   - 30 shop boxes and 3 depot boxes;
   - exactly 2,000 guests whose stay-until is 37800, among at least 2,000 guests in all;
   - at least one shop with a non-empty queue;
   - at least one supplies packet in transit.
4. `scripts/runtime-report.sh build/runtime-report/full-park-first.txt` writes a report with a park line for each of full.park and winding-path.park on each build, and prints how long it took. The feature's report gives that report and the time. It also gives the full park's food-overlay median on windows-release beside the 33 ms between ticks, which is the condition affordable-overlay depends on.
5. The existing tests pass, and scripts/cross-build-check.sh passes with output identical to before, since neither reads tests/parks/stress/.

## Medium

This feature introduces, samples, and emits no fields or flows. makeFullPark changes a world only through park commands, addGuest, resolveWorld, and stepWorld, so each module alone writes its own state. It reads the guest network's edges and each shop's nearestDepot through public headers.

## Principle checks

- Principle 6: makeFullPark writes no module's state. It creates paths and boxes only through park commands, which isAccepted checks, and guests only through addGuest.
- Principle 10: the park is a pure function of the seed and the sizes. Its draws are keyed, it reads no clock, and tpj_scenarios_lib builds with tpj_sim's floating-point flags. Criterion 2 shows the same text on three builds.
- Principle 5: the park is physically valid. makeFullPark applies only accepted commands and refuses a park in which one was refused (criterion 1).

These are run checks, since the generator is a test utility.

## Spec changes

src/scenarios/SPEC.md, a new paragraph after makeSliceParks's two:

> makeFullPark() makes the full park, the stress park the size the game means to support (plans/scalable-runtime/measured-runtime). tests/parks/stress/full.park is its save, written by tpj_bench --full-park. It starts from makeNewPark(FULL_PARK_SEED), with FULL_PARK_SEED 1, deletes the template's path, and applies, in this order, these park commands, every one of which must be accepted:
>
> - a guest path through (0, 123) and (0, 90), from the entrance's door to the grid;
> - ten guest paths through (x, -100) and (x, 100), for x = -90 + 20i with i from 0 to 9, then ten through (-100, z) and (100, z), for z = -90 + 20j with j from 0 to 9, so 4 km of path crossing at 100 junctions;
> - a backstage spine through (-106, -100) and (-106, 100), then five backstage rows through (-106, r) and (100, r), for r = -80, -40, 0, 40, and 80;
> - for each row r in that order, a shop at (x, r - 4.5) facing (0, -1), for each x = -80 + 20k with k from 0 to 8 and k mod 3 not 2. That is 30 shops, each with its front door 2.5 m from the guest path at r - 10 and its back door 1.5 m from its row;
> - three depots at (-111.5, z) facing (1, 0), for z = -60, 0, and 60, each with its front door 1.5 m from the spine.
>
> It then resolves the world, and adds FULL_PARK_GUESTS, 2,000, guests with addGuest. Guest i, from 0, stands on the edge of the guest network that drawPick picks over the edges' lengths, ToDistance - FromDistance, keyed drawKey(world, NULL_KEY, hashName("full-park-edge"), i). It stands at FromDistance + u * (ToDistance - FromDistance), with u drawUniform(drawKey(world, NULL_KEY, hashName("full-park-along"), i)). So guests spread evenly along the network's length. Each one's stay ends at FULL_PARK_WARM_TICKS + FULL_PARK_STAY, with FULL_PARK_WARM_TICKS 1,800 and FULL_PARK_STAY 36,000. Last, it steps the world FULL_PARK_WARM_TICKS times with no commands. So the park is saved a minute into play, with guests spread along its paths, visits queued at shops, and supplies in transit. A guest the generator adds stays 20 minutes past the save, 120 times tpj_bench's default run, and guests the entrance admits meanwhile come and go as in any park.
>
> makeFullPark throws std::logic_error naming the command when one is refused, and naming the shop's key when a shop has no nearestDepot after the first resolution. So any park it gives is physically valid, and none of its shops is starved. It is simulation code, built with the library's floating-point flags, so its save is the same text on every build. It is a test utility, checked by running it, and has no tests.

src/bench/SPEC.md:

- The first paragraph's "It links tpj_views, tpj_render, tpj_legible, and tpj_sim" becomes "It links tpj_views, tpj_render, tpj_legible, tpj_scenarios_lib, and tpj_sim".
- Command line: "read in binary mode,, or none" becomes "read in binary mode, or none".
- Command line, the parseBenchOptions paragraph: "gives BenchOptions, the Ticks and the Park path," becomes "gives BenchOptions, the Ticks, the Park path, and FullPark, the path --full-park names or empty,"; the list of refusals gains "--full-park with no value or with a file;" before "no file", which becomes "no file without --full-park"; and "the usage line `usage: tpj_bench [--ticks N] FILE`" becomes "the usage line `usage: tpj_bench [--ticks N] FILE | --full-park PATH`".
- Command line, a new paragraph after the first:

> tpj_bench --full-park PATH writes saveWorld of makeFullPark() (scenarios/SPEC.md) to PATH with writeTextFile, which writes in binary mode, so every line ends with a line feed on every build. It writes nothing to standard output. It exits with status 0 once the file is written, or with a nonzero status after writing to standard error `tpj_bench: cannot write <path>` when the file cannot be written, and `tpj_bench: <what>` when makeFullPark throws.

src/bench/text_file.h gains:

```cpp
// Writes the text to the file in binary mode, replacing it. False when it cannot be written.
bool writeTextFile(const std::string &path, std::string_view text);
```

src/scenarios/full_park.h, new:

```cpp
// The full park's seed, its added guests, the ticks it steps before it is saved, and how long its
// added guests stay past the save.
inline constexpr uint64_t FULL_PARK_SEED = 1;
inline constexpr uint64_t FULL_PARK_GUESTS = 2000;
inline constexpr uint64_t FULL_PARK_WARM_TICKS = 1800;
inline constexpr uint64_t FULL_PARK_STAY = 36000;

// The full park, a minute into play (scenarios/SPEC.md). Throws std::logic_error when a command
// is refused or a shop is starved.
World makeFullPark();
```

## Files affected

- Create: src/scenarios/full_park.h, src/scenarios/full_park.cpp
- Create: tests/parks/stress/full.park, written by tpj_bench --full-park
- Modify: src/scenarios/CMakeLists.txt, src/scenarios/SPEC.md
- Modify: src/bench/CMakeLists.txt, src/bench/options.h, src/bench/options.cpp, src/bench/text_file.h, src/bench/text_file.cpp, src/bench/main.cpp, src/bench/SPEC.md

## Dependencies

- created-guests: addGuest, met.
- unsaved-choices: guests keep no last choice, so the save is about 1 MB, met.
- runtime-report: scripts/runtime-report.sh, which launches every tests/parks/stress/*.park, met.
- Park commands, makeNewPark, nearestDepot, and the guest network's edges: met.

## Out of scope

- Fewer ticks for debug launches: the milestone's open question. The time this feature's report takes is the evidence, and Evan decides.
- Other stress parks, such as a park of many short paths: added when an incident or a measurement calls for one.
- Sizes given on the command line: the sizes are constants, and changing them changes the park's definition, which is a change to this spec.

## Open questions

- Whether the food overlay is costly on windows-release on the full park. Resolved by criterion 4's report. Evan decides whether affordable-overlay goes ahead.
