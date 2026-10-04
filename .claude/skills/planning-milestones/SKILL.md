---
name: planning-milestones
description: Use whenever the user wants to plan a milestone inside an approved capability. Reads the parent CAPABILITY.md and the principles, explores related work, researches tactical patterns, and brainstorms with the user one question at a time. Names the core feature, maps feature dependencies and the fields and flows each feature uses, and writes MILESTONE.md with a feature decomposition. Stops at the brief; does not write feature plans or code. Use this even when the milestone seems straightforward.
---

# Planning Milestones

## Purpose

Take one milestone from a capability brief. Produce MILESTONE.md. It defines the milestone's scope, core feature, medium, dependencies, and feature decomposition.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved MILESTONE.md. Do not write a FEATURE.md or code. The parent CAPABILITY.md must exist at plans/<capability-slug>/CAPABILITY.md.

From here you may call only three skills:

- `researching`, to keep research at this node.
- `maintaining-backlog`, to deposit speculative items and to drop items this milestone draws from or supersedes.
- `reviewing`, for the review before approval.

While closing a milestone, you may also call `commit-hygiene` and dispatch the `test-writer` agent. You may rewrite the acceptance criterion or SPEC.md statement that a finding names. You may delete test cases. Edit test code only to fix a build, test, or tidy failure, and don't change what a case asserts.

## Checklist

Create a task for each item. Complete them in order.

1. Read the parent CAPABILITY.md in full and treat it as settled. Confirm which milestone you are planning and where it sits in the dependency order.
2. Read sibling milestones if there are any, and note how they relate. If the milestone is a slice member, read its SLICE.md in full. The milestone must deliver its share of the scenario. It must produce or consume exactly the cross-capability fields and flows that the slice's medium map assigns to its capability. It may add fields and flows internal to its own capability. If the cross-capability medium needs to change, take that back to `planning-slices`; don't change it in the milestone.
3. Explore related context. Read docs/principles.md, the SPEC.md of the modules the milestone touches, and the relevant decision records. Also read the code each acceptance criterion depends on; don't infer it from specs or plans. Identify patterns to follow.
4. Reconcile the backlog with `maintaining-backlog`. Drop the item this milestone draws from, and any item it supersedes.
5. Research tactical patterns with the `researching` skill. It writes findings to plans/<capability-slug>/<milestone-slug>/RESEARCH.md.
6. Ask clarifying questions, one at a time. Cover scope boundaries, acceptance criteria, and feature granularity.
7. Propose two or three ways to split the milestone into features, each with trade-offs.
8. Map the medium: for each feature, the fields and flows it samples, emits, draws, or supplies. Apply the principles gate from `planning-overview`. If a feature reaches into another feature's internals, rework it before you list it.
9. Name the core feature: the first feature that makes the milestone partly usable.
10. Map feature dependencies, bottom-up.
11. Separate committed features from speculative ones. Commit only the features that current evidence supports, and send the rest to `maintaining-backlog`.
12. Write MILESTONE.md at plans/<capability-slug>/<milestone-slug>/MILESTONE.md in the format below.
13. If the milestone is not already in the parent CAPABILITY.md's Milestones list, add it.
14. Self-review, and fix problems inline. Look for placeholders, contradictions, scope drift, vague acceptance criteria, and interactions that bypass the medium. Check every claim about existing code, and every count or listing taken from the repository, by reading that code or running the read-only command.
15. Review. Stage MILESTONE.md and RESEARCH.md, then dispatch the reviewer with `reviewing`. Give it CAPABILITY.md as context, and SLICE.md too for a slice member. Handle the findings as `reviewing` describes for planning briefs. Fix Minor findings and settled Major findings yourself. Bring the user only the Major findings that need a design choice, and report the rest in one line.
16. Ask the user to approve.

## Closing a milestone

Close a milestone when every feature in its Features list has merged into main.

1. Confirm that the windows-debug build has no warnings, `ctest.exe --preset windows-debug` passes, and `scripts/tidy.sh` is clean.
2. Play the milestone's acceptance criteria in the app, or check each one by the means it names. Report what you saw to Evan.
3. Set the MILESTONE.md's Status to complete, and commit with `commit-hygiene`.

## Process notes

Stay inside the parent's scope. If work belongs in another milestone or capability, name it and hand it back instead of absorbing it.

Size features so each can be built in hours to days. Split anything bigger, and merge anything smaller.

Plan only to the next loop. Uncertainty compounds, and it invalidates the rest.

A weak core feature fails the brief. If a milestone's first feature produces nothing, the milestone is too abstract or has the wrong boundaries. Push back.

## Output format

```markdown
# Milestone: <Name>

Slice: <slice-slug>, or none

Status: planned

## Summary

One paragraph. What this milestone delivers and why this slice is the right next step.

## Acceptance criteria

Concrete, observable conditions that show the milestone is complete.

## Medium

Fields and flows the milestone's features introduce or use, and which feature is on each side.

## Dependencies

Sibling milestones, capabilities, or external conditions required.

## Core feature

The first feature to build. Two to three sentences of justification.

## Features

In order. Each entry gives a slug, a one-sentence description, and dependencies.

1. `<feature-slug>`: One sentence. Depends on: none.
2. `<feature-slug>`: One sentence. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

- <item>: One or two sentences. Gated on: <condition, when one exists>.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; the details go in RESEARCH.md.
```

## Next step

The user invokes `planning-features` against the core feature.
