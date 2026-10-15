---
name: planning-features
description: Use whenever the user wants to plan a feature inside an approved milestone. Reads the parent MILESTONE.md, the principles, and the module specs the feature touches, explores the code, researches implementation patterns, and brainstorms with the user one question at a time. Writes FEATURE.md (the contract: observable criteria and the few tests the feature adds) and PLAN.md (spec, interface, and implementation tasks the implementer can follow without making further design decisions). Stops at the plan; does not write code or invoke implementing-features. Use this even for small features.
---

# Planning Features

## Purpose

Take one feature from a milestone brief. Produce two artifacts:

- FEATURE.md says what the feature does as the player and other modules see it, how each criterion is confirmed, and the few tests it adds.
- PLAN.md has small tasks the implementer can follow without making design decisions.

## Hard gate

Do not write code. Do not scaffold files. Do not invoke `implementing-features`. The parent MILESTONE.md must exist at plans/<capability-slug>/<milestone-slug>/MILESTONE.md.

From here you may call only three skills:

- `researching`, to keep research at this node.
- `maintaining-backlog`, to deposit speculative items and to drop items this feature draws from or supersedes.
- `reviewing`, for the review before approval.

## Checklist

Create a task for each item. Complete them in order.

1. Read the parent MILESTONE.md in full and confirm which feature you are planning.
2. Read the grandparent CAPABILITY.md and confirm the feature fits its scope.
3. Explore related code. Read docs/principles.md, the SPEC.md of every module the feature touches, and the relevant decision records. Read in full every file the feature will modify, and the code that each criterion's setup depends on. Identify patterns to follow.
4. Reconcile the backlog with `maintaining-backlog`. Drop the item this feature draws from, and any item it supersedes.
5. Research implementation patterns with the `researching` skill. It writes findings to plans/<capability-slug>/<milestone-slug>/<feature-slug>/RESEARCH.md.
6. Ask clarifying questions, one at a time. Cover acceptance criteria, edge cases, and integration boundaries.
7. Name the simplest version that works. Strip polish, edge cases, and nice-to-haves, and send them to `maintaining-backlog`.
8. Map the medium: the fields and flows the feature samples, emits, draws, or supplies. If the feature registers state, the park world checks cover that state once a park file holds it. Name that park file. Apply the principles gate from `planning-overview`.
9. Draft the spec change. Write the exact sentences to add or change in each affected SPEC.md. State observable behavior only, in the shape docs/conventions.md gives. Also draft the public interface (header declarations) the feature exposes. How the behavior is achieved goes in PLAN.md.
10. Place each behavior the feature adds, following decision 0027. Name the module and component that own it, and say why. If a behavior fits no existing component, give it a new one. Don't add a new concern to src/app/main.cpp or src/scenarios/main.cpp until sound-architecture restructures them. If a feature must touch one of them, say what it adds there and that it is composition only.
11. List the files to touch, with specific paths. Mark each one create or modify.
12. Break the work into small tasks, each one action of two to five minutes, using the task structure below.
13. Write FEATURE.md at plans/<capability-slug>/<milestone-slug>/<feature-slug>/FEATURE.md.
14. Write PLAN.md at the same path.
15. Self-review, and fix problems inline. Check that:
    - Every acceptance criterion is observable and names how it is confirmed.
    - Every listed test is one that docs/testing.md allows a feature to add.
    - Every spec statement the feature adds is observable behavior, not implementation.
    - Every task has exact paths and exact content.
    - The Placement section places every behavior the tasks add.
    - Every claim about existing code has been checked by reading that code. This includes what a function does, how a test reaches its state, and what a command prints. Run each read-only command, and copy its Expected line from what it printed.
16. Review. Stage FEATURE.md, PLAN.md, and RESEARCH.md, then dispatch the reviewer with `reviewing`. Give it MILESTONE.md, the SPEC.md of every module the feature touches, and docs/principles.md as context. Handle the findings as `reviewing` describes for planning briefs. Fix Minor findings and settled Major findings yourself. Bring the user only the Major findings that need a design choice, and report the rest in one line.
17. Ask the user to approve both artifacts.

## Process notes

Assume the implementer has no project context. Write paths and commands in full. Don't write "add validation" when you can write the validation itself.

Write each criterion as what the feature does as the player or another module observes it. Say how each one is confirmed:

- By playing. Say what to do and what to see.
- By a `--capture` run.
- By `tpj_scenarios --compare` showing the simulation unchanged.
- By a check the implementer runs. Give its command and expected output.
- By a listed test.

Give a feature a handful of criteria. If it has more than about eight, the feature is too big, or its criteria are listing examples.

Follow docs/testing.md for tests. List a test only to reproduce a bug the feature fixes, or as an integration test of a cross-module consequence the feature exists to make. State that test as cause and effect. Most features list no tests. Because the `test-writer` agent writes the listed tests in a separate pass without reading PLAN.md, each entry in the Tests section must say what to set up, what to change, and what difference to expect. Don't put test code in PLAN.md.

Order the work in PLAN.md like this:

1. Spec tasks, which update SPEC.md.
2. Interface tasks. These add public headers with stub definitions that compile, so the listed tests can be written against them.
3. The test pass. This is one task that says to run it, with no content. Leave it out when no tests are listed.
4. Implementation tasks.
5. The commit.

Give every PLAN.md a Placement section. If a commit adds a PLAN.md without a line reading "## Placement", the pre-commit hook refuses it. The reviewer judges what the section says.

Graybox first. When a feature combines mechanics and presentation, deliver the mechanics first and add presentation in later tasks.

Make one commit per feature. End the final task with the feature's single commit, made with the `commit-hygiene` skill. Don't commit in any other task.

Put one concept in each task. If a task combines two unrelated changes, split it, because the combination hides a dependency.

Defer everything optional. If something doesn't contribute to the acceptance criteria, put it in FEATURE.md's Out of scope section and in the backlog.

## Task structure

```markdown
### Task N: <Name>

Files:
- Create: `exact/path/to/file.ext`
- Modify: `exact/path/to/existing.ext:123-145`

Step 1: <Action>

[Exact code, command, or content.]

Step 2: <Action>

[Exact code, command, or content.]
```

Don't give implementation tasks a build or a check. The implementer writes all the code, builds once, and then confirms the criteria. Put every check in the one confirmation task: the full windows-debug build, its ctest run, `scripts/tidy.sh`, and each criterion's means, each run once (decision 0030). If the feature must leave the simulation unchanged, record the scenarios once before the change and compare once after it.

## FEATURE.md format

```markdown
# Feature: <Name>

## Summary

One paragraph. What this feature does.

## Acceptance criteria

What the feature does as the player or another module observes it. Each criterion says how it is confirmed: play, capture, scenarios comparison, a check with its command, or a listed test.

## Tests

The tests the feature adds. Each is a bug reproduction or an integration test of a cross-module consequence, stated as setup, change, and expected difference. Usually "None."

## Medium

Fields sampled and emitted, flows drawn and supplied, and the entities on the other side of each.

## Spec changes

The SPEC.md files this feature changes and what they will say.

## Files affected

Paths to create or modify.

## Dependencies

Code, libraries, content, or sibling features required.

## Out of scope

What this feature deliberately does not do. If an item still has value, put it in the backlog.

## Open questions

Each with what could resolve it.
```

## PLAN.md format

```markdown
# Implementation Plan: <Feature Name>

## Goal

One sentence saying what this plan delivers.

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
