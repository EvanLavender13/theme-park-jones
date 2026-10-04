---
name: implementing-features
description: Use whenever the user wants to build a feature whose PLAN.md exists and is approved. Updates the specs and interfaces, dispatches the separate test pass for the few tests FEATURE.md lists, and executes the implementation tasks. Confirms each criterion by play, capture, comparison, or test, dispatches the fresh-context reviewer on the diff, and commits the feature once via commit-hygiene. Stops on any deviation from the plan. Does not modify scope, acceptance criteria, or the test pass's tests. If no approved plan exists, routes to planning-features.
---

# Implementing Features

## Purpose

Carry out an approved PLAN.md task by task. Any tests the feature lists are written in a separate pass from the implementation, and a fresh-context reviewer checks the result. The feature is committed once, at the end.

## Hard gate

A PLAN.md must exist at plans/<capability-slug>/<milestone-slug>/<feature-slug>/PLAN.md, and the user must have approved it. If not, route to `planning-features` and stop.

Don't change acceptance criteria or scope during implementation. Don't edit, weaken, or delete tests written by the test pass. If a task or a test shows that the plan is wrong, follow the deviation procedure.

## Checklist

Create a task for each item. Complete them in order.

1. Read the PLAN.md and the parent FEATURE.md in full.
2. Create a feature branch. If you're on the default branch, run `git switch -c <feature-slug>`. If you're already on the branch named `<feature-slug>`, stay on it. If you're on any other branch, stop and ask the user.
3. Do the spec and interface tasks. The windows-debug build must succeed with the stub definitions.
4. Run the test pass if FEATURE.md's Tests section lists tests. If it lists none, skip this step.
    - First, check each listed test against docs/testing.md. A feature adds only two kinds of test: one that reproduces a bug the feature fixes, and an integration test of a cross-module consequence the feature exists to make. Fix FEATURE.md before going on.
    - Then dispatch the `test-writer` agent with the Agent tool, using the template below.
    - When it returns, confirm that its tests build. Confirm that the tests for unimplemented behavior fail for that reason, not because of a compile error or a broken harness.
    - Settle each gap its report lists before implementing. If a gap follows from the principles and specs, record it as a spec decision in FEATURE.md. If it needs a design choice, take it to the user. If it can wait, record it in FEATURE.md's Open questions. Don't leave a gap only in the report.
5. Do the implementation tasks in order. Perform each task's steps and run its verification before starting the next. Verify narrowly: build only the targets the task changes, and run only the test executables that cover it, filtered to the relevant files or test names. The full build and ctest belong to step 6.
6. Confirm every acceptance criterion in FEATURE.md by the means it names: play, a `--capture` run, a `tpj_scenarios --compare` showing the simulation unchanged, or a listed test.
    - If the feature registers state, confirm that a park file in tests/parks/ holds it, so the park world checks cover it.
    - The full windows-debug build must have no warnings, `ctest.exe --preset windows-debug` must pass, and `scripts/tidy.sh` must be clean.
    - Linux is checked at release by scripts/release-check.sh, not per feature (decision 0030).
    - Keep the output of each check for the report.
7. Review. Stage everything with `git add -A`. Dispatch the `reviewer` agent through the `reviewing` skill, with the staged diff as the artifact. Pass FEATURE.md, the changed SPEC.md files, and docs/principles.md as context. Present the findings verbatim. The user decides which to act on, and fixes go through the deviation procedure.
8. Commit the feature once, using the `commit-hygiene` skill.
9. Merge into the default branch: `git switch main`, then `git merge --no-ff <feature-slug> -m "Merge: <feature-slug>" -m "<Co-Authored-By trailer>"`, then `git branch -d <feature-slug>`. The pre-push hook blocks a push that fails its checks.
10. Reconcile the backlog: use `maintaining-backlog` to drop any item the feature implemented or made moot.
11. Report back in the format below.

## Test pass dispatch

Put only the paths and the instruction in the dispatch prompt. Don't describe the implementation, the plan, or what you expect the tests to find.

```
Write the tests this feature lists.

Feature: plans/<capability-slug>/<milestone-slug>/<feature-slug>/FEATURE.md
Specs: <SPEC.md paths the feature changes or depends on>
Public headers: <header paths from the interface tasks>

Follow your standard procedure and return your standard report.
```

## Deviation procedure

A deviation is any of these:

- A task fails.
- The actual output differs from the expected output.
- A referenced file is missing or has a different shape.
- A step depends on a precondition the plan didn't establish.
- A test from the test pass looks wrong.
- The work needs something that doesn't fit fields, flows, or encapsulation.

When a deviation occurs:

1. Stop. Don't improvise.
2. Identify the cause: a typo, a wrong assumption, a missing dependency, an incompatibility with existing code, a test that contradicts the spec, or a principle conflict.
3. Classify the severity.
    - Minor (a typo, an off-by-one, a wrong flag, a trivially wrong path in the plan): fix the plan inline, note the fix, and continue.
    - Major (a wrong approach, a missing task, a scope error, a broken assumption, a disputed test, a principle conflict, a needed exception): stop and report. Let the user decide whether to amend the plan, re-plan, or abandon. Never fix a disputed test yourself. The user decides whether the test or the spec is wrong.

## Commit discipline

Make one commit per feature. Commit after the acceptance criteria pass and the user has dealt with the review findings. Use the `commit-hygiene` skill. Don't commit per task, and don't commit incomplete work.

## Report format

```
Feature `<feature-slug>` completed.

What was built:
[One paragraph.]

Evidence:
[Build and test output summary, and each criterion's capture, comparison, or play check.]

Deviations:
- [List, or "none"]

Review findings acted on:
- [List, or "none"]

Items added to the backlog:
- [List, or "none"]

Items dropped from the backlog (now shipped):
- [List, or "none"]

Open questions:
- [List, or "none"]
```
