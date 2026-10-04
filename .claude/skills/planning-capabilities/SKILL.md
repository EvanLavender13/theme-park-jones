---
name: planning-capabilities
description: Use whenever the user wants to plan a capability, a durable quality, discipline, or principle implementation the game deepens over many milestones. Reads project context and the principles, researches prior art, and brainstorms with the user one question at a time. Names the foundation, maps dependencies and the shared fields and flows the capability uses, and writes CAPABILITY.md covering the foundation and first milestones. Stops at the brief; does not write milestones, features, or code. Use this even when the capability seems straightforward.
---

# Planning Capabilities

## Purpose

Take a capability concept. Produce CAPABILITY.md. It defines the quality the capability deepens, its foundation, the fields and flows it uses, and the first milestones that establish it.

A capability is a lasting domain, not a finite deliverable. It starts with a foundation, which the first milestones make real. After that it deepens indefinitely, as later milestones are drawn from the backlog. This skill plans the foundation and the first milestones, not the capability's whole future.

## Naming

Name a capability after a quality it increases, a discipline it practices, or a principle it implements. Don't name it after a feature or an object it builds.

A quality deepens indefinitely and is never done: `legible-simulation`, `effortless-building`, `plausible-operations`. The same is true of a principle's implementation. For example, `shared-medium` implements principles 3 and 6, and it deepens with every new kind of field, flow, and network. A feature or object, such as `food-shops` or `coaster-editor`, reaches a done state and then collects unrelated milestones. The first kind is a capability. The second is a feature named like a capability. It strains the structure by the third milestone added after it feels finished.

If a capability is named for a principle, cite the principle in its Summary. Its foundation criteria must include the checks that back the principle. docs/principles.md says which principles code can violate.

An engineering capability deepens a property of the software that a player would not miss if the machine were infinitely fast and the code never changed again. Performance and code structure are examples, as in `sound-architecture` (decision 0028). Name it after that property. Its Medium section says it introduces no fields or flows. Its foundation criteria are checks it owns. Its work may change code in any module, but only if observable behavior does not change:

- Existing tests pass unchanged.
- Where the simulation is touched, tpj_scenarios's output is identical before and after (decision 0030).
- A public contract changes only together with its module's SPEC.md.

Test the name before you commit to it. Ask whether a tenth milestone could land in it two years from now without the name feeling wrong. If yes, the name will last. If the name implies a finish line, rework it.

Treat content-heavy domains the same way. A catalog of shops or rides grows by adding instances, which is not the same as deepening a quality. Name the capability after the quality the content serves.

## Hard gate

Do not invoke a planning or implementing skill until the user has approved CAPABILITY.md. Do not write a MILESTONE.md, code, or scaffolding. Even a small capability gets a brief.

From here you may call only three skills:

- `researching`, to keep research at this node.
- `maintaining-backlog`, only to deposit speculative items.
- `reviewing`, for the review before approval.

## Checklist

Create a task for each item. Complete them in order.

1. Explore the project context. Read docs/principles.md and the related files in plans/, including any slice that names this capability as a member. Also read the SPEC.md of the modules the capability touches, the relevant decision records, and recent commits. Check whether the concept deepens an existing capability instead of starting a new one.
2. Ask clarifying questions, one at a time, preferring multiple choice. Cover purpose, constraints, foundation criteria, and the boundary of the quality this capability owns.
3. Name the capability using the Naming rules above. The slug sets the node's path, so name it before researching.
4. Research prior art with the `researching` skill. It writes findings to plans/<capability-slug>/RESEARCH.md. If the findings raise new questions, return to step 2.
5. Propose two or three approaches, each with trade-offs and a recommendation.
6. Map the medium. List the fields and flows the capability introduces, the ones it samples or draws from other capabilities, and the ones it emits for others. Apply the principles gate from `planning-overview`.
7. Name the foundation: the smallest version that establishes the domain and produces value. It becomes the first milestone or milestones. If a slice names this capability as a member, include the member milestone among them, delivering the share the slice assigns.
8. Map dependencies. Order the foundation milestones so the ones others build on come first.
9. Separate the foundation from the backlog. Commit the milestones that establish the foundation, and send everything beyond them to `maintaining-backlog`.
10. Write CAPABILITY.md at plans/<capability-slug>/CAPABILITY.md in the format below.
11. Self-review, and fix problems inline. Look for:
    - Placeholders.
    - Contradictions.
    - Foundation criteria that imply a finish line.
    - Scope that reaches into another quality.
    - Interactions that bypass the medium.
12. Review. Stage CAPABILITY.md and RESEARCH.md, then dispatch the reviewer with `reviewing`. If a slice names this capability, give it SLICE.md as context. Handle the findings as `reviewing` describes for planning briefs. Fix Minor findings and settled Major findings yourself. Bring the user only the Major findings that need a design choice, and report the rest in one line.
13. Ask the user to approve the brief.

## Process notes

Ask one question per message. Each answer changes which question comes next.

Search by problem, not by skill name: "guest pathfinding in park simulations", "desire paths from foot traffic", "procedural building massing from context".

Plan only to the next loop. Uncertainty compounds, and it invalidates anything committed beyond the first one or two milestones.

Don't define a completed state. Write foundation criteria that show the domain is established and ready to deepen. Don't write success criteria that imply completion.

A weak foundation fails the brief. If the foundation produces nothing on its own, it is too abstract; suggest splitting it.

Stay inside the quality. Work that deepens a different quality belongs to a different capability. Name that capability and hand the work back instead of absorbing it.

## Output format

```markdown
# Capability: <Name>

## Summary

One paragraph. The quality, discipline, or principle this capability deepens, and the value it produces.

## Foundation criteria

Concrete, observable conditions that show the foundation is established. They mark readiness to deepen, not a finish line.

## Medium

Fields and flows this capability introduces, samples, emits, draws, or supplies, and which other capabilities are on the other side of each.

## Principles

Which principles this capability's work is most at risk of violating, and how that will be checked.

## Dependencies

Capabilities, systems, or external conditions required. Mark each met or unmet.

## Foundation

The smallest version of this capability that establishes the domain and produces value. Two to three sentences of justification.

## Milestones

The foundation milestones plus the first deepening milestones, in order. Each entry gives a slug, a one-sentence description, and dependencies.

1. `<milestone-slug>`: One sentence. Depends on: none.
2. `<milestone-slug>`: One sentence. Depends on: milestone 1.

## Deepening candidates

Unordered pool this capability draws later milestones from.

- <item>: One or two sentences. Gated on: <condition, when one exists>.

## Open questions

Each with what could resolve it.

## Research notes

Brief. Cite by URL or filename; the details go in RESEARCH.md.
```

## Next step

The user invokes `planning-milestones` against the first milestone.
