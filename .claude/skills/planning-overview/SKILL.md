---
name: planning-overview
description: Read this first whenever the user asks to plan, design, scope, decompose, build, or implement anything. Names the planning hierarchy (capability, milestone, feature, plan, implementation) and the slices that span capabilities, the principles gate every level passes, where artifacts live, and which skill to invoke next. Consult before any of planning-slices, planning-capabilities, planning-milestones, planning-features, implementing-features, or maintaining-backlog.
---

# Planning Overview

This skill routes. It does not plan, decompose, or build.

## Hierarchy

Work decomposes through four levels.

1. Capability: a durable quality, discipline, or principle implementation the game deepens over many milestones; never completes.
2. Milestone: weeks of work; one coherent slice that deepens a capability; completes.
3. Feature: hours to days; one implementable unit; completes.
4. Implementation: code satisfying a feature's plan, with the module specs updated to match.

Each level produces an artifact the next level consumes. The principles sit above the whole tree: docs/principles.md outranks every artifact in it.

A slice sits beside the tree (decision 0023). It is a goal no single capability owns, met when member milestones in several capabilities land together. The slice fixes the end-to-end criteria and the fields and flows that pass between capabilities; each member milestone stays in its own capability and names its slice.

## File layout

```
plans/
  BACKLOG.md
  slices/
    <slice-slug>/
      SLICE.md
      RESEARCH.md
  <capability-slug>/
    CAPABILITY.md
    RESEARCH.md
    <milestone-slug>/
      MILESTONE.md
      <feature-slug>/
        FEATURE.md
        PLAN.md
```

Plans are the route to a change. The long-lived contract is each module's SPEC.md beside its code, which a feature updates as part of its work.

## Skill selection

| User intent | Skill |
| --- | --- |
| Plan, check, or close a goal that spans capabilities | `planning-slices` |
| Plan a capability | `planning-capabilities` |
| Plan a milestone inside an existing capability | `planning-milestones` |
| Plan a feature inside an existing milestone | `planning-features` |
| Build an existing feature plan | `implementing-features` |
| Add, select, or drop a speculative idea | `maintaining-backlog` |
| Review any artifact for real issues | `reviewing` |
| Research a question and retain the findings | `researching` |

If the level is ambiguous, ask. Do not guess.

## Hard gates

- `planning-milestones` requires an approved parent `CAPABILITY.md`, and for a slice member also the approved `SLICE.md`.
- `planning-features` requires an approved parent `MILESTONE.md`.
- `implementing-features` requires an approved target `PLAN.md`.

The user may start at any level. The user may not skip a level downward without producing its artifact.

## The principles gate

A tree gives each thing one owner, which is why cross-cutting mechanics felt wrong in the previous project. The fix is to check every plan against the principles, at every level.

- Every brief has a Medium section naming the fields it samples and emits and the flows it draws and supplies. Interactions between things go through these (principle 3, decision 0005).
- A plan in which one system depends on another's internals fails the gate (principle 6). Rework it so the interaction goes through the shared medium.
- Anything that genuinely cannot fit fields, flows, or encapsulation is raised with Evan before planning continues. If he accepts it, it goes in docs/exceptions.md with a reason.
- A plan that conflicts with any principle is surfaced to Evan, not planned around.
- Work that cuts across capabilities lives under the capability whose quality it primarily deepens. It names the other capabilities it touches, and reaches them only through the medium. A goal that needs several capabilities to move together is a slice, and its medium map is where the gate applies across them.

## Rules every planning skill enforces

- Read docs/principles.md, the SPEC.md of every module the work touches, and the decision records that bear on it. docs/design-notes.md is provisional input, never a spec.
- Do the research in-skill via the `researching` skill, which retains findings to a RESEARCH.md at the node. Read project files. Brainstorm with the user one question at a time.
- Build the smallest version that produces value first.
- Order work bottom-up. Foundations before dependents.
- Send speculation to `maintaining-backlog`. Commit only what current evidence supports.
- Plan one level down only. Deeper detail belongs to the next skill.
- Questions in docs/open-questions.md are Evan's to decide. When a plan depends on one, ask it; do not decide it in the plan.
