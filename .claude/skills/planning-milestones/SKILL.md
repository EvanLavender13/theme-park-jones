---
name: planning-milestones
description: Use whenever the user wants to plan a milestone inside an approved capability. Reads the parent CAPABILITY.md and the principles, explores related work, researches tactical patterns, brainstorms with the user one question at a time, names the core feature, maps feature dependencies and the medium each feature speaks, and writes MILESTONE.md with a feature decomposition. Stops at the brief; does not write feature plans or code. Use this even when the milestone seems straightforward.
---

# Planning Milestones

## Purpose

Take one milestone from a capability brief. Produce MILESTONE.md defining the milestone's scope, core feature, medium, dependencies, and feature decomposition.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved MILESTONE.md. Do not write a FEATURE.md or code. The parent CAPABILITY.md must exist at plans/<capability-slug>/CAPABILITY.md.

`researching`, `maintaining-backlog`, and `reviewing` are the only callable skills from here: `researching` to retain research at this node, `maintaining-backlog` to deposit speculative items and to drop items this milestone draws from or supersedes, and `reviewing` for the review before approval.

While closing a milestone, `commit-hygiene` is also callable and the `test-writer` agent may be dispatched. The acceptance criterion or SPEC.md statement a finding names may be rewritten. Test cases may be deleted, and test code edited only to fix a build, test, or tidy failure without changing what a case asserts.

## Checklist

Create a task for each item. Complete in order.

1. Read the parent CAPABILITY.md in full and treat it as ground. Confirm which milestone you are planning and its dependency position.
2. Read sibling milestones if present and note relationships. If the milestone is a slice member, read its SLICE.md in full: the milestone must deliver its share of the scenario and produce or consume exactly the cross-capability fields and flows the slice's medium map assigns to its capability. It may add fields and flows internal to its own capability. A needed change to cross-capability medium goes back to `planning-slices`, not into the milestone.
3. Explore related context: docs/principles.md, the SPEC.md of modules the milestone touches, relevant decision records, and the code. Identify patterns to follow.
4. Reconcile the backlog: drop the item this milestone draws from, and any item it supersedes, via `maintaining-backlog`.
5. Research tactical patterns via the `researching` skill. It writes findings to plans/<capability-slug>/<milestone-slug>/RESEARCH.md.
6. Ask clarifying questions, one at a time. Cover scope boundaries, acceptance criteria, and feature granularity.
7. Propose two or three approaches for decomposing into features, each with trade-offs.
8. Map the medium: for each feature, the fields and flows it samples, emits, draws, or supplies. Apply the principles gate from `planning-overview`; a feature that reaches into another's internals is reworked before it is listed.
9. Name the core feature: the first feature that makes the milestone partially usable.
10. Map feature dependencies, bottom-up.
11. Separate committed from speculative. Commit only the features current evidence supports; send the rest to `maintaining-backlog`.
12. Write MILESTONE.md at plans/<capability-slug>/<milestone-slug>/MILESTONE.md in the format below.
13. Record the milestone in the parent CAPABILITY.md's Milestones list when it is not already there.
14. Self-review for placeholders, contradictions, scope drift, vague acceptance criteria, and interactions that bypass the medium. Fix inline.
15. Review: stage MILESTONE.md and RESEARCH.md, then dispatch the reviewer via `reviewing`, with CAPABILITY.md and, for a slice member, SLICE.md as context. Handle the findings as `reviewing` describes for planning briefs: fix Minor ones and settled Major ones directly, bring the user only Major findings that need a design choice, and report the rest in one line.
16. Ask the user to approve.

## Closing a milestone

When every feature in the milestone's Features list has merged into main. A MILESTONE.md with no Status line predates milestone closing; decision 0032's audit read the tests those milestones added.

1. Find the milestone's merges: for each feature directory under the milestone, `git log --format='%h %s' --grep='^Merge: <feature-slug>$' main`.
2. Dispatch the test audit via the `reviewing` skill's Test audit dispatch template, and present the report to Evan verbatim.
3. Settle each finding with Evan. It is fixed, filed, or rejected.
    - A case that should not exist is deleted.
    - A case asserting a real property in the wrong shape is rewritten. When the finding names the criterion or spec statement behind the case, rewrite that first. Then dispatch the `test-writer` with the template below.
    - A fix that needs a design choice, or belongs to planned work, is filed in plans/BACKLOG.md via `maintaining-backlog`, naming the test, the rule, and the property. A finding plans/focused-tests/RESEARCH.md already lists counts as filed.
    - A rejected finding needs nothing more, unless Evan says the rule is wrong or incomplete. Then a decision record changes docs/testing.md.
4. Confirm that the windows-debug build has no warnings, `ctest.exe --preset windows-debug` passes, and `scripts/tidy.sh` is clean.
5. Set the MILESTONE.md's Status to complete, and commit via `commit-hygiene`.

Rewrite dispatch:

```
Rewrite the test cases these closing-audit findings name.

Features: <FEATURE.md path of each feature whose cases are named>
Specs: <SPEC.md paths those features change or depend on>
Findings:
<the findings, verbatim>

Follow your rewrite mode and return your standard report.
```

## Process notes

Stay inside the parent's scope. Work that belongs in another milestone or capability gets named and handed back, not absorbed.

Feature granularity: a feature must be implementable in hours to days. Split anything bigger; merge anything smaller.

Plan to the next loop only. Cascading uncertainty invalidates the rest.

Weak core features fail the brief. A milestone whose first feature produces nothing is too abstract or wrongly bounded. Push back.

## Output format

```markdown
# Milestone: <Name>

Slice: <slice-slug>, or none

Status: planned

## Summary

One paragraph. What this milestone delivers and why this slice is the right next step.

## Acceptance criteria

Concrete, observable conditions that indicate the milestone is complete.

## Medium

Fields and flows the milestone's features introduce or use, and which feature is on each side.

## Dependencies

Sibling milestones, capabilities, or external conditions required.

## Core feature

The first feature to build. Two to three sentences of justification.

## Features

Ordered. Each entry: slug, one-sentence description, dependencies.

1. `<feature-slug>`: One sentence. Depends on: none.
2. `<feature-slug>`: One sentence. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

- <item>: One or two sentences. Gated on: <condition, when one exists>.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; depth lives in RESEARCH.md.
```

## Next step

The user invokes `planning-features` against the core feature.
