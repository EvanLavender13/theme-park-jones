# Vision

Theme Park Jones is a theme park game in the lineage of RollerCoaster Tycoon. It sits between Parkitect (simple stylized graphics, easy building, staff management) and Planet Coaster (gridless, powerful coaster building). Its simulation should feel meaningful beyond cashflow and worker micromanagement.

The core feeling is this: the player plays around with a vague idea and ends up with something that looks good. They don't hand-place window panes, fight pixel alignment, or wonder what a theme park building should look like. The player is an art director, not a bricklayer. The player states intent at a high level, authored systems turn it into something coherent, and the player curates rather than constructs.

The biggest complaint about existing games is that Planet Coaster 2 offers nothing but manual placement of tiny individual pieces. That is why the project starts with building.

The target is a plausible theme park. It is not a realistic one, and not an unconstrained sandbox. The counterexample is coaster-builder tools that are really CAD and rendering software: they give maximum control, but nothing is at stake, so there is no game.

## Reference points

- Tiny Glade: gridless, hand-authored procedural rules that react to simple player inputs.
- Townscaper.
- Urbek City Builder: buildings shaped by their surroundings.
- Jurassic World Evolution 3: brush-based terrain and biome-aware foliage.
- Cities: Skylines, as a warning: many overlays, but no way to tell why things happen or what a change will do.
- RollerCoaster Tycoon: click a guest and read their thoughts.

## The four original pillars

- Realistic-feeling rides with stylized graphics.
- Procedural building.
- Procedural painting of nature and paths. Paths want structure; nature wants scatter.
- A compelling simulation.

## First slice

The first slice is the boxes-and-tubes slice. It is a few weeks of work and spans several foundational capabilities (decision 0023). It contains:

- terrain with an orbit camera;
- a path drawn as a curve that becomes a graph;
- one generic food shop, shown as a box, that samples demand and draws supplies from a depot over a backstage path;
- guests as simple shapes that carry hunger and pathfind by network distance;
- one field overlay with source attribution;
- a placement preview.

It has no procedural building geometry and no coasters yet.
