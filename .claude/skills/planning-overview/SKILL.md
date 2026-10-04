---
name: planning-overview
description: Read this first whenever the user asks to plan, design, scope, decompose, build, or implement anything. Describes the planning hierarchy (capability, milestone, feature, plan, implementation), the slices that span capabilities, the principles gate that every level must pass, where artifacts live, and which skill to invoke next. Consult before any of planning-slices, planning-capabilities, planning-milestones, planning-features, implementing-features, or maintaining-backlog.
---

# Planning Overview

This skill only points to the right skill. It doesn't plan, decompose, or build.

## Hierarchy

Work breaks down through four levels.

1. Capability: a durable quality, discipline, or principle implementation that the game deepens over many milestones. It never completes. An engineering capability deepens a property of the software that the player can't perceive, such as performance or code structure (decision 0028).
2. Milestone: weeks of work on one coherent piece that deepens a capability. It completes.
3. Feature: hours to days of work on one implementable unit. It completes.
4. Implementation: code that satisfies a feature's plan, with the module specs updated to match.

Each level produces an artifact that the next level uses. The principles sit above the whole tree: docs/principles.md outranks every artifact in it.

A slice sits beside the tree (decision 0023). It is a goal that no single capability owns. It is met when member milestones in several capabilities land together. The slice sets the end-to-end criteria and the fields and flows that pass between capabilities. Each member milestone stays in its own capability and names its slice.

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

Plans describe how to make a change. The long-lived contract is each module's SPEC.md, next to its code. A feature updates it as part of its work.

## Skill selection

| User intent | Skill |
| --- | --- |
| Plan, check, or close a goal that spans capabilities | `planning-slices` |
| Plan a capability | `planning-capabilities` |
| Plan or close a milestone inside an existing capability | `planning-milestones` |
| Plan a feature inside an existing milestone | `planning-features` |
| Build an existing feature plan | `implementing-features` |
| Add, select, or drop a speculative idea | `maintaining-backlog` |
| Review any artifact for real issues | `reviewing` |
| Research a question and retain the findings | `researching` |

If the level is unclear, ask. Don't guess.

## Hard gates

- `planning-milestones` requires an approved parent `CAPABILITY.md`. For a slice member, it also requires the approved `SLICE.md`.
- `planning-features` requires an approved parent `MILESTONE.md`.
- `implementing-features` requires an approved target `PLAN.md`.

The user may start at any level. The user may not skip a level on the way down without producing its artifact.

## The principles gate

A tree gives each thing one owner. That is why cross-cutting mechanics felt wrong in the previous project. To fix this, check every plan against the principles at every level.

- Give every brief a Medium section. It names the fields the work samples and emits and the flows it draws and supplies. Interactions between things go through these (principle 3, decision 0005).
- If a plan has one system depending on another's internals, it fails the gate (principle 6). Rework it so the interaction goes through the shared medium.
- If something genuinely can't fit fields, flows, or encapsulation, raise it with Evan before planning continues. If he accepts it, add it to docs/exceptions.md with a reason.
- If a plan conflicts with any principle, tell Evan instead of planning around it.
- Put work that cuts across capabilities under the capability whose quality it mainly deepens. Name the other capabilities it touches, and reach them only through the medium.
- Work is engineering if a player wouldn't notice it missing, assuming the machine were infinitely fast and the code never changed again. It goes in an engineering capability, whichever capability surfaced it, and it changes no observable behavior (decision 0028).
- Work that is motivated by speed but changes what the player sees is gameplay.
- A goal that needs several capabilities to move together is a slice. The gate applies across those capabilities through the slice's medium map.

## Rules every planning skill enforces

- Read docs/principles.md, the SPEC.md of every module the work touches, and the decision records that bear on it. docs/design-notes.md is provisional input, not a spec.
- Do the research inside the skill with the `researching` skill. It saves findings to a RESEARCH.md at the node. Read project files. Brainstorm with the user one question at a time.
- Build the smallest version that produces value first.
- Order work bottom-up, foundations before the work that depends on them.
- Send speculation to `maintaining-backlog`. Commit only to what current evidence supports.
- Plan one level down only. Deeper detail belongs to the next skill.
- Questions in docs/open-questions.md are Evan's to decide. When a plan depends on one, ask him. Don't decide it in the plan.
