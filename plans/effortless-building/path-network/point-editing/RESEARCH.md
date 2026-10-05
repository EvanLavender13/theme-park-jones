# Research: point-editing

The slice's RESEARCH.md covers how node-and-segment editors delete nodes and segments, and the milestone's RESEARCH.md covers splits at crossings. This file covers what happens when a dragged point is dropped.

## What should happen when a dragged point is dropped on another point, on a segment, or across segments?

iD, the OpenStreetMap web editor, snaps a dragged node to nodes and ways under the pointer. It leaves out the ways the dragged node belongs to. Dropped on a node, the two connect and the target node survives. Dropped on a way, the node is added to that way at the nearest place on it, as a new middle node. If the result would be invalid, such as a way crossing itself where that is not allowed, iD shows a refusal while dragging and puts the node back on drop. A held modifier turns snapping off. A press and release without movement changes nothing.

JOSM merges a dragged node into the node it is dropped on only while Ctrl is held. Its Merge Nodes action keeps the oldest node's identity.

These findings suggest the following for this feature:

- Snap a dragged point to points and segments of its kind, leaving out the point itself and the segments at it, as iD does.
- Dropped on a point, the dragged point merges into the target, which keeps its key. A segment between the two collapses and goes.
- Dropped on a segment, the segment splits at the dragged point, which keeps its key.
- Where a moved segment crosses another of its kind, apply crossing-splits' rules, so a moved path and a drawn path behave the same.
- When two segments merge because their shared point is deleted, keep the lower key, as JOSM keeps the oldest node.
- A click on a point without a drag commits nothing.
- Snapping without a modifier, as iD does. The held suppress key comes with snaps-and-guides.

Rejected: merging only while a key is held, as JOSM does — every other join in this model happens at a visible snap without a key, and the suppress key already turns snapping off in snaps-and-guides. Putting the point back when the drop would be invalid — the game refuses invalid commits anyway, and the ghost shows the invalid tint while dragging (principle 5).

Sources: https://raw.githubusercontent.com/openstreetmap/iD/develop/modules/modes/drag_node.js — iD's snapping targets, exclusions, connect on drop, and refusal; https://josm.openstreetmap.de/wiki/Help/Action/MergeNodes — JOSM keeps the oldest node; https://josm.openstreetmap.de/wiki/Help/Action/Select — Ctrl-drag to merge nodes.
