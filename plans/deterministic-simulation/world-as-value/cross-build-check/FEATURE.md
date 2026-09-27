# Feature: Cross-Build Check

## Summary

cross-build-check proves that the Windows and Linux builds simulate bit-identically (decision 0022). tpj_scenarios is a headless runner in src/scenarios, built by every preset, that links tpj_sim and the scenarios library. It runs each registered synthetic scenario, and then each park file it is given, for a number of ticks, printing one line with the world's hash at each tick. Then it prints simExp's and simLog's results for every argument of sim-math's reference table, and the draws for every key of keyed-draws' expected-draw table. Every line names its source and tick, so the first line on which two outputs differ says what diverged and when. Its --compare mode finds that line. Each run's ticks per second go to standard error, which is the harness for the slice's timing question.

scripts/cross-build-check.sh builds the runner with linux-debug and windows-debug, runs it on both over every park file in tests/parks/, and compares the outputs. The pre-push hook runs the script when the pushed commits touch the runner's build inputs. Park files are loaded with makeParkSchema, the park's single registration function in tpj_sim. It registers nothing yet, and the capabilities that add park types fill it in. Two synthetic scenarios exercise the cycle across builds:

- walkers makes keyed draws and calls simExp and simLog in its systems, and creates and destroys entities.
- beacons queues commands, whose resolution creates derived entities.

## Acceptance criteria

1. runScenario writes n + 1 lines for a run of n ticks, each "scenario <name> tick <t> hash <h>". The first line is for the scenario's world after construction from its schema and seed, Populate, and resolveWorld. Each later line follows one stepWorld with the commands the scenario's QueueCommands queued for that cycle. t is the world's Tick, and h is hashWorld of the world as 16 lowercase hexadecimal digits. Running a scenario twice writes identical lines.
2. runSave writes n + 1 lines for a run of n ticks, each "file <label> tick <t> hash <h>". The first line is for the world loadWorld reads from the text with the given schema, after resolveWorld. Each later line follows one stepWorld with no commands. Text that is not a save throws LoadError, as loadWorld does.
3. writeSimMathLines writes one line "exp argument <a> result <r>" for each argument of sim-math's EXP_REFERENCE table, in table order, and then one "log argument <a> result <r>" line for each argument of LOG_REFERENCE. a is the argument's bits and r the bits of simExp or simLog of it, each as 16 lowercase hexadecimal digits. writeDrawLines writes one line "draw <i> bits <b> uniform <u> integer-pick <p> double-pick <q>" for each key of keyed-draws' EXPECTED_DRAWS table, in table order. i is the key's index in the table, and b and u are drawBits and drawUniform's bits as 16 lowercase hexadecimal digits. p and q are drawPick over the table's integer and double weights, in decimal.
4. The registered scenario names are distinct, and each is lowercase letters, digits, and hyphens. At least one registered scenario's systems make draws: two worlds made from it that differ only in seed differ, after 100 cycles, in more than their seed.
5. firstDifference of two texts is empty exactly when they hold the same lines. Otherwise it gives the lowest 1-based line number at which they differ, with each text's line there, or none for a text that has ended. A line is the text up to each line feed, and a last line with no line feed. So a text that is a proper prefix of the other differs at the line after its last. Lines are compared byte for byte, carriage returns included.
6. tpj_scenarios, run with [--ticks n] and park file paths, writes to standard output exactly:
    - runScenario's lines for each registered scenario, in registration order;
    - runSave's lines for each file, in the order given, labeled by its path as given and loaded with makeParkSchema;
    - writeSimMathLines' lines;
    - writeDrawLines' lines.

    n defaults to 3000. For each scenario and file it writes to standard error one line with its ticks per second. It exits with status 0. When an option is not recognized, the value of --ticks is not a decimal count, or a file cannot be read or loaded, it exits with a nonzero status and a message on standard error naming the problem, and for a load error, the file and the line. tpj_scenarios --compare left right exits with status 0 when the two files hold the same lines. When they differ, it exits with status 1 and prints firstDifference's line number and both lines, labeled by the file paths.
