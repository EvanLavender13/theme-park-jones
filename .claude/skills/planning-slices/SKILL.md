---
name: planning-slices
description: Use whenever the user wants to plan, check, or close a slice, a goal that spans capabilities and is met when member milestones in several capabilities land together, such as the boxes-and-tubes slice in docs/vision.md. Brainstorms the end-to-end scenario with the user one question at a time, maps every field and flow that crosses capabilities, names each member capability's share, orders the members, and writes SLICE.md. Closing a slice verifies its end-to-end criteria through a separate test pass. Does not write capability briefs, milestones, or code. Use this even when a slice seems obvious.
---

# Planning Slices

## Purpose

Take a goal that no single capability owns. Produce plans/slices/<slug>/SLICE.md (decision 0023). It states:

- What the player can do end to end when the slice lands.
- How the capabilities involved exchange data through the shared medium.
- Which milestone each capability contributes, in build order.

A slice completes. Its member milestones belong to their capabilities and stay there after the slice closes.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved SLICE.md. Do not write CAPABILITY.md, MILESTONE.md, or code.

While planning, you may call only three skills:

- `researching`, to keep research at this node.
- `maintaining-backlog`, to deposit speculative items.
- `reviewing`, for the review before approval.

While closing a slice, you may also call `reviewing` and `commit-hygiene`, as the closing steps say.

## Checklist

Create a task for each item. Complete them in order.

1. Explore the context. Read docs/vision.md, docs/principles.md, docs/design-notes.md (provisional), the decision records, the existing slices and capabilities under plans/, and the module specs.
2. Ask clarifying questions, one at a time, preferring multiple choice. Cover the end-to-end scenario, what is deliberately left out, and what the player must be able to observe.
3. Give the slice a short slug.
4. Where the scenario raises open questions of design or technique, research them with the `researching` skill. It writes findings to plans/slices/<slug>/RESEARCH.md.
5. Write the end-to-end scenario in prose: what a player does and sees, from an empty park to the slice's payoff.
6. Map the medium. List every field and flow the scenario needs, each with the capability that produces it and the capabilities that consume it. Apply the principles gate from `planning-overview` across capabilities. If the scenario needs an interaction that is not a field or flow, rework it, or raise it with the user as a possible exception.
7. Name the members. For each capability involved, existing or still to be planned, name the one milestone it contributes. Give that milestone's share of the scenario in one sentence. If a capability contributes nothing observable, leave it out.
8. Order the members bottom-up, so the producers of the medium land before its consumers.
9. Write acceptance criteria. Include observable end-to-end conditions in the running app, and consequences that an integration test over the simulation can check, as docs/testing.md defines them. Each criterion names how it is checked:
    - An integration test.
    - A capture of a scripted scenario. Name the member milestone that delivers the script in Members.
    - A manual check by the user.
10. Write SLICE.md at plans/slices/<slug>/SLICE.md in the format below.
11. Self-review, and fix problems inline. Look for:
    - Placeholders.
    - Contradictions.
    - Cross-capability interactions missing from the medium map.
    - Members out of order.
    - Criteria that can't be observed or tested.
    - Claims about existing code. Read the code behind every criterion and every Medium rule before you dispatch the review, and fix the draft yourself. Read the park world checks (tests/sim/park_world_checks_test.cpp) for what checked parks must hold, the runtime report's stages (src/bench/SPEC.md) for any runtime criterion, each park file a criterion names, and the medium's rules (src/sim/medium/SPEC.md) for every field and flow the Medium uses.
    - A member whose new state no checked park holds when it lands, or whose behavior can't be played because its producer lands later.
12. Review. Stage SLICE.md and RESEARCH.md, then dispatch the reviewer with `reviewing`. Handle the findings as `reviewing` describes for planning briefs. Fix Minor findings and settled Major findings yourself. Bring the user only the Major findings that need a design choice, and report the rest in one line.
13. Ask the user to approve.

## Closing a slice

Close a slice when every member milestone has landed.

1. Dispatch the `test-writer` agent in slice mode with SLICE.md, the member MILESTONE.md files, and the module specs. It writes integration tests for the criteria checked by integration test.
2. Run the tests and checks:
    - Run the integration tests with `ctest.exe --preset windows-debug`.
    - For each cross-module bug the test-writer names, introduce that bug by hand once and confirm a test fails, then revert. For example, skip one consumption, or leave one component out of the save.
    - Run each scripted scenario on the Windows build with `--capture`, and inspect the captures against their criteria.
    - Ask the user to check the manual criteria, and record the answer.
3. Dispatch the `reviewer` with the `reviewing` skill on SLICE.md and the integration tests.
4. If everything passes, set the slice's Status to complete and commit with `commit-hygiene`. If not, report the failing criteria to the user. The fix belongs to the next milestone or feature of a member capability.

## Process notes

Ask one question per message. Each answer changes which question comes next.

A slice is the smallest end-to-end experience worth having. Send anything that doesn't serve the scenario to the backlog.

The medium map is the main thing a slice contributes. The capabilities plan their own internals; the slice sets what passes between them.

## Output format

```markdown
# Slice: <Name>

Status: planned

## Summary

One paragraph. What the player can do when this slice lands, and why it is the right next goal.

## End-to-end scenario

Prose, from an empty park to the payoff.

## Acceptance criteria

Observable conditions in the running app, and properties an integration test over the simulation can check. Each ends with how it is checked: (integration test), (scripted capture: <member>), or (manual).

## Medium

Each field and flow that crosses capabilities: name, kind (field or flow), producer, consumers, and what it carries.

## Members

In build order. Each entry gives the capability, the member milestone slug (or "to be planned"), its share of the scenario, and dependencies.

1. `<capability>/<milestone-slug>`: One sentence. Depends on: none.
2. `<capability>/<milestone-slug>`: One sentence. Depends on: member 1.

## Out of scope

What the slice deliberately leaves out.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; the details go in RESEARCH.md.
```

## Next step

For each member capability that doesn't exist yet, in member order, the user invokes `planning-capabilities`. Then the user invokes `planning-milestones` for each member milestone.
