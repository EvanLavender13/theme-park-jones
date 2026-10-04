# Research: sim-root-reshaped

## Which statements of src/sim/SPEC.md are trimmed?

Five statements fix a formula or a tuning value that nothing outside the sim root depends on. The tick's length, 1/30 s, is used everywhere by the name SIM_TICK_SECONDS, never by its number. The hash folding the walk's words with SplitMix64's finalizer, and drawBits folding its five words with sim/mix.h's Hasher, are formulas: callers need only that each is a function of its words alone. drawUniform as the draw's top 53 bits times 2^-53 is a formula whose property is a double in [0, 1). drawPick's two methods, the high 64 bits of the draw times the total for integer weights and a scaled running sum for double weights, are formulas behind three properties that stay: a pick takes one draw, picks in proportion to weight with a stated bound on the integer bias, and never picks a zero weight.

Kept: the save format and the key counter starting at 1, which park files on disk depend on; the denylist and report line of cmake/check_sim_symbols.cmake, which the check's planted violation reads, and which checks-reshaped leaves unchanged; simExp's and simLog's IEEE special values and their argument thresholds, which IEEE fixes rather than the code; and simExp's and simLog's provenance, which their licence requires and no test reads. loadWorld's list of what it refuses stays, since a refusal is a rule the spec states, shown by one refused input.

No test outside the inventory pins a trimmed statement. draw_test's "drawUniform is the draw's top 53 bits times 2^-53" is the one that does, and it is an inventory entry.

## Which test support goes with the trim?

tests/sim/support/expected_draws.py computes each draw's expected bits, uniform, and picks from the formulas the trim removes, and writes them into expected_draws.h. No test reads those expected values any more; tpj_scenarios reads only the table's keys and weights, to write its draw lines, which the cross-build check compares between builds. So the generator is deleted, and the header keeps its keys and weights as a hand-written table. tpj_scenarios's output is unchanged, since it never read the expected values.

Rejected: keeping the generator for a future reference test — a test of the draw's bits against a reference is a change-detector of the formula the trim removes. Renaming expected_draws.h — it would change src/scenarios's code, which this milestone leaves alone; only its spec's mention of a generated table changes.

## How does the test-writer get the inventory's entries?

The test-writer's rewrite mode takes findings that name cases and the property each should assert, which an inventory entry is, but its wording names only closing-audit findings. implementing-features' test pass has only the dispatch that writes tests from FEATURE.md's criteria, which would add new cases beside the old ones rather than replace them. So the feature adds a rewrite dispatch to the test pass, carrying the module's entries verbatim, and widens rewrite mode to take them. The dispatch passes the entries rather than a path to plans/focused-tests/RESEARCH.md, which the test-writer may not read.
