# Feature: Box Tools

## Summary

box-tools lets the player change the park. A new library, src/tools, holds tool state driven by three inputs, where the pointer meets the ground, a press, and a release, and gives each tool's tentative edit and the edit a release commits. The edit is a ParkEdit, a variant of the park's five commands, added to sim/park with isAccepted and queueEdit over it. The place tools set a shop or depot down where the player presses, facing the way they drag. The move tool carries a box by its grab point and keeps its facing. The delete tool marks the box, or else the path, under the pointer. A release commits exactly the tentative edit the ghost showed, whether or not it is accepted, and the simulation's refusal is the only one. tpj_render builds each edit's ghost with the functions that draw the park, in the kind's color made translucent when isAccepted, in an invalid tint when not, and in a delete tint for a deletion. It draws ghosts and the move tool's hover highlight in a translucent pass that blends, tests depth greater-or-equal, and writes none, with the park's vertex shader position invariant, so a highlight lying on its box draws. groundAtCursor turns the cursor into a ground position. The app gains a Tools panel, the cursor's ground position, the left button, and a command queue its ticks apply.

## Acceptance criteria

1. isAccepted on a ParkEdit equals isAccepted on the command it holds, and a cycle with queueEdit's queue gives the world a cycle with that command pushed directly gives.
2. boxAt gives the least-keyed box whose footprint holds the point, with its edges included, and none when no box's footprint holds it. pathAt gives the least-keyed path whose ground line has a segment within half its pathWidth of the point, and none when no path's does. Neither gives the entrance.
3. For every tool, a release gives exactly the tentativeEdit of the moment before it when the press before it took hold, and none otherwise, whether or not isAccepted is true for that edit (principles 5 and 8).
4. A place tool not holding shows an AddBox of its kind at the pointer with the tool's facing. A press on the ground lands the box there. While it holds, the landing facing becomes the pointer minus the landing position whenever the pointer is at least MIN_FACING_DRAG from it, and is kept otherwise. After a release, the tool's facing is the one that release committed.
5. The move tool not holding highlights boxAt's box under the pointer and shows no edit. A press over a box takes hold of it. While it holds, its edit is a MoveBox of that box to the pointer plus the offset its position had from the pointer at the press, with the facing the box had, or none while that pose is the box's pose at the press.
6. The delete tool shows DeleteBox for the box under the pointer, else DeletePath for the path under it, else nothing, as boxAt and pathAt give them.
7. buildGhostMesh gives the mesh the render spec's Ghosts section defines for each kind of edit: translucent in its kind's color when accepted, INVALID_TINT when not, and DELETE_TINT for a deletion. appendEntity draws the box or path a key holds as buildParkMesh draws it, in a given color. GHOST_ALPHA, INVALID_TINT, DELETE_TINT, and HIGHLIGHT_TINT are translucent, and the three tints differ in red, green, and blue from each other and from the path and box kinds' colors.
8. For an accepted AddBox or MoveBox on a resolved world, the ghost's vertex positions and normals equal, in order, those buildParkMesh draws for that box in makeCandidate of the world with the edit queued (principle 8).
9. groundAtCursor gives a point that the projection drawFrame uses for the same view and aspect maps back to the cursor's position, within 1e-3 in normalized device coordinates, and none when the eye is not above the ground or the ray does not descend.
10. In the running app, the Tools panel selects each tool. The place tools show a translucent ghost at the cursor, red where it overlaps or leaves the park. They place a box on click and turn it by dragging. Move box highlights the box under the cursor and carries it. Delete marks the box or path under the cursor in orange and removes it on click. Highlights and deletion ghosts draw over their geometry without flicker or gaps. Checked by hand (manual).

## Medium

None. Tools read intent, which sits outside principle 3 (decision 0025), through parkBoxes, parkPaths, groundLine, and footprintOf, and give commands the app queues. The ghost builder reads the same queries and isAccepted. The feature samples, emits, draws, and supplies nothing.

## Principle checks

- Principle 1: ghosts and highlights are derived and never saved. buildGhostMesh and appendEntity take the world as const and change nothing.
- Principle 5: tools refuse nothing. A release commits the tentative edit whatever isAccepted says (criterion 3), and the simulation's physical check is the only refusal.
- Principle 6: tpj_tools and tpj_render read intent only through sim/park's public headers, which the private header check covers.
- Principle 8: a release commits exactly the tentative edit the ghost showed (criterion 3), the ghost is marked valid exactly when isAccepted is true (criterion 7), and an accepted ghost has the committed box's geometry (criterion 8).
- Principle 10: tpj_tools links tpj_sim alone, and its tests link it alone. Tools never write to the world: their functions take it as const, and edits reach it only through the command queue between ticks.

## Spec changes

src/sim/park/SPEC.md: at the end of the Commands section, add:

