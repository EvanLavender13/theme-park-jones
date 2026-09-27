# Vision

A theme park game in the lineage of RollerCoaster Tycoon, sitting between Parkitect (simple stylized graphics, easy building, staff management) and Planet Coaster (gridless, powerful coaster building), with a simulation that feels meaningful beyond cashflow and worker micromanagement.

The core feeling: the player dabbles with the ghost of an idea and ends up with something that looks good, without hand-placing window panes, fighting pixel alignment, or wondering what a theme park building should look like. The player is an art director, not a bricklayer. The player expresses intent at a high level, authored systems resolve it into something coherent, and the player curates rather than constructs.

The biggest gripe with existing games, and the reason the project starts with building: Planet Coaster 2 offers nothing but manual placement of tiny individual pieces.

The target is a plausible theme park, not a realistic one and not an unconstrained sandbox. Coaster-builder tools that are really CAD and rendering software are the counterexample: maximum control, nothing at stake, no game.

## Reference points

- Tiny Glade: gridless, hand-authored procedural rules that react to simple player inputs.
- Townscaper.
- Urbek City Builder: buildings shaped by their surroundings.
- Jurassic World Evolution 3: brush-based terrain and biome-aware foliage.
- Cities: Skylines, as a warning: many overlays, but no way to tell why things happen or what a change will do.
- RollerCoaster Tycoon: click a guest and read their thoughts.

## The four original pillars

Realistic-feeling rides with stylized graphics; procedural building; procedural painting of nature and paths (paths want structure, nature wants scatter); a compelling simulation.

## First slice

The boxes-and-tubes slice, a few weeks of work spanning several foundational capabilities (decision 0023): terrain with an orbit camera; a path drawn as a curve that becomes a graph; one generic food shop as a box that samples demand and draws supplies from a depot over a backstage path; guests as simple shapes that carry hunger and pathfind by network distance; one field overlay with source attribution; a placement preview. No procedural building geometry and no coasters yet.
