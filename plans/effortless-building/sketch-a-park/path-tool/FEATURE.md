# Feature: Path Tool

## Summary

The player draws a guest or backstage path by clicking points on the ground. The tools library gains two path tools. They keep the points drawn so far. Each click adds the cursor's point, snapped onto the nearest ground line of a same-kind path within SNAP_REACH, 2 m. While drawing, the tentative edit is an AddPath through the drawn points and the snapped cursor, so the ghost curve follows the cursor, snaps where the commit will, and turns red where the path would meet a box or run past the park's edge. A point clicked off the park's square gives an empty ground line, so the ghost shows nothing until the drawing is cancelled. Clicking the last point again finishes, so a double-click works, and the release commits exactly the AddPath the ghost showed. Choosing a tool, or the Tools panel's Cancel path button, drops the drawing.

## Acceptance criteria

1. snapToPath gives the point itself when no ground line of a path of the given kind comes within SNAP_REACH of it. Otherwise it gives a point on a segment of such a ground line within SNAP_REACH of it, and no point on any segment of any such ground line is nearer. Paths of the other kind never change it.
2. A path tool's press with a ground position, unless it finishes, appends snapToPath of the pointer for the tool's kind to the tool's drawn points and takes no hold.
3. A path tool that is not holding shows an AddPath of its kind through its drawn points followed by the snapped pointer when it has drawn points, the pointer has a ground position, and the snapped pointer lies farther than FINISH_REACH from the last drawn point. Otherwise it shows the AddPath through the drawn points alone. Either is none when it would hold fewer than two points.
4. A press with a ground position whose snapped pointer lies within FINISH_REACH of the last drawn point takes hold. While the tool holds, its edit is the AddPath through exactly the drawn points, whatever the pointer does, or none with fewer than two. After the release the tool has no drawn points.
5. selectTool, to another kind or the same one, leaves the tool with no drawn points, so no later release gives an edit holding the points drawn before it.
6. For the path tools, as for every tool, a release gives exactly the tentativeEdit of the moment before it when the press before it took hold, and none otherwise, whether or not isAccepted is true for that edit (principles 5 and 8).
7. For an accepted AddPath on a resolved world, the ghost's vertex positions and normals equal, in order, those buildParkMesh draws for the path it adds in makeCandidate of the world with the edit queued (principle 8).
8. In the running app, the Tools panel selects Guest path and Backstage path. Clicking draws a path whose translucent ghost follows the cursor, snaps onto a path of the same kind within reach, and turns red where it meets a box. Clicking the last point again, or double-clicking, commits the path the ghost showed. Cancel path, or choosing another tool, drops the drawing. Checked by hand (manual).

## Medium

None. The path tools read intent, which sits outside principle 3 (decision 0025), through parkPaths and groundLine, and give AddPath commands the app queues. The feature samples, emits, draws, and supplies nothing.

## Principle checks

- Principle 1: a committed path holds only the points the player clicked, snapped, and nothing sampled from its curve (criteria 2 and 4). The drawing lives in the ToolState, never in the world.
- Principle 5: the path tools refuse nothing. A finished path that meets a box or leaves the park is still given as the edit (criterion 6), and the simulation's physical check refuses it.
- Principle 6: tpj_tools reads paths only through parkPaths and groundLine.
- Principle 8: a release commits exactly the AddPath the ghost showed (criterion 6), and an accepted path's ghost has the committed ribbon's geometry (criterion 7).
- Principle 10: driving the path tools through presses, releases, pointer moves, and selectTool leaves the world's save and hash unchanged, since the tools take the world as const.

## Spec changes

src/tools/SPEC.md, in "## Input", the first two sentences become:

    A ToolState holds one tool, of a ToolKind: None, GuestPath, BackstagePath, PlaceShop, PlaceDepot, MoveBox, or Delete. Its fields are public for reading, but only four calls change them. selectTool sets the kind and drops any hold and any drawn points without committing, keeping the pointer and the place tools' facing.

