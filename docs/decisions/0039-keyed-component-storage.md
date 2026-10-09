# 0039. The world stores its components itself, keyed by EntityKey

Status: Accepted, 2026-10-09

Supersedes 0016.

## Context

Decision 0016 chose EnTT as the ECS on 2026-09-26. The next day, world-as-value made the world a value with stable keys and a registered walk. That design routes around most of EnTT:

- EnTT's storage order depends on the history of inserts and removals. A loaded world would iterate differently from the live one, so systems walk entities in key order instead of through EnTT's views. The sim has one view, and it sorts what it collects.
- EnTT can't clone a registry, and its snapshots follow storage order. Copy, equality, hash, save, and load all go through the schema's registered walk.
- World keeps its own maps between EntityKey and entt::entity. Every access looks up the key, then looks up the component in EnTT.

What EnTT still does is hold each type's components, hand out entity handles, list its storages for the walk's checks, and give each type an id. The project carries a library whose main features it can't use, and every call site names it.

## Decision

Drop EnTT. The world stores each registered component type itself, keyed by EntityKey, and iterates each type in key order. Code reaches components only through World, by key. Entity handles other than EntityKey leave the sim's interface.

The first store is the simplest one that keeps key order and keeps references valid while other entities gain or lose the same type. Faster stores may replace it later behind the same interface, when the runtime report shows a need. Any replacement keeps key order, so iteration never depends on history.

Principle 6 stays enforced as 0016 enforced it. A module declares its private components in headers under an internal directory. Other modules can't include those headers, so they can't name the types. cmake/check_private_headers.cmake checks this.

Decision 0002's stack keeps entities and components, without an ECS library.

## Consequences

The sound-architecture milestone keyed-storage does the change. It must leave the simulation unchanged, as decision 0028 requires of engineering work: tpj_scenarios --compare and the cross-build check show the same output before and after.

It can't meet 0028's other condition, that the existing tests pass unchanged, because it changes World's public contract. src/sim/SPEC.md changes with it. The tests change in three ways:

- Tests that reach components through World::Registry and findEntity are rewritten to reach them by key. What they assert doesn't change.
- Cases that check EnTT handles are deleted, because the handles no longer exist. These are the entity key tests that findEntity and keyOf are inverses, and that lookups of keys that aren't live give entt::null.
- The world checks case for an unregistered component asserts a throw when the component is added, instead of when the world is walked.

That last change is a change in behavior. Today, debug builds refuse an unregistered component when the walk reaches it. Afterwards, every build refuses it when it is added, because the world has no store to put it in. The walk no longer needs to look for an entity created outside createEntity either, because nothing else can create one.

Specs, conventions, and the private header check cite this record for the private-header rule instead of 0016.