7. The symbol check passes on the scenarios library's archive, as it does on tpj_sim's.
8. Checked by running rather than by the test pass:
    - scripts/cross-build-check.sh, run from WSL at the repository root, prints each stage as it starts and exits with status 0 on this tree.
    - Given a Windows output that differs from the Linux one, it exits with a nonzero status and prints the report from --compare.
    - The pre-push hook runs the script when the pushed commits touch a trigger path and skips it otherwise. Either way, it prints which it did.

## Medium

This feature introduces no fields or flows. It provides:

- makeParkSchema: effortless-building, shared-medium, believable-guests, and plausible-operations register their park types, systems, resolvers, and commands in it, in a written order. tpj_scenarios loads park files with it, and sketch-a-park's app options will too.
- tpj_scenarios and the cross-build script: they run the park files that sketch-a-park checks into tests/parks/, which is how the slice's criterion 7 compares fed.park across builds. The per-run ticks per second are the harness for the slice's timing question and the milestone's copy-cost question.
- The line forms: a divergence report names the scenario or file and the tick. The capability's first-divergence deepening candidate starts from that line.

## Principle checks

- Principle 10: criterion 1's repeated run, and criteria 5 and 8. A run's output depends on nothing but its scenario or save and its tick count, and the script shows the two builds write the same lines. The runner reads the clock only in its main, and it writes timings to standard error, so no timing reaches the compared output. The scenarios library builds with -ffp-contract=off like tpj_sim, and criterion 7 shows it calls no C runtime transcendental function.
- Principle 1: criterion 2. A park file is run as its load plus resolution, so the park's derived data is regenerated, never read from the file.
- Principle 6: the runner touches worlds only through tpj_sim's public functions, and each synthetic scenario's systems, resolvers, and commands touch only that scenario's own types.

## Spec changes

src/sim/SPEC.md: at the end of the paragraph beginning "Component types are registered with a WorldSchema", add:

"The park's schema comes from one function, makeParkSchema in sim/park_schema.h, where each capability's component types, systems, swap functions, resolvers, and commands are registered in a written order. Park files are loaded with it. It registers nothing yet."

Create src/scenarios/SPEC.md:

"# scenarios

The scenario runner: a headless executable that runs synthetic scenarios and park files, and prints what the two builds must compute identically (decision 0022).

## Contract

The library tpj_scenarios_lib holds the scenarios and the runner's output functions. It links tpj_sim alone and reads the generated tables in tests/sim/support. The executable tpj_scenarios adds only its main, and every preset builds it. The library is simulation code: it builds with -ffp-contract=off, and the symbol check runs on its archive as on tpj_sim's. Its systems draw only through sim/draw.h and compute e^x and ln x only through sim/sim_math.h. Only main reads the clock.

A Scenario has a name, a seed, a function making its schema, a Populate function that fills a new world, and a QueueCommands function that queues the commands for the cycle about to run, or none. Names are distinct, and lowercase letters, digits, and hyphens. registeredScenarios lists them in a written order. walkers makes keyed draws, calls simExp and simLog in its systems, and creates and destroys entities. beacons queues commands whose resolution creates derived entities.

runScenario makes the scenario's world from its schema and seed, populates it, and resolves it. It writes the line 'scenario <name> tick <t> hash <h>', then steps the world n times with the commands QueueCommands queues before each cycle, writing the line again after each. runSave does the same for a save's text, loaded with a given schema and resolved, stepping with no commands and writing 'file <label> tick <t> hash <h>'. t is the world's tick in decimal, and h is hashWorld's value as 16 lowercase hexadecimal digits. writeSimMathLines writes 'exp argument <a> result <r>' for each argument of tests/sim/support/sim_math_reference.h's exp table, then 'log argument <a> result <r>' for each of its log table's, with a and r as bits in 16 lowercase hexadecimal digits. writeDrawLines writes 'draw <i> bits <b> uniform <u> integer-pick <p> double-pick <q>' for each key of tests/sim/support/expected_draws.h, with drawUniform as its bits and each pick over the table's weights. Every line ends with a line feed.

