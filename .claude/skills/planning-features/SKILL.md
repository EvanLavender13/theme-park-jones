---
name: planning-features
description: Use whenever the user wants to plan a feature inside an approved milestone. Reads the parent MILESTONE.md, the principles, and the module specs the feature touches, explores the code, researches implementation patterns, brainstorms with the user one question at a time, and writes FEATURE.md (the contract the separate test pass derives tests from) and PLAN.md (spec, interface, and implementation tasks the implementer follows without further design decisions). Stops at the plan; does not write code or invoke implementing-features. Use this even for small features.
---

# Planning Features

## Purpose

Take one feature from a milestone brief. Produce two artifacts:

- FEATURE.md, defining the feature precisely enough that tests can be derived from it without seeing the implementation.
- PLAN.md, with bite-sized tasks the implementer can follow without making design decisions.

## Hard gate

Do not write code. Do not scaffold files. Do not invoke `implementing-features`. The parent MILESTONE.md must exist at plans/<capability-slug>/<milestone-slug>/MILESTONE.md.

`researching`, `maintaining-backlog`, and `reviewing` are the only callable skills from here: `researching` to retain research at this node, `maintaining-backlog` to deposit speculative items and to drop items this feature draws from or supersedes, and `reviewing` for the review before approval.

## Checklist

Create a task for each item. Complete in order.

1. Read the parent MILESTONE.md in full and confirm which feature you are planning.
2. Read the grandparent CAPABILITY.md and confirm scope alignment.
3. Explore related code: docs/principles.md, the SPEC.md of every module the feature touches, relevant decision records, and the files the feature will touch or depend on. Identify patterns to follow.
4. Reconcile the backlog: drop the item this feature draws from, and any item it supersedes, via `maintaining-backlog`.
5. Research implementation patterns via the `researching` skill. It writes findings to plans/<capability-slug>/<milestone-slug>/<feature-slug>/RESEARCH.md.
6. Ask clarifying questions, one at a time. Cover acceptance criteria, edge cases, and integration boundaries.
7. Name the simplest version that works. Strip polish, edge cases, and nice-to-haves and send them to `maintaining-backlog`.
8. Map the medium: the fields and flows the feature samples, emits, draws, or supplies. For each principle the feature can violate in code (1 to 6, 8, 10), name the criterion that shows the feature keeps it, or the existing standard that already covers it (docs/testing.md rule 2); never restate an owned standard as a criterion. Apply the principles gate from `planning-overview`.
9. Draft the spec change: the exact sentences to add or change in each affected SPEC.md, and the public interface (header declarations) the feature exposes.
10. Place each behavior the feature adds, under decision 0027: the module and component that own it, and why. A behavior that fits no existing component gets a new one. src/app/main.cpp and src/scenarios/main.cpp take no new concern until sound-architecture restructures them; a feature that must touch one says what it adds there, and that it is composition only.
11. List files to touch, with specific paths, each marked create or modify.
12. Decompose into bite-sized tasks, each one action of two to five minutes, in the task structure below.
13. Write FEATURE.md at plans/<capability-slug>/<milestone-slug>/<feature-slug>/FEATURE.md.
14. Write PLAN.md at the same path.
15. Self-review: every acceptance criterion is testable from FEATURE.md and the specs alone and keeps docs/testing.md, or, for a test utility, is a check the implementer runs with its command and expected output; every spec statement the feature adds or changes is covered by a criterion; every task has exact paths, exact commands, and expected outputs; the Placement section places every behavior the tasks add. Fix inline.
16. Review: stage FEATURE.md, PLAN.md, and RESEARCH.md, then dispatch the reviewer via `reviewing`, with MILESTONE.md, the SPEC.md of every module the feature touches, and docs/principles.md as context. Handle the findings as `reviewing` describes for planning briefs: fix Minor ones and settled Major ones directly, bring the user only Major findings that need a design choice, and report the rest in one line.
17. Ask the user to approve both artifacts.

