# 0014. Port and adapt the SteelJones workflow

Status: Accepted, 2026-09-26

## Context

SteelJones used a capability, milestone, feature, plan tree with skills for each level, a fresh-context reviewer, research notes, and commit hooks. The tree worked, but cross-cutting mechanics felt wrong because a tree gives each thing one owner.

## Decision

Port the review, git hygiene, and research tooling as-is, retargeted at this repository. Port the planning tree with a principles gate at every level: a plan names the fields and flows its work samples and emits, and a plan that makes one system depend on another's internals fails review and is reworked to go through the shared medium. Plans live in plans/. Module SPEC.md files remain the long-lived contract; plans are the route to a spec change, not a replacement for it.

## Consequences

The domain skills (animation, strategy) and the release skill are not ported. The reviewer checks plans and diffs against docs/principles.md as well as the spec.
