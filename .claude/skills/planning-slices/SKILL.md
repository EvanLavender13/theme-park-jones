---
name: planning-slices
description: Use whenever the user wants to plan, check, or close a slice, a goal that spans capabilities and is met when member milestones in several capabilities land together, such as the boxes-and-tubes slice in docs/vision.md. Brainstorms the end-to-end scenario with the user one question at a time, maps every field and flow that crosses capabilities, names each member capability's share, orders the members, and writes SLICE.md. Closing a slice verifies its end-to-end criteria through a separate test pass. Does not write capability briefs, milestones, or code. Use this even when a slice seems obvious.
---

# Planning Slices

## Purpose

Take a goal that no single capability owns. Produce plans/slices/<slug>/SLICE.md stating what the player can do end to end when the slice lands, how the capabilities involved talk to each other through the shared medium, and which milestone each capability contributes, in build order (decision 0023).

A slice completes. Its member milestones live in their capabilities and keep their place there after the slice closes.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved SLICE.md. Do not write CAPABILITY.md, MILESTONE.md, or code.

While planning, `researching`, `maintaining-backlog`, and `reviewing` are the only callable skills: `researching` to retain research at this node, `maintaining-backlog` to deposit speculative items, and `reviewing` for the review before approval. While closing a slice, `reviewing` and `commit-hygiene` are also callable, as the closing steps say.

## Checklist

Create a task for each item. Complete in order.

1. Explore context: read docs/vision.md, docs/principles.md, docs/design-notes.md (provisional), the decision records, existing slices and capabilities under plans/, and the module specs.
2. Ask clarifying questions, one at a time, preferring multiple choice. Cover the end-to-end scenario, what is deliberately left out, and what the player must be able to observe.
3. Name the slice with a short slug.
4. Research via the `researching` skill where the scenario raises open design or technique questions. It writes findings to plans/slices/<slug>/RESEARCH.md.
5. Write the end-to-end scenario: what a player does and sees, in prose, from an empty park to the slice's payoff.
6. Map the medium: every field and flow the scenario needs, each with its producing capability and its consuming capabilities. Apply the principles gate from `planning-overview` across capabilities: any interaction the scenario needs that is not a field or flow is reworked, or raised with Evan as a possible exception.
7. Name the members: for each capability involved, existing or to be planned, the one milestone it contributes and that milestone's share of the scenario in one sentence. A capability that contributes nothing observable does not belong.
8. Order the members bottom-up, so producers of the medium land before consumers.
9. Write acceptance criteria: observable end-to-end conditions in the running app, and properties that can be checked by an integration test over the simulation. Each criterion names how it is checked: an integration test, a capture of a scripted scenario (the member milestone that delivers the script is named in Members), or a manual check by Evan.
10. Write SLICE.md at plans/slices/<slug>/SLICE.md in the format below.
11. Self-review for placeholders, contradictions, cross-capability interactions missing from the medium map, members out of order, and criteria that cannot be observed or tested. Fix inline.
12. Review: stage SLICE.md and RESEARCH.md, then dispatch the reviewer via `reviewing`. Handle the findings as `reviewing` describes for planning briefs: fix Minor ones and settled Major ones directly, bring the user only Major findings that need a design choice, and report the rest in one line.
13. Ask the user to approve.

## Closing a slice

When every member milestone has landed:

1. Dispatch the `test-writer` agent in slice mode with SLICE.md, the member MILESTONE.md files, and the module specs, to write integration tests for the criteria checked by integration test.
2. Run them with `ctest --preset linux-debug`. Run each scripted scenario on the Windows build with `--capture` and inspect the captures against their criteria. Ask Evan to check the manual criteria, and record his answer.
3. Dispatch the `reviewer` via the `reviewing` skill on SLICE.md and the integration tests.
4. If everything passes, set the slice's Status to complete and commit via `commit-hygiene`. If not, report the failing criteria to the user; the fix belongs to a member capability's next milestone or feature.

## Process notes

One question per message. Each answer changes which question comes next.

A slice is the smallest end-to-end experience worth having. Anything that does not serve the scenario goes to the backlog.

The medium map is the slice's main contribution. The capabilities plan their own internals; the slice fixes what passes between them.

## Output format

```markdown
# Slice: <Name>

Status: planned

## Summary

One paragraph. What the player can do when this slice lands and why it is the right next goal.

## End-to-end scenario

Prose, from an empty park to the payoff.

## Acceptance criteria

Observable conditions in the running app, and properties an integration test over the simulation can check. Each ends with how it is checked: (integration test), (scripted capture: <member>), or (manual).

## Medium

Each field and flow that crosses capabilities: name, kind (field or flow), producer, consumers, and what it carries.

## Members

Ordered for building. Each entry: capability, member milestone slug (or "to be planned"), its share of the scenario, dependencies.

1. `<capability>/<milestone-slug>`: One sentence. Depends on: none.
2. `<capability>/<milestone-slug>`: One sentence. Depends on: member 1.

## Out of scope

What the slice deliberately leaves out.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; depth lives in RESEARCH.md.
```

## Next step

For each member capability that does not exist yet, in member order, the user invokes `planning-capabilities`. Then `planning-milestones` for each member milestone.
