# 0032. Tests follow one set of rules, held in docs/testing.md

Status: Accepted, 2026-10-03

## Context

An audit read all 754 test cases in tests/ against one written standard. 287 passed it as written, 194 asserted a real property in the wrong shape, 245 should not exist, and 28 tested tpj_scenarios, a checking tool. Of the 245, about 65 re-proved a law another module owns, about 65 enumerated inputs or entry points, about 56 pinned tuning, text, vertex order, or a copied formula, about 47 tested that one function forwards to another, and about 14 tested that a function taking `const World &` leaves the world unchanged.

The causes were upstream of the tests. The world-as-value laws were proven only on hand-made synthetic schemas, never over the types the park schema registers, so every feature that added state re-proved them for that state. planning-features asked each feature to name a test for every principle it could break, which for principles 1, 8, and 10 is always the same law. Specs fixed tessellation, formulas, and constants as contracts, so tests of them were change-detectors. The test-writer saw one feature at a time, so it could not know what the suite already proved. The rules meant to prevent this were scattered across CLAUDE.md, docs/conventions.md, the test-writer and reviewer agents, and the planning skills, each worded differently, and each new problem added another rule to whichever file was nearest.

## Decision

docs/testing.md holds the rules for what a test is. It ranks below docs/principles.md and above every spec, plan, skill, and agent. Skills and agents cite its rules by number and do not restate them. It changes only through a decision record.

The world-as-value laws are proven once, by the sim root, over every type the park schema registers. A feature that registers state inherits them and writes no criterion for them. FEATURE.md has no Principle checks section; a feature's criteria state only the properties the feature itself owns.

tpj_scenarios, its runner, and the park generators are test utilities, checked by running them.

## Consequences

Every test case that fails docs/testing.md is deleted, except the tests that re-prove a world-as-value law over real park state. Those remain until the sim root's law suite runs over the park schema's registered types, and are deleted when it lands. The tests that assert a real property in the wrong shape are rewritten by the test-writer against docs/testing.md. Specs that fix layouts or constants as contracts are trimmed to the properties beneath them.

A capability owns the suite's discipline from here: the law suite over registered types, the rewrites, the spec trims, and checks that keep the suite from regrowing the shapes this audit removed. That capability also plans moving what tpj_scenarios still does, the per-tick hash lines and the math and draw tables the cross-build check compares, into tpj_bench, which frees the name for scenarios the player plays.
