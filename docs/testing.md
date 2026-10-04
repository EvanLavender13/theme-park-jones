# Testing

What gets a test, and what a test may assert (decision 0037). This document ranks below docs/principles.md and above every spec, plan, skill, and agent. It changes only through a decision record.

Tests guard what playing the game cannot show: that a world copies, saves, hashes, and replays exactly, on every build players run. Everything else is checked by playing the game, by `--capture`, and, for work that must leave the simulation unchanged, by `tpj_scenarios --compare`.

## The suite

The suite has three parts. The sim root's tests in tests/sim prove the world-as-value standards over the park files in tests/parks/ and the worlds a few edits reach from them: a copy equals its world, a save loads back and resolves equal, the hash covers all registered state, a candidate equals the world that commits the same commands, and two runs give the same state. The same directory tests the pieces those standards rest on: the tick cycle, keyed draws, entity keys, schemas, sim math, and the floating-point environment. The integration tests in tests/integration each check one consequence across modules that the player depends on, stated as cause and effect, such as cutting a shop's supply leaving its guests hungrier. The checks in tests/checks, and the sim symbol check, keep the code's layers, private headers, and the sim's runtime independence.

## What a feature adds

A feature adds a test in two cases only, and its FEATURE.md lists each one:

- a test that reproduces a bug the feature fixes;
- an integration test of a consequence across modules that the feature exists to make, stated as cause and effect.

State a feature registers is covered by the park world checks once a park file in tests/parks/ holds it, so the feature adds it to a park file and writes no test for it. The world checks fail when a registered type is held by no checked world.

No test is derived from a spec or from a feature's other criteria, and no test checks a single module's internals. Test utilities, such as tpj_bench, tpj_scenarios, the park generators, and the scripts, get no tests and are checked by running them.

## What a test may assert

A test's expected outcome never depends on a tuning constant's value, a tick count, an exact text layout, or a formula copied from the code, and neither does its setup. An integration test compares a park with the same park changed, never a tuned run with a remembered result.
