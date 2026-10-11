# 0040. Staff cross between guest and backstage paths only through buildings

Status: Accepted, 2026-10-10.

## Context

docs/open-questions.md asked where staff pass between the guest paths and the backstage paths. It offered three ways: at every crossing, only at gates the player places, or only through buildings open on both sides. The choice decides where staff appear in guest areas, and so how much visible backstage affects theme.

The litter-and-janitors slice brings the first staff, janitors who work on guest paths and send garbage out over backstage paths, so it needs the answer. Guest and backstage paths never join today. Boxes already join the two networks: a shop has a guest door at its front and a backstage door at its back.

## Decision

Staff pass between the guest paths and the backstage paths only through a building with a door on each side, such as a staff room. Guest and backstage paths never join each other, and staff never cross where the two kinds of path meet or overlap.

## Consequences

Where the player puts a staff room decides where its staff enter the guest paths, and so how far they walk to their work. A staff room needs both a guest door and a backstage door to do both halves of its job.

No network joins the two kinds of path. A composed staff network, as navigable-networks' deepening candidates describe, would join them only at such buildings.

Gates the player places are not ruled out. A later decision can add them as a kind of building open on both sides.
