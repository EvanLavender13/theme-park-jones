---
name: maintaining-backlog
description: Use whenever the user wants to add, select, drop, or reconcile a speculative idea, or whenever a planning or implementing skill needs to deposit one or prune items that shipped work has implemented or made moot. Ideas live in three pools, chosen by the narrowest scope that fits. An idea that fits one existing milestone goes to that MILESTONE.md's "Deepening candidates" section. A capability-scoped idea with no fitting milestone goes to that CAPABILITY.md's section. A cross-capability idea, or one with no home, goes to plans/BACKLOG.md. Items are added when ideas fall out of current scope, selected when a new piece of work needs a starting point, and dropped when shipped work retires them. Use this even for a single idea.
---

# Maintaining the Backlog

## Purpose

Keep the pools of speculative ideas. Add items when they fall out of current scope. Select items when a new piece of work needs a starting point. Drop items the user no longer wants. Reconcile items against shipped work so that nothing already built stays in a pool.

The pools hold ideas without committing to them. Most items will never be implemented, and that is intended.

## Routing

Ideas live in one of three pools. Choose the pool with the narrowest scope that fits.

First apply decision 0028's test. Ask whether a player would notice the idea missing if the machine were infinitely fast and the code never changed again. If not, as with a speedup or a restructure, it belongs in an engineering capability's pools, whichever capability's work surfaced it. If no engineering capability fits, put the idea in `plans/BACKLOG.md`.

- If an idea fits the theme of one existing milestone, put it in the `## Deepening candidates` section of that milestone's `MILESTONE.md`. Milestones are durable buckets, so a shipped milestone still collects candidates.
- If an idea is scoped to one existing capability but fits no milestone, put it in the `## Deepening candidates` section of that capability's `CAPABILITY.md`.
- If an idea spans capabilities, or has no capability yet, put it in `plans/BACKLOG.md`.

If the `## Deepening candidates` section doesn't exist, create it. For a milestone, open it with the line "Unordered pool this milestone draws later features from." For a capability, open it with "Unordered pool this capability draws later milestones from."

Every operation below applies to all pools. When an item's home is created or its scope narrows so that a capability or milestone now fits, move it to the narrower pool.

## File format

`plans/BACKLOG.md` is a single file with a short header and a flat bullet list under `## Items`. Match the existing file's shape exactly. Don't add dates, tags, IDs, status markers, sections, or rankings.

If `plans/BACKLOG.md` doesn't exist, create it with this header and an empty `## Items` section:

```markdown
# Backlog

Unordered pool of ideas held for future work. Items get added when they drift out of current scope and selected when a new piece of work needs a starting point. Not a wishlist or a roadmap.

## Items

- <first item>
```

## Operations

### Add

Append a bullet to `## Items`. Write one or two sentences. State the idea, not the rationale.

When a planning skill deposits several items, add them all in a single edit.

### Select

The user has chosen an item to start work on.

1. Identify which planning skill applies (`planning-capabilities`, `planning-milestones`, or `planning-features`).
2. Delete the item from `## Items`.
3. Tell the user which skill to invoke and what the starting input is.

Don't invoke the planning skill. The user invokes it.

### Drop

The user has decided an item is no longer worth keeping. Delete it.

### Reconcile

A feature or milestone has shipped. Check the backlog against it.

1. Find items that the shipped work implements, supersedes, or makes moot.
2. Show each candidate with the work that retired it.
3. Delete the ones the user confirms.

Many items name what they wait on ("deferred from X", "gated on Y", "ships with Z"). When that work lands, the item is a candidate for reconciling.

### Review

Show the user the full list. Don't filter, sort, or group it. Let the user decide what to select or drop.

## Process notes

Don't impose structure. Use no dates, tags, sections, or rankings inside a pool. Keep each pool a flat list.

Put each item in one pool only. An item lives in exactly one `## Deepening candidates` section or in `plans/BACKLOG.md`, not in two places.

Items never start work on their own. The user chooses when to draw from a pool. Pruning is the exception: planning and implementing skills reconcile against the backlog when shipped work retires an item.
