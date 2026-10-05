# Research: points-and-segments

The milestone's RESEARCH.md covers undo, crossing splits, edge lines, and corner arcs. This file covers how a drawn path refers to what it joins, and which part of a split segment keeps its key.

## How should a drawn path name the points and segments it joins?

OpenStreetMap's data model is the closest match to points and segments. Nodes hold positions, and ways list node IDs in order, so two ways join exactly when they list the same node. Nothing joins by position. iD adds a node to a way by naming the way and the index of the stretch it goes in, and places the node at a position on that stretch. A segment here is a way of exactly two nodes, so the stretch is the segment itself.

For this feature, each stop of a drawn path names what it lands on:

- a new point at a ground position;
- an existing point, by its key;
- an existing segment, by its key and the fraction of the way from its first point to its second.

A fraction puts the new point exactly on the segment's line, so a guest standing there stays on its ground when the segment splits. It also can't name a place off the segment.

Rejected: joining by position alone — two points at the same place would join without the player asking, which is the Planet Coaster 2 failure. Naming a segment with a ground position — a position slightly off the line bends both parts, and a guest on the line would no longer be on its ground.

Sources: https://wiki.openstreetmap.org/wiki/Elements — nodes with positions, ways as ordered lists of node IDs; https://github.com/openstreetmap/iD/blob/develop/modules/actions/add_midpoint.js — adding a node to a way at a stretch and a position.

## Which part of a split segment keeps its key?

OpenStreetMap editors keep the original way's ID on one part, so its history stays with it. JOSM keeps it on the longest part by default. iD keeps it on the longest part or on the first, depending on an option.

Here, nothing keeps history by key. ground-carry-over keeps a guest or a footfall entry on its ground whichever part keeps the key. The first part, from the segment's first point to the first split, keeps the key. That rule needs no comparison of lengths and gives the same answer however the parts are measured.

Rejected: keeping the key on the longest part — it adds a rule for ties and changes nothing the player sees. Giving every part a fresh key — every holder of a place on the segment would move to another carrier, and footfall on the first part would regroup for no reason.

Sources: https://josm.openstreetmap.de/doc/org/openstreetmap/josm/command/SplitWayCommand.html — JOSM's split command and its strategy for the part that keeps the way; https://github.com/openstreetmap/iD/blob/develop/modules/actions/split.js — iD's split and its option for the part that keeps history.
