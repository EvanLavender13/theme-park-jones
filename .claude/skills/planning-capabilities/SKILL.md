---
name: planning-capabilities
description: Use whenever the user wants to plan a capability, a durable quality, discipline, or principle implementation the game deepens over many milestones. Reads project context and the principles, researches prior art, brainstorms with the user one question at a time, names the foundation, maps dependencies and the shared medium the capability speaks, and writes CAPABILITY.md covering the foundation and first milestones. Stops at the brief; does not write milestones, features, or code. Use this even when the capability seems straightforward.
---

# Planning Capabilities

## Purpose

Take a capability concept. Produce CAPABILITY.md defining the quality the capability deepens, its foundation, the fields and flows it speaks, and the first milestones that establish it.

A capability is a durable domain, not a finite deliverable. It has a foundation, the first milestones that make it real, and then it deepens indefinitely as later milestones are drawn from the backlog. This skill plans the foundation and the first milestones, not the capability's whole future.

## Naming

Name a capability after a quality it increases, a discipline it practices, or a principle it implements, never after a feature or an object it builds.

A quality deepens indefinitely and never reaches done: `legible-simulation`, `effortless-building`, `plausible-operations`. So does the implementation of a principle: `shared-medium` implements principles 3 and 6 and deepens with every new kind of field, flow, and network. A feature or object reaches a done state and then accumulates unrelated milestones: `food-shops`, `coaster-editor`. The first kind is a capability. The second is a feature wearing a capability's name, and it strains the structure by the third milestone added after it feels finished.

A capability named for a principle cites the principle in its Summary, and its foundation criteria include the checks that back the principle (docs/principles.md says which principles can be violated by code).

An engineering capability deepens a property of the software that a player would not notice missing if the machine were infinitely fast and the code never changed again, such as performance or code structure: `sound-architecture` (decision 0028). It is named after that property. Its Medium section says it introduces no fields or flows. Its foundation criteria are checks it owns, and its work may change code in any module only if observable behavior does not change: existing tests pass unchanged, tpj_scenarios's output is identical before and after where the simulation is touched (decision 0030), and a public contract changes only with its module's SPEC.md.

Test the name before committing it: can a tenth milestone land inside it two years from now without the name feeling wrong? If yes, the name is durable. If the name implies a finish line, rework it.

Content-heavy domains need the same treatment. A catalog of shops or rides grows by accreting instances, which is not the same as deepening a quality. Name the capability after the quality the content serves.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved CAPABILITY.md. Do not write a MILESTONE.md, code, or scaffolding. Even a small capability gets a brief.

`researching`, `maintaining-backlog`, and `reviewing` are the only callable skills from here: `researching` to retain research at this node, `maintaining-backlog` only to deposit speculative items, and `reviewing` for the review before approval.

## Checklist

Create a task for each item. Complete in order.

1. Explore project context: read docs/principles.md, related files in plans/ including any slice that names this capability as a member, the SPEC.md of modules the capability touches, relevant decision records, and recent commits. Check whether the concept deepens an existing capability rather than starting a new one.
2. Ask clarifying questions, one at a time, preferring multiple choice. Cover purpose, constraints, foundation criteria, and the boundary of the quality this capability owns.
3. Name the capability by the Naming rule above. The slug fixes the node's path, so name before researching.
4. Research prior art via the `researching` skill. It writes findings to plans/<capability-slug>/RESEARCH.md. Return to step 2 if the findings raise new questions.
5. Propose two or three approaches, each with trade-offs and a recommendation.
6. Map the medium: which fields and flows the capability introduces, which it samples or draws from other capabilities, and which it emits for others. Apply the principles gate from `planning-overview`.
7. Name the foundation: the smallest version that establishes the domain and produces value. It becomes the first milestone or milestones. When a slice names this capability as a member, the member milestone is among them and delivers the share the slice assigns.
8. Map dependencies. Order foundation milestones so the ones others build on come first.
9. Separate foundation from backlog. Commit the milestones that establish the foundation; everything beyond goes to `maintaining-backlog`.
10. Write CAPABILITY.md at plans/<capability-slug>/CAPABILITY.md in the format below.
11. Self-review for placeholders, contradictions, foundation criteria that imply a finish line, scope that reaches into another quality, and interactions that bypass the medium. Fix inline.
12. Review: stage CAPABILITY.md and RESEARCH.md, then dispatch the reviewer via `reviewing`, with SLICE.md as context when a slice names this capability. Handle the findings as `reviewing` describes for planning briefs: fix Minor ones and settled Major ones directly, bring the user only Major findings that need a design choice, and report the rest in one line.
13. Ask the user to approve the brief.

## Process notes

One question per message. Each answer changes which question comes next.

Search by problem, not by skill name: "guest pathfinding in park simulations", "desire paths from foot traffic", "procedural building massing from context".

Plan to the next loop only. Cascading uncertainty invalidates anything committed beyond the first one or two milestones.

No completed state. Write foundation criteria that indicate the domain is established and ready to deepen, not success criteria that imply completion.

Weak foundations fail the brief. A foundation that produces nothing on its own is too abstract; suggest splitting.

Stay inside the quality. Work that deepens a different quality belongs to a different capability; name it and hand it back rather than absorbing it.

## Output format

```markdown
# Capability: <Name>

## Summary

One paragraph. The quality, discipline, or principle this capability deepens, and the value it produces.

## Foundation criteria

Concrete, observable conditions that indicate the foundation is established. They mark readiness to deepen, not a finish line.

## Medium

Fields and flows this capability introduces, samples, emits, draws, or supplies, and which other capabilities are on the other side of each.

## Principles

Which principles this capability's work is most at risk of violating, and how that will be checked.

## Dependencies

Capabilities, systems, or external conditions required. Mark each met or unmet.

## Foundation

The smallest version of this capability that establishes the domain and produces value. Two to three sentences of justification.

## Milestones

The foundation milestones plus the first deepening milestones, ordered. Each entry: slug, one-sentence description, dependencies.

1. `<milestone-slug>`: One sentence. Depends on: none.
2. `<milestone-slug>`: One sentence. Depends on: milestone 1.

## Deepening candidates

Unordered pool this capability draws later milestones from.

- <item>: One or two sentences. Gated on: <condition, when one exists>.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; depth lives in RESEARCH.md.
```

## Next step

The user invokes `planning-milestones` against the first milestone.
