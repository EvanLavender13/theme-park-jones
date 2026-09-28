# Milestone: Sketch a Park

Slice: boxes-and-tubes

## Summary

sketch-a-park gives the park something to be about. It adds park intent (guest and backstage paths as centripetal Catmull-Rom curves through clicked points, shop and depot boxes with a pose, and the entrance) as registered intent in tpj_sim, with the new-park template and the edits that change intent as commands applied between ticks. Physical validity is their only refusal. On screen, paths render flat on the ground and boxes as boxes. The player draws paths, snapping endpoints onto existing paths, and places, moves, and deletes boxes and paths with a ghost shown before each commit. Park files are opened and saved through the app, and the --park, --ticks, and --hash options script captures. It comes third in the slice because navigable-networks, plausible-operations, and believable-guests all derive their parts of the world from this intent, and every capture the slice checks draws into this view. It needs only world-as-value: nothing here samples a field or moves a flow.

## Acceptance criteria

1. A new park is the fixed template: the entrance at the middle of one edge of the park's square and a short guest path leading in from it, both as intent, with nothing else, and the template passes the physical-validity check. Intent holds only clicked points, poses, and kinds, registered as intent, so a save holds nothing sampled or meshed (principle 1).
2. A path's curve is the centripetal Catmull-Rom curve through its points, with phantom end points reflected through its ends. It passes through every point in order. Its ground line is the curve sampled by a fixed rule that depends on the points alone. One evaluation in tpj_sim serves the check, the renderer, and navigable-networks, so what is refused, drawn, and walked is the same line.
3. Commands add a path of a kind through points, add a box of a kind at a pose, move a box to a pose, and delete a path or a box. A box's facing is a direction on the ground, not an angle, since the simulation calls no sin or cos. Intent holds it as the command gave it, and geometry normalizes it where it is used. The only refusals are physical (principle 5). A command describes no physical object: a coordinate that is not finite, a facing of zero length, or a path with fewer than two distinct points once repeated consecutive points are dropped. Two footprints overlap, the entrance's included. A path's ground line comes within half its width of a footprint. A footprint or a path's ground line with its half width leaves the park's square. A refused command leaves the world unchanged. Everything else is accepted, including a box far from any path.
4. Randomized sequences of commands of every kind, including ones that name deleted or missing entities, ones that describe no physical object, and ones the check refuses, leave a legitimate world after every cycle (principle 2). The world saved after any such sequence loads back equal, and saving it again gives an identical file.
5. Each tool builds a tentative command from ground positions and button presses, in a library that links tpj_sim alone. The ghost shows exactly that command's intent, marked valid when and only when the command would be accepted, and committing queues the same command, so the committed intent equals what the ghost showed (principle 8). Tests drive the tools with synthetic input.
6. A path endpoint within snapping reach of a path of the same kind snaps onto its ground line, and the ghost shows it snapped. The delete tool highlights the path or box it hovers and shows the deletion as a ghost before the click commits it.
7. The app draws each path flat on the ground, its width wide, along its ground line and each box and the entrance as a box, with guest and backstage paths and shop and depot boxes told apart, and ghosts drawn translucent with a distinct tint when invalid. Marking a box with display state, such as a starved shop, is left to plausible-operations' milestone, which has the first state to show and so can check it.
8. --park PATH starts from a park file, --ticks N steps it N ticks before the first frame, and --hash prints the state hash after them. The hash equals tpj_scenarios' hash for the same file after the same ticks. tests/parks/sketch.park, holding both kinds of path and both kinds of box, is checked in, so the cross-build check runs it, and a --capture of it shows its paths and boxes.
9. In the running app, the tooling panel selects a tool, draws guest and backstage paths with snapping, places, moves, and deletes boxes, deletes paths, and starts a new park, opens a park file, and saves one. A park saved from the app and reopened is unchanged. (manual)
10. src/sim/park/SPEC.md and src/tools/SPEC.md describe the intent, curves, commands, check, template, and tools as built, and src/app/SPEC.md and src/render/SPEC.md their changes. linux-debug builds without warnings and its tests pass, windows-debug builds, and the cross-build check passes.

## Medium

This milestone introduces no fields or flows. What it provides sits outside principle 3 by decision 0025.

