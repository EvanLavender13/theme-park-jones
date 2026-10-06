# Research: Predictable Editing

## How can a path join a curve without moving it?

A circular arc that rounds a corner can be cut at any place on it into two arcs of the same radius. Each is a rounded corner in its own right. The line that touches the arc at the cut is the new straight segment between them. Each new corner point lies where that line meets one of the old segment lines. The tangents from that point to the circle are equal, so each new corner's tangent length equals the distance from its point to the cut. The new arcs therefore lie exactly on the old one, and the cut point lies on a straight run with no corner. Several cuts work the same way, with a corner point between each two cuts where their touching lines meet. Every step needs only dot products, cross products, and square roots, so it fits decision 0022.

Rejected: joining at the corner's point instead — on a large curve the point lies meters off the paving, and the ghost would show the path running somewhere the player didn't click. Refusing joins on curves — the user rejected it, since paths must be able to meet curved paths.

Sources: src/sim/park/layout.cpp — how roundingOf builds the arc from the point, Toward, the tangent length, and the radius.

## Why do curves shrink when a path beside them changes?

Two limits split each segment in half between its ends. A corner's arc may touch a segment no farther than half its length. A paving edge may reach half the segment's length whenever the other end has another segment. Inserting a point on a straight-on run therefore halves the room a curve has, though the new point uses none of it. Sharing by what the other end asks for keeps every curve whose ends fit together. It falls back to halves only when both ends ask for more than half.

Rejected: refusing edits that would shrink a curve — most such edits need no room at all, so refusing them would block ordinary inserts.

Sources: src/sim/park/layout.cpp (maxRadius in roundingOf); src/sim/park/surface.cpp (Shape::reach).

## What should deleting do?

Today one Delete click can do three different things. It removes a point and every segment at it, merges two segments into a new straight one, or removes a segment, depending on whether an unseen point lies within 2 m. A tool that only removes the paving under the pointer, with a separate tool for merging, makes each click do one thing the ghost can show.

Rejected: merging with Edit path's click on a point — Edit path's press on a point starts a drag, so a click that only meant to grab a bend would straighten it.

Sources: src/tools/tools.cpp (tentativeEdit for Delete); src/sim/park/edits.cpp (planDelete).