## Process notes

Assume the implementer has no project context. Write paths and commands in full. Do not say "add validation" when you can write the validation itself.

Tests come from a separate pass. The implementer does not write the feature's tests; the `test-writer` agent derives them from FEATURE.md, the specs, and the principles, without reading PLAN.md. So FEATURE.md must carry everything a test needs: acceptance criteria stated as properties, and the edge cases the spec names.

Test utilities get no tests (docs/testing.md rule 6). Their acceptance criteria are checks the implementer runs, each naming the command and the output that shows it holds. Their PLAN.md has no interface stubs written for tests and no test pass task, and its confirming task runs the tool and puts the output in the feature's report. Do not specify a tool's edge cases, such as every malformed input, merely so they can be tested.

Criteria follow docs/testing.md. Each states one lasting property the feature owns, as a standard or invariant of the public interface, with concrete values only where the value is the contract. What a function refuses is a rule the spec states; the criterion is the one property behind it. A feature needs a handful; more than about eight means the feature is too big or its criteria are listing examples. PLAN.md contains no test code.

Order of work inside PLAN.md: spec tasks first (update SPEC.md), then interface tasks (public headers with stub definitions that compile, so tests can be written against them), then the test pass (one task that says to run it, no content; none for a test utility), then implementation tasks, then the commit.

Every PLAN.md has a Placement section. The pre-commit hook refuses a commit that adds a PLAN.md without a line reading "## Placement", and the reviewer judges what the section says.

Graybox first. When a feature combines mechanics and presentation, deliver mechanics first and add presentation in later tasks.

One commit per feature. The final task ends with the feature's single commit via the `commit-hygiene` skill. No other task commits.

One concept per task. A task that combines two unrelated changes hides a dependency. Split it.

Defer everything optional. Anything that does not contribute to acceptance criteria goes in FEATURE.md's Out of scope section and into the backlog.

## Task structure

```markdown
### Task N: <Name>

Files:
- Create: `exact/path/to/file.ext`
- Modify: `exact/path/to/existing.ext:123-145`

Step 1: <Action>

[Exact code, command, or content.]

Step 2: <Action>

Run: `exact command`
Expected: <exact expected output>
```

Implementation tasks verify with the narrowest windows-debug build and test run that covers them: build the test executables for the modules the task changes (`cmake.exe --build --preset windows-debug --target tpj_sim_tests`) and run them directly, filtered to the relevant files or test names (`build/windows-debug/tpj_sim_tests.exe -# "[#edits_test]"`), stating which of the test pass's tests are expected to pass after the task. The full windows-debug build, its ctest run, and `scripts/tidy.sh` run once, when the feature's acceptance criteria are confirmed (decision 0030).

## FEATURE.md format

```markdown
# Feature: <Name>

## Summary

One paragraph. What this feature does.

## Acceptance criteria

The properties the feature guarantees, each stated once and testable through the public interface. Concrete values only where the value is the contract.

## Medium

Fields sampled and emitted, flows drawn and supplied, and the entities on the other side of each.

## Spec changes

The SPEC.md files this feature changes and what they will say.

## Files affected

Paths to create or modify.

## Dependencies

Code, libraries, content, or sibling features required.

## Out of scope

What this feature deliberately does not do. Items that remain valuable belong in the backlog.

## Open questions

Each with what could resolve it.
```

## PLAN.md format

```markdown
# Implementation Plan: <Feature Name>

## Goal

One sentence describing what this plan delivers.

## Approach

Two to three sentences on the technical approach.

## Placement

Decision 0027 places each behavior this feature adds:

- <Behavior>: <module>, <component and its header>. <Why that component owns it.>

## Tasks

[Spec tasks, interface tasks, the test pass task, implementation tasks, and the final commit, in the structure above.]
```

## Next step

The user invokes `implementing-features` against the PLAN.md.