- Park intent (park-intent, changed by park-edits): paths by kind with their points, boxes by kind with position and facing, and the entrance. navigable-networks derives networks and connections from it through the shared curve evaluation and ground line, plausible-operations derives shops and the depot from the boxes, and believable-guests takes the entrance's key as where guests arrive and leave.
- Box geometry (park-intent): each box's footprint rectangle and its front and back faces, derived from its pose and fixed size with basic operations, so navigable-networks connects from the faces its capability names without recomputing them.
- Tentative commands (box-tools, path-tool): the edit a ghost stands for, a command value. legible-simulation passes it to deterministic-simulation's makeCandidate for previews. The tools never resolve it themselves.
- The park file (park-view, park-files): saveWorld's text, opened and saved by the app. The encoding is deterministic-simulation's.
- Box marks: not in this milestone. plausible-operations' supplied-food-shop adds the starved mark to the box rendering park-view builds, reading its shop's inspection record for display (decision 0025, and the slice's inspection-records entry).

Inside the milestone, park-edits applies commands to park-intent's types through the module's own functions, the tools submit commands and read intent only through the module's public queries, and the app and renderer read intent the same way. The check reads only intent geometry and the park's square.

## Dependencies

- deterministic-simulation's world-as-value: registered intent types, commands applied between the swap and resolution, makeCandidate, saves, and the cross-build check. Met.
- shared-medium's first-field-and-flow: not needed. Met in any case.
- The renderer, orbit camera, ImGui tooling panel, and --capture: met.
- SDL3's file dialogs, already in the linked SDL: met.

## Core feature

park-intent is the core. It gives the park its first content: the intent types registered with the walk, the curve and its ground line, box geometry, and the new-park template, so a new park saves, loads, and runs through tpj_scenarios and the cross-build check. navigable-networks can plan against it, and every later feature here reads it.

## Features

1. `park-intent`: the module src/sim/park with path, box, and entrance intent registered as intent in makeParkSchema, the park's square, fixed sizes and path widths, the centripetal Catmull-Rom evaluation and ground line, box footprints and faces, public queries over intent, and the new-park template. Depends on: none.
2. `park-edits`: the add, move, and delete commands with duplicate points dropped, the physical-validity check as the only refusal, exposed as a query tools use for ghosts, and the randomized command-sequence and save tests. Depends on: feature 1.
3. `park-view`: a lit mesh pipeline drawing flat paths along ground lines, boxes, and the entrance from intent, the app starting from the new-park template, the --park, --ticks, and --hash options, and tests/parks/sketch.park. Depends on: feature 1.
4. `box-tools`: the library src/tools with tool state driven by ground positions, the tooling panel's tool selection, cursor-to-ground picking, the place, move, and delete tools for boxes, and the delete tool for paths, with ghosts and hover highlights drawn in a translucent pass. Depends on: features 2 and 3.
5. `path-tool`: drawing a guest or backstage path by clicking points, with its ghost curve, endpoint snapping onto same-kind paths, finishing and cancelling. Depends on: feature 4.
6. `park-files`: new, open, and save from the tooling panel through SDL's file dialogs, replacing the world and emptying the command queue on open and new. Depends on: feature 4.

## Tuning values

The capability leaves these to this milestone. Playing adjusts them.

- The park's square: 256 m, as the terrain already is, centered on the origin.
- Path widths: guest 3 m, backstage 2 m.
- Footprints: shop 8 m wide by 6 m deep, depot 12 m by 8 m, entrance 10 m by 3 m.
- Snapping reach: 2 m from a path's ground line.
- Finishing reach: 1 m from a path's last drawn point, where a click finishes the path.
- The template: the entrance at the middle of the edge at +z, facing into the park, and a straight guest path of 20 m from just in front of it.

## Deepening candidates

- Facing inferred from the nearest path: a box set down near a path turns its front to it unless the player drags a facing. Gated on: navigable-networks' connections by face, so the inference serves a real connection.
- Snapping preferences: an endpoint near another path's end joins it end to end rather than landing beside it.
- Keyboard shortcuts for tools and for finishing or cancelling a path.
- Turning a placed box, in place or while moving it. box-tools' move keeps a box's facing, so turning one means deleting and placing it again.
- Undo and redo, and reshaping paths, from the capability's pool.
- A refusal's reason given with isAccepted, such as which box a ghost overlaps, so the ghost can say why it is invalid.
- Ground lines cached as derived data, so the physical-validity check stops recomputing every path's line on each query, if box-tools' ghosts show the cost.
- Crossings and junctions drawn cleanly: where ribbons meet or cross they overlap, and ribbons of different kinds tie in depth where they cross.

## Open questions

- The ground line's sampling rule decides both how smooth paths look and how many carrier points navigable-networks carries. Settled while planning park-intent, and checked by park-view's captures.

## Research notes

- A box's facing is a ground direction, since sin and cos are denied to the simulation, and that keeps rotation free.
- The validity check runs on the canonical ground line with the path's width, so it is exact on what is drawn and walked. Rectangles overlap by the separating axis test.
- Open paths reflect their end neighbors as phantom points, and repeated points are dropped, since centripetal spacing divides by chord lengths.
- SDL3's asynchronous file dialogs open and save park files, and their callback only hands the path to the main loop.
- Tool logic lives in a library driven by ground positions, so the ghost-equals-commit property is tested.
- A highlight lies exactly on what it marks, so the translucent pass tests depth greater-or-equal, and park.vert's position is invariant so both pipelines agree on depth.
- A click on a path's last drawn point finishes it, so a double-click does, and the ghost while the cursor rests there is the path that commits. The path tool snaps every point it draws onto the nearest point of a same-kind ground line.

Depth is in RESEARCH.md.