src/tools/SPEC.md, at the end of "## Picking", a new paragraph:

    snapToPath gives, for a ground point and a path kind, the nearest point to it on the ground line of a path of that kind when one lies within SNAP_REACH, 2 m, its distance at most that, and the point itself otherwise. The nearest point to P on the segment from A to B is A + (B - A) * t, with t the projection ((P - A) · (B - A)) / ((B - A) · (B - A)) clamped to between 0 and 1, and its distance is sqrt(dx * dx + dz * dz) of it minus P. Paths are taken in parkPaths' order and segments in order along the line, and a point replaces the nearest so far only when strictly nearer. Paths of the other kind are never snapped onto.

src/tools/SPEC.md, in "## Tools", after the paragraph "None gives no edit...", a new paragraph:

    GuestPath and BackstagePath draw a path of their kind. The tool keeps Drawn, the points drawn so far, empty at first. Its next point is snapToPath of the pointer for its kind, when the pointer has a ground position and, if Drawn is not empty, that snapped point is farther than FINISH_REACH, 1 m, from Drawn's last point: dx * dx + dz * dz is above FINISH_REACH squared. A press with a ground position appends the next point to Drawn when there is one, and otherwise, the snapped point lying within FINISH_REACH of the last point, finishes: it takes hold. A press with no ground position does nothing. Not holding, its edit is an AddPath of its kind through Drawn followed by the next point when Drawn is not empty and there is one, and through Drawn alone otherwise. Holding, its edit is the AddPath through Drawn alone. Either is none when it would hold fewer than two points. A release while it holds empties Drawn, so finishing on a lone first point commits nothing and ends the drawing. A point outside the park's square gives the AddPath an empty ground line, so its ghost is empty. It highlights nothing. Every point it appends lies at least FINISH_REACH from the one before, so the path it commits keeps all of them.

src/render/SPEC.md, in "## Ghosts", after the sentence ending "the box buildParkMesh draws in the world the edit gives.", add:

    Likewise, the ghost of an accepted AddPath has the positions and normals of the ribbon buildParkMesh draws for the path it adds, since a ribbon depends only on its kind and its ground line, and the ground line of the points the path keeps is that of the edit's points.

src/app/SPEC.md, in "## Tools", the first sentence becomes:

    The Tools panel selects the tool (tools/SPEC.md): Look, which is ToolKind None and the tool at start, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete. While the tool has drawn points, the panel says to click the last point again to finish, and a Cancel path button selects the same tool again, which drops them.

## Files affected

- Modify: `src/tools/SPEC.md`
- Modify: `src/tools/tools.h`
- Modify: `src/tools/tools.cpp`
- Modify: `src/render/SPEC.md`
- Modify: `src/app/SPEC.md`
- Modify: `src/app/tool_panel.h`
- Modify: `src/app/tool_panel.cpp`
- Modify: `src/app/main.cpp`
- Modify: `plans/effortless-building/sketch-a-park/MILESTONE.md`
- Tests from the test pass, under `tests/tools/` and `tests/render/`.

## Dependencies

- box-tools (merged): ToolState, the press and release contract, buildGhostMesh's AddPath ghost, the translucent pass, and the Tools panel.
- park-intent and park-edits: parkPaths, groundLine, pathWidth, AddPath, and isAccepted.

## Out of scope

- Keyboard shortcuts to finish or cancel, such as Enter and Escape, which the milestone keeps as a deepening candidate.
- Removing the last drawn point while drawing, and a visible invalid ghost for a drawing with a point off the park's square.
- Preferring another path's end over its side when snapping, a milestone deepening candidate.
- A mark at the snapped cursor before the first click. The first point's snap shows once the ghost has two points.
- Closing a path into a loop by finishing on its first point. Paths are open curves.
- Snapping onto boxes, the entrance, or paths of the other kind.
- Automated checks of the Tools panel and the app's wiring, as in box-tools.
- Automated checks of snapToPath's tie-breaking, which decides only between exactly equal distances, and of principle 6, which the private header check covers.

## Open questions

None.
