# Capability: Effortless Building

## Summary

effortless-building deepens how easily the player turns an idea into a park. The player is an art director, not a bricklayer (docs/vision.md). They express intent with a few forgiving strokes: a path drawn through a handful of points, or a box set down where they point. Everything that can be inferred is inferred, previewed live, and rebuilt whenever the intent changes (decisions 0004 and 0007).

This capability owns:

- park intent, meaning what the player authored;
- the tools that create and change it, with their ghosts and the physical-validity check;
- turning intent into what is drawn for it;
- the park file as a player document.

Its value is that every other capability has a park to work on, and the player can reshape that park freely. It deepens with every new kind of stroke, every inference that saves the player a placement, and eventually procedural building forms, and never finishes.

## Foundation criteria

- A new park is flat ground with an entrance at the middle of one edge and a short guest path leading in, both as intent. The template is fixed, and moving the entrance is a deepening candidate.
- The player draws a guest path or a backstage path by clicking points. The path is a centripetal Catmull-Rom curve through the points, previewed as a ghost while drawing, and committed on finish. An endpoint within snapping reach of an existing path of the same kind snaps onto it, so the junction navigable-networks derives is the one the player meant.
- The player places, moves, and deletes shop and depot boxes of a fixed footprint, with a facing, and deletes paths. The ghost shows the box where it would land before any commit.
- Physical validity is the only refusal (principle 5). A box overlapping a box, a path passing through a box, or a footprint or curve leaving the park's bounds shows the ghost as invalid, and its commit is refused. Bounds are checked on the whole curve, since a curve can swing outside between points that are inside. A refused commit leaves the world unchanged. Nothing else is refused: a shop far from any path, or a depot with no route, is placed and simply does poorly.
- Tools never write to the world. Each commit is a command queued to deterministic-simulation's cycle, which applies it after the buffer swap and before resolution, so the next tick already sees the resolved result. Randomized sequences of add, move, and delete commands in any order leave a legitimate world at every step (principle 2): nothing crashes, and every derived part rebuilds.
- Intent is saved as the clicked points and box poses, never as sampled or meshed geometry (principle 1). A park saved after any command sequence loads back equal, through deterministic-simulation's walk. A committed placement derives exactly what its ghost showed.
- Paths render as tubes and boxes as boxes, with the ghost drawn distinctly. The app's --park PATH option loads a park file, --ticks N runs N ticks, and --hash prints the state hash. With --capture, these make scripted captures of park files possible.

## Medium

This capability introduces no fields or flows. What it produces sits outside principle 3 by decision 0025:

- Park intent: path curves by kind (guest, backstage), boxes by kind (shop, depot) with position and facing, and the entrance. navigable-networks derives the networks and connections from it, plausible-operations derives shops and the depot, and believable-guests takes the entrance as where guests arrive. Connections between boxes and paths are derived, never saved.
- Tentative intent: the edit a ghost stands for. deterministic-simulation provides a candidate copy of the world, the other capabilities resolve the edit there, and legible-simulation samples the candidate's fields for preview context. This capability shows the ghost's own geometry and validity. It never samples fields or calls another capability's derivation.
- The park file: this capability owns the save and load commands and the new-park template. The file's encoding is deterministic-simulation's state walk.

The physical-validity check reads only intent geometry and the park's bounds: box footprints, path curves, and the terrain square.

## Principles

- Principle 1 is at risk when derived geometry is saved or edited. Intent holds only points and poses. Tubes, connections, and graphs are derived, and saves are checked to contain no derived types (deterministic-simulation's classes).
- Principle 2 is at risk from edits that leave half-built states. Randomized command-sequence tests check that every intermediate world is legitimate.
- Principle 5 is at risk from convenience gates, such as requiring a shop to touch a path. The only refusals are overlap and leaving the park's bounds, and they are tested both ways: those refused, everything else accepted.
- Principle 8 is at risk if the ghost differs from the result. Tests check that a committed placement derives what its ghost showed.
- Principle 10 is at risk if edits land from the input handler mid-tick. Commands apply only between ticks, and the input and rendering code stays outside tpj_sim.

## Dependencies

- deterministic-simulation (world-as-value): the state walk for intent and saves, candidate copies for ghosts, and the command queue its cycle applies between the swap and resolution. Unmet; planned.
- shared-medium: nothing directly. The foundation publishes no fields.
- navigable-networks: derives connections and junctions from intent. The foundation's tools and rendering work without it, and junction snapping only has to place endpoints on existing curves. Unmet; to be planned.
- The renderer, camera, and ImGui tooling UI: met.
- The player-facing UI question (docs/open-questions.md): open. ImGui tool buttons stand in for the foundation.

## Foundation

The foundation is intent for curves and boxes, the path and box tools with ghosts and the overlap check, commands applied between ticks, tubes and boxes on screen, and park files with the command-line options that script them. That is the smallest version that lets a player sketch a park and lets every other slice member derive from it. It produces value on its own: a park can be drawn, reshaped, saved, and captured before anything in it simulates.

## Milestones

1. `sketch-a-park`: the new-park template with its entrance, guest and backstage paths drawn by clicked points as centripetal Catmull-Rom curves with endpoint snapping, placing, moving, and deleting shop and depot boxes, ghosts with the overlap check, edits as commands applied between ticks, tubes and boxes rendered, save and load, and the --park, --ticks, and --hash options. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- Reshaping paths: drag a path's points, insert and remove points, and split or join paths.
- Undo and redo: commands are values, so the history is a list of them.
- Freehand strokes: sketch a path by dragging, simplified and smoothed into the same curve intent.
- Snapping guides: parallel offsets, angle hints, and alignment with nearby boxes, offered and never forced.
- Moving the entrance, and parks with several entrances.
- Box sizes and shapes: resizable footprints, and later procedural building forms resolved from context (decision 0004, docs/design-notes.md). Gated on: the slice's boxes working as shops.
- Paths on terrain: paths following or cutting terrain, with rotation-minimizing frames for their tubes. Gated on: terrain editing.
- Path styles and widths as intent, with queue lines as a kind of path.

## Open questions

- The player-facing UI for the tools (docs/open-questions.md). Evan decides. The foundation uses ImGui buttons.
- Snapping reach and the box footprint size are tuning values. They are settled while planning sketch-a-park, and adjusted by playing.

## Research notes

- Centripetal Catmull-Rom never forms cusps or loops from uneven clicks.
- Tiny Glade shows that forgiving strokes with rich inferred results are the draw.
- Tubes on flat ground need no special framing; rotation-minimizing frames matter once paths climb.
- Edits are Command-pattern values applied between ticks, which enables previews, and later undo and replay.

Depth is in RESEARCH.md.
