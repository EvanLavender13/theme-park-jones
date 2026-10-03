# Capability: Focused Tests

## Summary

focused-tests deepens how well the test suite holds to docs/testing.md (decision 0032): each test asserts one lasting property of a public interface, and each property is proven once, by the module that owns it. It is an engineering capability (decision 0028). A player would not notice it missing, but every feature pays for its absence, in tests that break when code changes rather than when it is wrong, and in laws re-proven for each new piece of state.

It owns docs/testing.md's practice across the suite: the law suite that proves the world-as-value laws once over every registered type, the audit that reads a milestone's tests against the rules when the milestone closes, and the reshaping of tests and specs that predate the rules. Its value is that a test failure means a defect, and a new feature costs only the tests of what it owns. It deepens as the game adds kinds of state and as the audit finds shapes the rules did not foresee, and never finishes.

## Foundation criteria

- When a milestone closes, a fresh-context agent reads every test case the milestone added or changed against docs/testing.md and reports each one that breaks a rule, citing the rule. The report goes to Evan, and the milestone is not closed until each reported case is fixed or filed as work. planning-milestones holds the closing procedure, as planning-slices holds a slice's.
- The sim root proves each world-as-value law of docs/testing.md rule 2 once, over the park files in tests/parks/ and the worlds a few park edits reach from them, each edit chosen for a stated reason, such as deleting the path a guest stands on (rule 4). The law suite fails when a component type makeParkSchema registers holds no component in any world it checked, so a newly registered type is either covered or named.
- The sim root's law tests over synthetic schemas join the law suite or are deleted, except those only a synthetic schema can show, such as an unregistered type failing loudly and the hash changing with each kind of field. No test outside the law suite re-proves a world-as-value law.
- Every test case RESEARCH.md lists as rewrite asserts the property listed for it, or is deleted when another test already shows that property.
- Each module's SPEC.md states properties, not vertex orders, formulas, or tuning values, as its contract, except where another module depends on the detail.
- Each repository check in cmake/, the private header, layer, placement, and sim symbol checks, is shown by its run on what it gates and by one planted violation that proves it can fail, and has no other tests. What a check gates is its own: the placement check gates each PLAN.md a commit adds, through the pre-commit hook, and the symbol check's run and planted violation stay beside tpj_sim's tests.
- The suite's changes leave the simulation unchanged: tpj_scenarios's output is identical before and after (decision 0030), and src/ changes only where a spec is trimmed.

## Medium

This capability introduces, samples, and emits no fields or flows. It works on the tests every other capability's fields and flows are checked by.

Every capability's milestones pass its closing audit, and every capability's registered state is covered by its law suite. Reshaping a module's tests and trimming its SPEC.md changes no behavior and no public header; where a spec's contract is narrowed to a property, that module's code is unchanged.

## Principles

- Principles 1, 8, and 10 are backed by the world-as-value laws. The law suite proves them over the real registered types for the first time, so the risk is coverage lost in the move: the held re-proofs are deleted only after the law suite covers the state they checked, and the law suite's own failure on an uncovered type keeps that true as types are added.
- Principle 6 is at risk if a rewritten test reaches another module's internals to state a property. The rewrites test through public headers only, as the test-writer does, and a property that cannot be stated that way is reported, not worked around.
- Principle 2 benefits: each world the law suite checks, the park files and what the chosen edits make of them, candidates included, must copy, save, load, and step as a working world.

## Dependencies

- docs/testing.md and decision 0032: met, on the test-discipline branch until it merges.
- The registered walk, makeParkSchema, and the park files checked in to tests/parks/: met.
- The test-writer and reviewer agents, which the audit and the rewrites use: met.

## Foundation

The foundation is the closing audit, the law suite over registered types, and the reshaping of the tests and specs the first audit found. The audit comes first because it is small and checks everything after it, the law suite and the rewrites included. The law suite comes before the rewrites because it removes the reason features re-proved the laws, and the held re-proofs cannot go until it exists.

## Milestones

1. `closing-audit`: closing a milestone dispatches a fresh-context audit of the tests it added or changed against docs/testing.md, and its findings are fixed or filed before the close. Depends on: none.
2. `registered-laws`: the sim root proves the world-as-value laws once over the park files and edits chosen for a reason, fails on a registered type no checked world holds, folds in its synthetic-schema law tests, and deletes the 39 re-proofs RESEARCH.md lists as held. Depends on: milestone 1.
3. `reshaped-tests`: module by module, each SPEC.md is trimmed to properties and the tests RESEARCH.md lists as rewrite are rewritten by the test-writer, and each repository check keeps its run and one planted violation. Depends on: milestone 2.

## Deepening candidates

- A law that catches a field a type's visitFields leaves out: copy, equality, hash, and save all read through visitFields, so no current law can see the omission. Gated on: a design that compares a type's registered fields with its full layout without reading another module's internals.
- Mutation testing, to find tests that kill no mutant another test does not, and code no test checks. Gated on: a Clang build, since Mull works on LLVM IR and the project builds with GCC.
- Moving what tpj_scenarios still does, the per-tick hash lines and the math and draw tables the cross-build check compares, into tpj_bench, built with the simulation's floating-point flags, and dropping the synthetic scenarios, which predate real park content. The app's --hash option and its tests go with it, and the name scenarios is left for scenarios the player plays (decision 0032).
- An audit of tests a milestone did not touch, for modules whose tests predate docs/testing.md and fall outside the first reshaping. Gated on: a closing audit finding a shape the rules missed.

## Open questions

- Whether the closing audit reads only test cases, or also the acceptance criteria and spec statements that produced them. The first closing audits will show whether failures trace back to criteria often enough to audit them too.

## Research notes

The suite regrew enumerations and law re-proofs because the laws were proven only on synthetic schemas, planning asked every feature to restate them, specs fixed layouts as contracts, and shape rules moved enumeration rather than removing it. Laws written once and checked for every instance, as quickcheck-classes and contract tests do, are the fix the registry already allows. A numeric ratchet is rejected: it is gamed and suite size rightly grows. The guard is instead a closing audit against the written rules by an agent that wrote none of the tests, reporting to Evan. Depth and the audit's inventory of tests to rewrite are in RESEARCH.md.
