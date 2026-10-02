---
name: implementing-features
description: Use whenever the user wants to build a feature whose PLAN.md exists and is approved. Updates the specs and interfaces, dispatches the separate test pass to the test-writer agent, executes the implementation tasks until those tests pass, dispatches the fresh-context reviewer on the diff, and commits the feature once via commit-hygiene. Stops on any deviation from the plan. Does not modify scope, acceptance criteria, or the test pass's tests. If no approved plan exists, routes to planning-features.
---

# Implementing Features

## Purpose

Execute an approved PLAN.md task by task, with tests written in a separate pass from the implementation and the result checked by a fresh-context reviewer. The feature commits once, at the end.

## Hard gate

A PLAN.md must exist at plans/<capability-slug>/<milestone-slug>/<feature-slug>/PLAN.md and the user must have approved it. If not, route to `planning-features` and stop.

Do not modify acceptance criteria or scope during implementation. Do not edit, weaken, or delete tests written by the test pass. If a task or a test reveals the plan is wrong, follow the deviation procedure.

## Checklist

Create a task for each item. Complete in order.

1. Read the PLAN.md and the parent FEATURE.md in full.
2. Create a feature branch: if on the default branch, run `git switch -c <feature-slug>`. If already on the branch named `<feature-slug>`, stay on it. If on any other branch, stop and ask the user.
3. Do the spec and interface tasks. The windows-debug build must succeed with the stub definitions.
4. Run the test pass, unless the feature is a test utility, such as tpj_bench, tpj_bench_report, or a script, which gets no tests ever (planning-features); its criteria are confirmed by running it in step 6. First, check FEATURE.md's acceptance criteria and principle checks: each states one property, with no lists of example inputs and nothing that reads as every function against every case; every spec statement the feature adds or changes is covered by one of them; and none that was in an earlier version of the list has been dropped without a reason. Fix FEATURE.md before going on, since the tests can only be as good as the criteria. Then dispatch the `test-writer` agent with the Agent tool, using the template below. When it returns, confirm its tests build, and that the ones for unimplemented behavior fail for that reason and not because of a compile error or a broken harness. Then settle each gap its report lists before implementing: one that follows from the principles and specs becomes a spec decision in FEATURE.md; one that needs a design choice goes to the user; one that can wait is recorded in FEATURE.md's Open questions with what could resolve it. A gap is never left only in the report.
5. Do the implementation tasks in order. Perform each task's steps and run its verification before starting the next. Verify narrowly: build only the targets the task changes and run only the test executables that cover it, filtered to the relevant files or test names; the full build and ctest belong to step 6.
6. Confirm every acceptance criterion in FEATURE.md: the full windows-debug build has no warnings, `ctest.exe --preset windows-debug` passes, and `scripts/tidy.sh` is clean. Linux is checked at release by scripts/release-check.sh, not per feature (decision 0030). When the feature has visible output, check it with a `--capture` run. For a test utility, run each check its criteria name and keep the output for the report.
7. Review. Stage everything with `git add -A` and dispatch the `reviewer` agent via the `reviewing` skill, with the staged diff as the artifact and FEATURE.md, the changed SPEC.md files, and docs/principles.md as context. Present the findings verbatim. The user decides which to act on; fixes go through the deviation procedure.
8. Commit the feature once, via the `commit-hygiene` skill.
9. Merge into the default branch: `git switch main`, then `git merge --no-ff <feature-slug> -m "Merge: <feature-slug>" -m "<Co-Authored-By trailer>"`, then `git branch -d <feature-slug>`. The pre-push hook gates what leaves the machine.
10. Reconcile the backlog: via `maintaining-backlog`, drop any item the feature implemented or made moot.
11. Report back in the format below.

## Test pass dispatch

The dispatch prompt contains only paths and the instruction. Do not describe the implementation, the plan, or what you expect the tests to find.

```
Write the tests for this feature.

Feature: plans/<capability-slug>/<milestone-slug>/<feature-slug>/FEATURE.md
Specs: <SPEC.md paths the feature changes or depends on>
Public headers: <header paths from the interface tasks>

Follow your standard procedure and return your standard report.
```

## Deviation procedure

A deviation is any of: a task fails, the actual output differs from the expected output, a referenced file is missing or differently shaped, a step depends on a precondition the plan did not establish, a test from the test pass looks wrong, or the work needs something that does not fit fields, flows, or encapsulation.

When a deviation occurs:

1. Stop. Do not improvise.
2. Identify the cause: typo, wrong assumption, missing dependency, incompatibility with existing code, a test that contradicts the spec, or a principle conflict.
3. Classify severity.
    - Minor (typo, off-by-one, wrong flag, trivially wrong path in the plan): fix the plan inline, note the fix, and continue.
    - Major (wrong approach, missing task, scope error, broken assumption, a disputed test, a principle conflict, a needed exception): stop, report, and let the user decide whether to amend the plan, re-plan, or abandon. A disputed test is never fixed by the implementer; the user decides whether the test or the spec is wrong.

## Commit discipline

One feature, one commit, after the acceptance criteria pass and the user has dealt with the review findings. Use the `commit-hygiene` skill. Do not commit per task. Do not commit incomplete work.

## Report format

```
Feature `<feature-slug>` completed.

What was built:
[One paragraph.]

Evidence:
[Build and test command output summary: tests run, passed, failed.]

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