```markdown
Commands compare equal when their fields do. A ParkEdit is any one of the five commands, the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on the command it holds, and queueEdit pushes that command to a CommandQueue as its own type, so a cycle applies it exactly as if it had been pushed directly.
```

src/tools/SPEC.md: create:

```markdown
# tools

The park's editing tools. The library tpj_tools links tpj_sim alone, so its logic runs and is tested without a window (principle 10). A tool never changes the world: it reads intent through sim/park's public queries and geometry (sim/park/SPEC.md), and gives edits, ParkEdit values, which the app queues for the next cycle.

## Input

A ToolState holds one tool, of a ToolKind: None, PlaceShop, PlaceDepot, MoveBox, or Delete. Its fields are public for reading, but only four calls change them. selectTool sets the kind and drops any hold without committing, keeping the pointer and the place tools' facing. movePointer records where the pointer meets the ground, or none when it meets no ground or is over a panel. pressPointer is the primary button going down, and whether it takes hold is up to the tool; a press while holding changes nothing. releasePointer is the button going up. When the press before it took hold, it gives exactly the edit tentativeEdit gave just before it for the same world, and otherwise none, and then it holds nothing.

tentativeEdit gives the edit the ghost shows, if any, and highlightedEntity the entity the tool marks, if any, which only MoveBox ever gives. Neither changes the state or the world. A tool refuses nothing: its edit is committed whether or not isAccepted is true for it, and the simulation's refusal is the only one (principle 5).

## Picking

boxAt gives the first box, in parkBoxes' key order, whose footprint holds a ground point: with d the point minus the pose's position, and Forward and Right from footprintOf for its kind's boxSize, |d · Forward| is at most half the depth and |d · Right| at most half the width. A box with no footprint holds nothing. pathAt gives the first path, in parkPaths' order, for which some segment between consecutive points of its ground line lies within half its pathWidth of the point, a segment's distance being that of its nearest point. Neither gives the entrance.

## Tools

None gives no edit and no highlight, and never takes hold.

PlaceShop and PlaceDepot place a shop and a depot. The tool keeps a facing, (0, -1) at first. Not holding, its edit is an AddBox of its kind at the pointer's position with that facing, and none when the pointer has no ground position. A press with a ground position takes hold, landing the box at that position with the tool's facing. While it holds, each movePointer to a ground position whose dx * dx + dz * dz from the landing position is at least MIN_FACING_DRAG squared, MIN_FACING_DRAG being 1 m, sets the landing facing to the pointer minus the landing position, as (dx, dz); a nearer position or none leaves it. Holding, its edit is the AddBox of its kind at the landing position with the landing facing. A release while it holds sets the tool's facing to the landing facing, so the next box faces the same way.

MoveBox moves a box. Not holding, it gives no edit and highlights the box boxAt gives under the pointer. A press with a ground position over a box takes hold of it, keeping its key, its pose, and the offset of its position from the pointer, and setting the target to its pose. While it holds, each movePointer to a ground position moves the target's position to the pointer plus the offset, keeping the facing; none leaves it. Holding, its edit is a MoveBox of the held key to the target, or none while the target equals the kept pose, so a click without a drag commits nothing. It highlights nothing while holding.

Delete gives DeleteBox of the box boxAt gives under the pointer, or when there is none DeletePath of the path pathAt gives, or none, whether or not it holds, and highlights nothing: its ghost marks what it deletes. Every press takes hold.
```

src/render/SPEC.md: in the Contract, after the paragraph beginning "setParkMesh uploads a park mesh", add:

```markdown
setGhostMesh uploads the translucent mesh of ghosts and highlights, replacing the one drawn before; an empty mesh draws nothing. drawFrame draws it after the park mesh, through a pipeline with the park mesh's shaders that blends by the vertex color's alpha, tests depth greater-or-equal without writing it, and culls back faces, so a ghost never hides what lies behind it, and a highlight lying exactly on a box or ribbon still draws. park.vert declares gl_Position invariant, so both pipelines compute the same depth for the same vertex.

groundAtCursor, in picking.h, gives the point on the ground under a cursor, for a CameraView, the view's aspect ratio, and the cursor's normalized device coordinates: x from -1 at the left edge to 1 at the right, and y from -1 at the bottom to 1 at the top. The ray leaves the eye along f + r * (x * tan(FovY / 2) * aspect) + u * (y * tan(FovY / 2)), where f is the unit direction from the eye to the target, r is normalize(cross(f, +Y)), and u is cross(r, f); drawFrame's projection, multiply(perspective(FovY, aspect, NearZ, FarZ), lookAt(Eye, Target, +Y)) from render/math.h, maps every point of that ray to the cursor's position. The point is where the ray meets y = 0. There is none when the eye is not above the ground or the ray does not descend.
```

