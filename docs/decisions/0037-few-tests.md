# 0037. Tests guard determinism and saves; gameplay is checked by playing

Status: Accepted, 2026-10-04. Supersedes 0032, 0033, and 0034.

## Context

The suite holds 446 test cases in 18,483 lines, more than the 15,759 lines of code under src/ it tests. Decision 0032 deleted the cases that broke its rules and set the rest to be reshaped. In the two days since, the rules took two more decision records (0033 and 0034), and reshaping went module by module through four features. Each one needed Evan to sort out judgments the agents made badly or inconsistently. Two runs of the same closing audit each reported findings the other missed, eight in all.

The cause is the process, not the individual tests. For every spec statement and every criterion, an agent judges whether it is a lasting property worth a test, and further agents then check those judgments. Agents are unreliable at that judgment. Each wrong call becomes either a test that breaks when the code changes or a finding Evan has to settle. Adding rules has not made the judgments better, and the work has crowded out gameplay.

Tests are uniquely good at one thing in this game: catching a broken copy, save, hash, or deterministic replay. Those failures are silent, nobody would see them while playing, and decisions 0022 and 0036 rest on them. The mechanical checks on layering, private headers, and the sim's symbols need no judgment either. Almost everything else a module test checks shows up when the game is played.

## Decision

The suite consists of three parts:

- the sim root's own tests, tests/sim/*.cpp: the world checks over the park files, the copy, equality, and hash walk, saves, the tick cycle, keyed draws, entity keys, schemas, sim math, and the floating-point environment;
- the integration tests in tests/integration;
- the mechanical checks: tests/checks and the sim symbol check.

A feature adds a test only in two cases. A test reproduces each bug the feature fixes. A test also covers each consequence across modules that the player depends on and that the feature's FEATURE.md names, stated as cause and effect, as food_loop_test does. State a feature registers is covered by the park world checks once a park file in tests/parks/ holds it, so a feature adds to a park file rather than writing a test. Each test a feature adds is listed in its FEATURE.md, which Evan approves. No agent derives tests from specs or criteria.

A feature is otherwise verified by playing it, by `--capture`, by `tpj_scenarios --compare` when the simulation must not change, and by the release checks.

A SPEC.md tells a reader what its module does and what other modules may rely on. Tests are no longer derived from it. It opens with what the module is for, gives its boundary (what it registers, publishes, and reads, and the headers others call), and then gives one short paragraph, claim first, for each behavior a caller or the player relies on. It states observable behavior, never how it is achieved: that a constructor refuses malformed input belongs in the spec, while the list of inputs it refuses, tie-breaks no caller relies on, construction strategy, formulas, and constants' values belong in the feature's PLAN.md and the code. docs/conventions.md holds this shape. Every spec is rewritten to it once, and features keep it from then on.

docs/testing.md shrinks to these rules plus the core of its old rule 1: a test never expects a tuning constant's value, a tick count, a text layout, or a formula copied from the code.

The test-writer still writes a feature's tests in a separate pass, but only the tests its FEATURE.md lists. The closing audit is retired, and the focused-tests capability is removed, its reshaped-tests milestone unfinished.

## Consequences

These are deleted: tests/sim/guests, medium, operations, park, and routes; every file in tests/sim/support except synthetic_types.h, sim_math_reference.h and .py, planted_exp.cpp, and expect_symbol_rejection.cmake; and tests/render, tests/app, tests/legible, and tests/tools. That leaves 50 test cases in about 2,700 lines, plus the mechanical checks. tests/parks/ stays, because the world checks, the cross-build check, and the runtime report all read it.

Regression protection on module internals is lost, such as route distance, field sampling, and the renderer. A defect there now shows up in play, in the park world checks, or in a scenarios comparison. Once found, it gets a test reproducing it, so the suite grows only where real defects have occurred.

CLAUDE.md, docs/testing.md, docs/conventions.md, the test-writer and reviewer agents, and the implementing-features, planning-features, planning-milestones, planning-slices, and reviewing skills are updated to match. focused-tests deepening candidates that still matter, such as moving tpj_scenarios's remaining work into tpj_bench, move to plans/BACKLOG.md.
