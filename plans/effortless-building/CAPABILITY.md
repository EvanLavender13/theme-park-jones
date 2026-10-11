# Capability: Effortless Building

## Summary

effortless-building makes it easy for the player to turn an idea into a park. The player is an art director, not a bricklayer (docs/vision.md). They say what they want with a few forgiving strokes, such as a path drawn through a handful of points, or a box set down where they point.

The game infers everything it can. It previews the result live, and it rebuilds the result whenever the intent changes (decisions 0004 and 0007).

This capability owns:

- park intent, which is what the player authored;
- the tools that create and change intent, with their ghosts and the physical-validity check;
- turning intent into what is drawn for it;
- the park file as a player document.

This matters because every other capability needs a park to work on, and the player can reshape that park freely.

The capability never finishes. It grows with each new kind of stroke, each inference that saves the player a placement, and eventually procedural building forms.

## Foundation criteria

A new park is flat ground with an entrance at the middle of one edge and a short guest path leading in. Both are intent. The template is fixed. Moving the entrance is a deepening candidate.

The player draws a guest path or a backstage path by clicking points. The path is a run of straight segments between those points. It shows as a ghost while the player draws, and it is committed when they finish.

If a point is within snapping reach of a point or segment of the same kind, the path joins it there, and a segment splits where it is joined. The junction that navigable-networks derives is then the one the player meant.

The player places, moves, and deletes shop and depot boxes. Each box has a fixed footprint and a facing. The player can also delete paths.

The ghost shows a box where it would land, before any commit. The delete tool highlights the path or box under the cursor and offers its deletion as a tentative edit. The player can preview a deletion the same way as a placement (principle 8).

Physical validity is the only reason to refuse an edit (principle 5). An edit is invalid if:

- a box overlaps another box;
- a path passes through a box;
- a footprint or a segment's width leaves the park's bounds.

The ghost of an invalid edit shows as invalid, and its commit is refused. A refused commit leaves the world unchanged.

Bounds are checked across each segment's whole width, not only at its points.

Nothing else is refused. If a shop is far from any path, or a depot has no route, it is placed anyway and simply does poorly.

Tools never write to the world. Each commit is a command queued to deterministic-simulation's cycle. The cycle applies it after the buffer swap and before resolution, so the next tick already sees the resolved result.

Any sequence of add, move, and delete commands, in any order, leaves a legitimate world at every step (principle 2). Nothing crashes, and every derived part rebuilds.

Intent is saved as path points, the segments between them, and box poses, not as sampled or meshed geometry (principle 1). If a park is saved after any sequence of commands, it loads back equal, through deterministic-simulation's walk. A committed placement derives exactly what its ghost showed.

Paths render flat on the ground at their full width, and boxes render as boxes. Ghosts are drawn so they stand out from committed intent.

navigable-networks derives a connector from each box's door to a path. Each connector renders as a walkway: a ribbon of its kind's width and color. When an edit is valid, its ghost shows the walkways that the edit's candidate world has. The player sees what connects, and what doesn't, before committing.

Box rendering also shows display state that the box's owner publishes in its inspection record, such as a shop marked as starved. Display reads these records and doesn't feed anything back (decision 0025).

The app has three command-line options:

- --park PATH loads a park file;
- --ticks N runs N ticks;
- --hash prints the state hash.

With --capture, these options make scripted captures of park files possible.

## Medium

This capability adds no fields or flows. It produces three things, and decision 0025 places them outside principle 3: park intent, tentative intent, and the park file.

Park intent holds:

- path points, and segments between them by kind (guest, backstage);
- boxes by kind (shop, depot, bench, toilet block), with position and facing;
- each shop's stock setting: food, drink, or both;
- the park's opening hours;
- the entrance.

Other capabilities derive from park intent. navigable-networks derives the networks and connections. plausible-operations derives shops and the depot, and reads each shop's stock setting. believable-guests uses the entrance as the place where guests arrive, and reads the opening hours. Connections between boxes and paths are derived, and they aren't saved.

Tentative intent is the edit a ghost stands for. deterministic-simulation provides a candidate copy of the world, and the other capabilities resolve the edit there. legible-simulation samples the candidate's fields for preview context.

This capability shows the ghost's own geometry and validity. It also shows the walkways of the candidate world's networks, which it reads through the medium's public queries. It doesn't sample fields or call another capability's derivation.

This capability owns the park file's save and load commands and the new-park template. The file is encoded by deterministic-simulation's state walk.

The physical-validity check reads only intent geometry and the park's bounds:

- box footprints;
- segments' ground lines and widths;
- the terrain square.

## Principles

