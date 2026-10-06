# Research: undo-redo

The milestone's RESEARCH.md settles the record: each history entry holds the points, segments, and boxes an edit touched, whole, before and after, and a map from each replaced key to the key that replaced it. This file adds how the history learns those keys, and what undo does while a path is half drawn.

## How does the history learn the key a restored thing gets?

A restored thing takes a fresh key from the world's counter when its command applies. Guests also take keys from the counter, while the systems step in the same cycle, before the commands apply. The tools can't predict the key when they queue the command, so the history has to read it after the cycle.

The history can read it exactly under two conditions:

- The restore applies alone in its cycle.
- It adds its records in a fixed order: points, then segments, then boxes, each in the order it lists them.

Under those conditions, the records the cycle added are exactly the restored ones. Within each kind, they appear in ascending key order, so the history pairs the i-th restored point with the i-th added point, and likewise for segments and boxes.

The same reading of intent after each cycle also records ordinary edits. The history compares the intent it saw last with the intent after the cycle. The difference is the edit: the records it removed, the records it added, and the records it changed, before and after. This catches every side effect of an edit, such as a radius that a join rewrites at a neighbouring point, without a per-command inverse. Edits that apply in one cycle become one entry. A cycle is 1/30 s, so two clicks rarely share one.

Rejected:

- Predicting keys from the counter when the command is queued. Guests take keys during the same cycle, before commands apply.
- Matching restored records to old ones by their values. Two records can hold equal values, and the research for the milestone rejected matching by position.
- Giving restored records derived keys computed from their old keys. Derived keys belong to resolvers, and the milestone requires keys from the counter.

## What does undo do while a path is half drawn?

In Inkscape's pen tool, Ctrl+Z and Esc cancel the unfinished path, and Backspace removes its last node. Krita users asked for undo of single nodes while drawing a Bézier path. Planet Coaster 2 binds undo to Ctrl+Z, and players report that its undo stops working after long sessions. Ctrl+Y and Ctrl+Shift+Z are the common bindings for redo.

For this game, these findings suggest the following:

- While a path is drawn or the button holds an edit, undo drops it, as Cancel path does, and reverses no committed edit. Redo drops it too, and redoes nothing.
- Use Ctrl+Z for undo, and both Ctrl+Y and Ctrl+Shift+Z for redo.

Rejected: removing only the last stop on undo. It is a separate gesture in Inkscape, and it goes to the milestone's candidates.

Sources:

- https://inkscape.org/doc/keys046.html — Ctrl+Z and Esc cancel an unfinished path, and Backspace removes its last node.
- https://krita-artists.org/t/bezier-curve-undo-only-last-node/10553 — users asking for undo of single nodes while drawing.
- https://steamcommunity.com/app/2688950/discussions/0/4634860720019787587/ — Planet Coaster 2's undo failing in long sessions.
- https://gamerant.com/planet-coaster-2-pc-hotkeys-most-important/ — Ctrl+Z for undo in Planet Coaster 2.