and at the end of the Park mesh section, add a section:

```markdown
## Ghosts

Ghosts and highlights are ParkMeshes built with the functions that build the park's, so a ghost is drawn by the code that draws the committed result. They are translucent: GHOST_ALPHA is 0.5, and INVALID_TINT, a red, DELETE_TINT, an orange, and HIGHLIGHT_TINT, a white, differ in red, green, and blue from each other and from the path and box kinds' colors, each with alpha above 0 and below 1.

appendPath given a color draws the ribbon appendPath draws, in that color; without one it uses pathColor(kind). appendEntity adds the box or path a key holds, as buildParkMesh draws it but in a given color: a box through appendBox with its kind's boxSize and boxHeight, and a path through appendPath of its kind and points. It adds nothing when the key holds neither.

buildGhostMesh gives an edit's ghost on a world. An edit's ghost color for a color c is c with alpha GHOST_ALPHA when isAccepted(world, edit), and INVALID_TINT when not. An AddBox's ghost is appendBox at its pose with its kind's boxSize and boxHeight, in the ghost color of boxColor(kind). A MoveBox's is the same at its pose with the kind of the box its key holds, and nothing when its key holds no box. An AddPath's is appendPath of its kind and points in the ghost color of pathColor(kind). An accepted DeletePath or DeleteBox's is appendEntity of its key in DELETE_TINT, and a refused one's is nothing. So the ghost of an accepted AddBox or MoveBox has the positions and normals of the box buildParkMesh draws in the world the edit gives.
```

src/app/SPEC.md: after the Park section, add:

```markdown
## Tools

The Tools panel selects the tool (tools/SPEC.md): Look, which is ToolKind None and the tool at start, Place shop, Place depot, Move box, and Delete. Each frame, the left button's press and release reach the tool before the frame's ticks, so they act on the world and pointer the ghost on screen was built from: a press only when ImGui does not want the mouse, and a release always. Then, after the camera moves, the app gives the tool the ground position under the cursor, from groundAtCursor with the cursor's position in the window, or none while ImGui wants the mouse. An edit a release gives is queued with queueEdit, and the ticks step the world with that queue, so it applies at the next tick. Whenever the tool's edit or highlight differs from the one last drawn, or the park mesh was rebuilt, the app builds the ghost mesh, buildGhostMesh's for the edit followed by appendEntity of the highlighted entity in HIGHLIGHT_TINT, and gives it to the renderer with setGhostMesh.
```

## Files affected

- Modify: src/sim/park/SPEC.md, src/sim/park/edits.h, src/sim/park/edits.cpp
- Create: src/tools/SPEC.md, src/tools/tools.h, src/tools/tools.cpp, src/tools/CMakeLists.txt
- Modify: CMakeLists.txt
- Modify: src/render/SPEC.md, src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/renderer.h, src/render/renderer.cpp, src/render/shaders/park.vert, src/render/CMakeLists.txt
- Create: src/render/picking.h, src/render/picking.cpp
- Modify: src/app/SPEC.md, src/app/main.cpp, src/app/CMakeLists.txt
- Create: src/app/tool_panel.h, src/app/tool_panel.cpp
- Modify: plans/effortless-building/sketch-a-park/MILESTONE.md, adding a deepening candidate and a research note
- Create, by the test pass: tests/tools/ test files and CMakeLists.txt, tests/render/ test files, and a ParkEdit test under tests/sim/park/
- Modify, by the test pass: tests/CMakeLists.txt, tests/render/CMakeLists.txt, tests/sim/CMakeLists.txt

## Dependencies

park-intent: the intent queries, groundLine, footprintOf, boxSize, and pathWidth. park-edits: the five commands and isAccepted. park-view: appendBox, appendPath, buildParkMesh, the park pipeline, and the app's mesh rebuild. deterministic-simulation: CommandQueue, stepWorld with commands, and makeCandidate.

## Out of scope

- Drawing a path, its ghost's use, and snapping: path-tool. buildGhostMesh already draws an AddPath's ghost.
- Turning a placed box, in place or while moving it: a box keeps its facing until deleted and placed again. A deepening candidate in MILESTONE.md.
- Cancelling a hold with a key, and keyboard shortcuts for tools: a deepening candidate in MILESTONE.md.
- A ghost that shows why it is invalid: a deepening candidate in MILESTONE.md.
- Judging a ghost against commands queued but not yet applied: a second commit within one tick is judged against the world before the first, and the simulation still refuses it if it conflicts.
- Picking the top of a box rather than the ground under the cursor: tools work on ground positions.
- Deleting or moving the entrance, which no command does.
- Automated checks of the translucent pass's GPU state, setGhostMesh, and the app's wiring of the tools, which need a window: criterion 10 checks them by hand.

## Open questions

None.