- Principle 1 is at risk if derived geometry is saved or edited. Intent holds only points and poses. Path meshes, connections, and graphs are derived. Saves must contain no derived types, as deterministic-simulation's classes define them. The park world checks confirm that a save loads back and resolves equal.
- Principle 2 is at risk from edits that leave a world half built. Every world between edits must be legitimate. The park world checks cover the worlds that a few edits reach from the park files. Check longer edit sequences by playing.
- Principle 5 is at risk from convenience gates, such as requiring a shop to touch a path. The only refusals are overlap and leaving the park's bounds. Check both sides by playing: those edits are refused, and every other edit is accepted.
- Principle 8 is at risk if the ghost differs from the result. The world checks confirm that a candidate equals the world that commits the same commands, so a committed placement derives what its ghost showed.
- Principle 10 is at risk if the input handler applies edits in the middle of a tick. Commands apply only between ticks. The input and rendering code stays outside tpj_sim.

## Dependencies

- deterministic-simulation (world-as-value): the state walk for intent and saves, candidate copies for ghosts, and the command queue that its cycle applies between the swap and resolution. Met.
- shared-medium: nothing directly. The foundation publishes no fields.
- navigable-networks: it derives connections and junctions from intent. Walkway rendering waits on its box-connections feature, because the park and its ghosts draw its connectors as walkways. Junction snapping only has to place endpoints on existing curves.
- The renderer, camera, and ImGui tooling UI: met.
- The player-facing UI question (docs/open-questions.md): open. ImGui tool buttons stand in for it in the foundation.

## Foundation

The foundation is the smallest version that lets a player sketch a park and lets every other slice member derive from it:

- intent for curves and boxes;
- the path and box tools, with ghosts and the overlap check;
- commands applied between ticks;
- paths and boxes on screen;
- park files, with the command-line options that script them.

It is useful on its own. A park can be drawn, reshaped, saved, and captured before anything in it simulates.

## Milestones

1. `sketch-a-park`: the new-park template with its entrance, and guest and backstage paths drawn by clicking points. Paths are centripetal Catmull-Rom curves with endpoint snapping. The player places, moves, and deletes shop and depot boxes. Ghosts show the overlap check, and edits are commands applied between ticks. Paths and boxes render, parks save and load, and the app has the --park, --ticks, and --hash options. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value.
2. `path-network`: paths become a network of shared points joined by straight segments, each with its own width, and each bend with its own corner radius. The player draws, joins, splits, and reshapes them with visible snaps that one key suppresses, and undoes and redoes every edit. Joined paths draw as one paved surface, and boxes are refused against that surface as drawn. Shops and depots placed beside a path face it. Member of the good-paths slice. Depends on: sketch-a-park, and shared-medium's carrier-widths.
3. `facilities-and-hours`: benches and toilet blocks placed beside guest paths with the frontage snap, each shop's stock of food, drink, or both, and the park's opening hours, all as saved intent the player previews, applies, undoes, and redoes. Member of the day-at-the-park slice. Depends on: path-network, and deterministic-simulation's park-clock.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- Freehand strokes: the player sketches a path by dragging, and the game simplifies it into the same points and segments.
- Moving the entrance, and parks with several entrances.
- Box sizes and shapes: resizable footprints, and later procedural building forms resolved from context (decision 0004, docs/design-notes.md). Gated on: the slice's boxes working as shops.
- Paths on terrain: paths that follow the ground, or climb it as ramps and stairs. Gated on: terrain editing.
- Path styles as intent, with queue lines as a kind of path.
- Snapping guides beyond path-network's: parallel offsets and alignment with nearby boxes.
- Stamps beyond a square: other shapes, stamps the player saves from a selection, and stamps that stay grouped so the player can move and rescale them together.
- Frontage as saved intent: a box is attached to a path by a distance along it, a side, and a setback. When the player reshapes the path, its boxes move with it.
- A row tool that lays boxes along a stretch of path in one drag, after Cities: Skylines 2's Line Tool.
- Furniture layout patterns beyond even spacing, such as clusters with gaps and alternating sides, and trims and offsets along a furnished segment.
- Single pieces of furniture placed or removed by hand, as exceptions to a segment's furnishing setting.

## Open questions

- The player-facing UI for the tools (docs/open-questions.md). The user decides. The foundation uses ImGui buttons.
- Snapping reach and the box footprint size are tuning values. sketch-a-park's MILESTONE.md sets them, and playing adjusts them.

## Research notes

- Centripetal Catmull-Rom doesn't form cusps or loops when the clicks are unevenly spaced.
- Tiny Glade shows that forgiving strokes with rich inferred results are what draws players in.
- Paths are flat ribbons on the ground, not tubes. Tubes are the plain look that coasters will take. Ramps and stairs wait for terrain.
- Edits are Command-pattern values applied between ticks. Commands like these enable previews now, and undo and replay later.

The details are in RESEARCH.md.
