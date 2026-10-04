---
name: maintaining-backlog
description: Use whenever the user wants to add, select, drop, or reconcile a speculative idea, or whenever a planning or implementing skill needs to deposit one or prune items that shipped work has implemented or made moot. Ideas live in three pools, chosen by the narrowest scope that fits. If an idea fits one existing milestone, it goes to that MILESTONE.md's "Deepening candidates" section. If it is scoped to one capability and fits no milestone, it goes to that CAPABILITY.md's section. If it spans capabilities or has no capability yet, it goes to plans/BACKLOG.md. Items are added when ideas fall out of current scope, selected when a new piece of work needs a starting point, and dropped when shipped work retires them. Use this even for a single idea.
---

# Maintaining the Backlog

## Purpose

Keep the pools of speculative ideas. Add items when they fall out of current scope. Select items when a new piece of work needs a starting point. Drop items the user no longer wants. Reconcile items against shipped work so that nothing already built stays in a pool.

The pools hold ideas without committing to them. Most items will never be implemented, and that is intended.

## Routing

Ideas live in one of three pools. Choose the pool with the narrowest scope that fits.

First apply decision 0028's test. Ask whether a player would notice the idea missing if the machine were infinitely fast and the code never changed again. If not, as with a speedup or a restructure, put it in an engineering capability's pools, whichever capability's work surfaced it. If no engineering capability fits, put the idea in `plans/BACKLOG.md`.

- If an idea fits the theme of one existing milestone, put it in the `## Deepening candidates` section of that milestone's `MILESTONE.md`. Milestones are durable, so keep adding candidates to a milestone after it ships.
- If an idea is scoped to one existing capability but fits no milestone, put it in the `## Deepening candidates` section of that capability's `CAPABILITY.md`.
- If an idea spans capabilities, or has no capability yet, put it in `plans/BACKLOG.md`.

If the `## Deepening candidates` section doesn't exist, create it. For a milestone, open it with the line "Unordered pool this milestone draws later features from." For a capability, open it with "Unordered pool this capability draws later milestones from."

Every operation below applies to all pools. When a capability or milestone that fits an item is created, or the item's scope narrows so that one now fits, move the item to the narrower pool.

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
2. Show each candidate with the shipped work that made it unnecessary.
3. Delete the ones the user confirms.

Many items name what they wait on ("deferred from X", "gated on Y", "ships with Z"). When that work lands, the item is a candidate for reconciling.

### Review

Show the user the full list. Don't filter, sort, or group it. Let the user decide what to select or drop.

## Process notes

Don't impose structure. Use no dates, tags, sections, or rankings inside a pool. Keep each pool a flat list.

Put each item in exactly one pool: either one `## Deepening candidates` section or `plans/BACKLOG.md`.

Don't start work from an item on your own. The user chooses when to draw from a pool. Pruning is the exception: when shipped work implements an item or makes it moot, planning and implementing skills reconcile the backlog against it.
