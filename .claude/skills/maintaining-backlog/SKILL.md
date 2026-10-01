---
name: maintaining-backlog
description: Use whenever the user wants to add, select, drop, or reconcile a speculative idea, or whenever a planning or implementing skill needs to deposit one or prune items that shipped work has implemented or made moot. Ideas live in three pools, routed by the narrowest scope that fits - an idea fitting one existing milestone goes to that MILESTONE.md's "Deepening candidates" section, a capability-scoped idea with no fitting milestone goes to that CAPABILITY.md's section, and a cross-capability or unhomed idea goes to plans/BACKLOG.md. Items get added when ideas drift out of current scope, selected when a new piece of work needs a starting point, and dropped when shipped work retires them. Use this even for a single idea.
---

# Maintaining the Backlog

## Purpose

Keep the pools of speculative ideas. Add items when they drift out of current scope. Select items when a new piece of work needs a starting point. Drop items the user no longer wants, and reconcile items against shipped work so nothing already built lingers.

The pools catch inspiration without forcing commitment. Most items will never be implemented. That is the point.

## Routing

Ideas live in one of three pools. Route by the narrowest scope that fits.

First apply decision 0028's test: an idea a player would not notice missing, if the machine were infinitely fast and the code never changed again, such as a speedup or a restructure, belongs to an engineering capability's pools, whichever capability's work surfaced it. With no engineering capability that fits, it goes to `plans/BACKLOG.md`.

- An idea that fits one existing milestone thematically goes to the `## Deepening candidates` section of that milestone's `MILESTONE.md`. Milestones are durable buckets; a shipped milestone still collects candidates.
- An idea scoped to one existing capability but no milestone goes to the `## Deepening candidates` section of that capability's `CAPABILITY.md`.
- An idea spanning capabilities, or with no capability yet, goes to `plans/BACKLOG.md`.

Create a `## Deepening candidates` section when absent, opening with the line "Unordered pool this milestone draws later features from." (milestone) or "Unordered pool this capability draws later milestones from." (capability).

Every operation below applies to all pools. When an item's home is created or its scope narrows (a capability or milestone now fits), move it to the narrower pool.

## File format

`plans/BACKLOG.md` is a single file with a short header and a flat bullet list under `## Items`. Match the existing file's shape exactly. Do not add dates, tags, IDs, status markers, sections, or rankings.

If `plans/BACKLOG.md` does not exist, create it with this header and an empty `## Items` section:

```markdown
# Backlog

Unordered pool of ideas held for future work. Items get added when they drift out of current scope and selected when a new piece of work needs a starting point. Not a wishlist or a roadmap.

## Items

- <first item>
```

## Operations

### Add

Append a bullet to `## Items`. One or two sentences. State the idea, not the rationale.

When a planning skill deposits multiple items, batch them into a single edit.

### Select

The user has chosen an item to start work on.

1. Identify which planning skill applies (`planning-capabilities`, `planning-milestones`, or `planning-features`).
2. Delete the item from `## Items`.
3. Tell the user which skill to invoke and what the seed input is.

Do not invoke the planning skill. The user invokes it.

### Drop

The user has decided an item is no longer worth keeping. Delete it.

### Reconcile

A feature or milestone has shipped. Cross-check the backlog against it.

1. Find items the shipped work implements, supersedes, or makes moot.
2. Show each candidate with the work that retired it.
3. Delete the ones the user confirms.

Many items name their gate ("deferred from X", "gated on Y", "ships with Z"). When that work lands, the item is a reconcile candidate.

### Review

Show the user the full list. Do not filter. Do not sort. Do not group. Let the user decide what to select or drop.

## Process notes

Do not impose structure. No dates, tags, sections, or rankings inside a pool. The flat list is the point.

One item, one pool. An item lives in exactly one `## Deepening candidates` section or in `plans/BACKLOG.md`, never two places.

Items are drawn, not self-firing. No item activates itself into work; the user chooses when to draw from the pool. Pruning is the exception: planning and implementing skills reconcile against the backlog when shipped work retires an item.