tpj_scenarios [--ticks n] [file...] writes each registered scenario's lines in registration order, then each file's lines in the order given, labeled by its path and loaded with makeParkSchema, then the math lines, then the draw lines. n defaults to 3000. For each run it writes '<source>: <n> ticks in <s> s, <r> ticks per second' to standard error, so timing never enters the compared output. It exits with a nonzero status and a message on standard error for an unknown option, a --ticks value that is not a decimal count, and a file it cannot read or load.

tpj_scenarios --compare left right compares two outputs by firstDifference, which splits each text into lines at line feeds, counts a last line with no line feed, and compares lines byte for byte. It exits with status 0 when the outputs hold the same lines. Otherwise it prints '<left> and <right> first differ at line <k>:' and each file's line there, or '(no line)' after a file's end, and exits with status 1.

scripts/cross-build-check.sh, run from WSL at the repository root, builds tpj_scenarios with linux-debug and windows-debug. It runs both over every tests/parks/*.park file in name order and strips the Windows output's carriage returns. It fails if either run fails, and otherwise compares the outputs with --compare, failing on the first differing line. It prints each stage as it starts. The pre-push hook runs it after the linux-debug tests when the pushed commits touch src/sim/, src/core/, src/scenarios/, cmake/, the top-level CMakeLists.txt or CMakePresets.json, tests/parks/, tests/sim/support/, or the script. It skips it otherwise, and says which it did."

## Files affected

- Create: src/sim/park_schema.h
- Create: src/sim/park_schema.cpp
- Create: src/scenarios/SPEC.md
- Create: src/scenarios/CMakeLists.txt
- Create: src/scenarios/scenarios.h
- Create: src/scenarios/scenarios.cpp
- Create: src/scenarios/walkers.cpp
- Create: src/scenarios/beacons.cpp
- Create: src/scenarios/synthetic.h
- Create: src/scenarios/runner.h
- Create: src/scenarios/runner.cpp
- Create: src/scenarios/main.cpp
- Create: scripts/cross-build-check.sh
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Modify: CMakeLists.txt
- Modify: .githooks/pre-push
- Modify: CLAUDE.md
- Modify: plans/deterministic-simulation/world-as-value/keyed-draws/FEATURE.md (its open question on the expected-draw table is resolved)
- Test pass: files under tests/, written by the test-writer agent: tests/scenarios/ with its CMakeLists.txt and the tpj_scenarios_tests executable, the symbol-check test on the scenarios library, and tests/CMakeLists.txt's add_subdirectory.

## Dependencies

- registered-walk, tick-cycle, keyed-draws, text-saves, and sim-math: merged.
- tests/sim/support/sim_math_reference.h and tests/sim/support/expected_draws.h, generated and checked in by sim-math and keyed-draws.
- The Windows toolchain reachable from WSL through cmake.exe, with the CPM cache populated.

## Out of scope

- The app's --park, --ticks, and --hash options, and building the app's world from makeParkSchema: sketch-a-park.
- Park files in tests/parks/: sketch-a-park checks in the slice's first ones. Until then the script runs the scenarios and the tables alone.
- Release presets in the check: the milestone's deepening candidate.
- Naming the first diverging type and entity: the first-divergence deepening candidate.
- Running the check in GitHub Actions: the backlog's Actions item, which needs a Windows runner.

## Open questions

- Whether 3000 ticks per scenario keeps the pre-push check quick enough once park files carry guests. Resolved by the ticks per second the runner reports after believable-guests lands.
