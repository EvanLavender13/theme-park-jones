# Feature: Undo Redo

## Summary

The player can undo and redo every edit they commit. Ctrl+Z undoes, and Ctrl+Y or Ctrl+Shift+Z redoes. The Tools panel has Undo and Redo buttons too. Undo puts the park's intent back as it was before the last edit, and redo applies that edit again. Both apply at the next tick with no ghost (decision 0029). The history records each edit as the intent it changed: the path points, segments, and boxes it removed, added, and changed, before and after. It reads that change from the world after the edit's tick, so it catches every side effect, such as a radius that a join rewrites. A new sim/park command, RestoreIntent, removes, sets, and adds those records. Anything it adds takes a fresh key from the counter. The history then reads the new keys and renames the old ones wherever its other edits name them. While a path is half drawn or the button holds an edit, undo and redo drop it, as Cancel path does, and change nothing else. Starting or opening a park clears the history. The history is session state in the tools, and it is never saved.

## Acceptance criteria

1. In a new park, in the windows-release build, make one edit of each kind with the tools:
    - Draw a guest path that joins the entrance path and crosses itself, and a backstage path.
    - Place a shop and a depot, and move the shop.
    - With Edit path, drag a point onto another point, insert a point, round a corner, curve an edge, widen a segment, and taper a junction.
    - Remove a point with Remove point.
    - Delete a segment and a box with Delete.

    After each edit, Ctrl+Z puts back the paths, paving, boxes, and walkways as they were before it, and Ctrl+Y puts back the result of the edit. Then press Ctrl+Z until Undo is disabled. The park is the new park again. Press Ctrl+Shift+Z until Redo is disabled, and every edit comes back in order. Guests walking on paths that an undo or a redo splits or merges keep walking, and none jumps. (play)
2. Undo two edits, then make a new edit. Redo is disabled, and Ctrl+Y does nothing. Undo then reverses the new edit, and then the edit before the two undone ones. (play)
3. Press Ctrl+Z while a path tool has stops drawn. The drawn stops go, and no committed edit is undone. Press Ctrl+Z again, and the last edit is undone. Pressing Ctrl+Z while dragging a box with Move box drops the drag, and the box stays where it was. Undo and redo change the park at the next tick, with no ghost first. (play)
4. After edits, New park and Open park leave Undo and Redo disabled. (play)
5. A scratch check applies a run of edits covering every command to warm.park, as the app applies them: each edit in its own cycle, then each undo and redo alone in its cycle. After each undo and each redo, the park's intent equals the intent before or after that edit, apart from keys. Every key an undo or redo adds is at least the world's next key before its cycle. A new edit after undos empties the redo list. It prints each failure, then `failures: 0`. (check: `build/undo-check/check.exe | tail -1`)
6. The park world checks pass, with a chosen RestoreIntent edit among them. (listed test, ctest)
7. The simulation's existing commands are unchanged: `tpj_scenarios --compare` of this feature's output against main's exits with status 0. (scenarios comparison)

## Tests

The feature adds a command, so the milestone asks for a chosen edit of it in the park world checks. Add it to tests/sim/park_world_checks_test.cpp next to the other chosen edits, and list it last in chosenEdits. It starts from the world warm.park reaches after one cycle, as the other chosen edits do. The existing checks then hold for it: its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name it for what it makes the capabilities do.

- Putting back a guest segment guests stand on under a fresh key, as an undo of its deletion does. Setup: find the guest segment that deletingGuestSegment finds: one guests stand on, some heading to the shop. Change: a RestoreIntent that removes that segment, and adds a SegmentRecord with the same kind, From, To, width, and edge radii. Name the added record by the world's next key, which holds no entity. It sets nothing. Require that the world accepts it. Expected: the existing checks hold. The committed world holds an equal segment under a fresh key, and the guests on the old segment are carried onto it.

## Medium

The feature adds no fields or flows.

- Park intent: a new command, RestoreIntent, puts path points, segments, and boxes back as an undo or redo finds them. It changes intent only through the command queue, like every other command.
- Published intent: the history reads parkPathPoints, parkSegments, and parkBoxes after each tick that applied commands. It reads no ground lines or surface.
- Tools' session state: the EditHistory lives in tpj_tools, and the app's park session holds it. It is not intent, and it is never saved (principle 1). If undo restores something deleted, the restored thing gets a fresh key from the counter, so keys are never reused (src/sim/SPEC.md).
- Preview: undo and redo commit without a ghost (decision 0029). Every other edit keeps its ghost.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: replace "addParkEdits registers the twelve commands." with "addParkEdits registers the thirteen commands."
- Commands: replace "Twelve commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, RemovePoint, SetCornerRadius, SetSegmentWidth, SetEdgeRadius, and SetTaperLength." with "Thirteen commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, RemovePoint, SetCornerRadius, SetSegmentWidth, SetEdgeRadius, SetTaperLength, and RestoreIntent."
- Commands: after the paragraph that starts "SetCornerRadius names a path point and a radius.", add:

  RestoreIntent puts intent back as an undo or redo finds it. It names keys to remove, records to set, and records to add. A record is a path point's position, radius, and taper length, a segment's kind, two points, width, and edge radii (a SegmentRecord), or a box's kind and pose, each with a key. A record it sets names the path point, segment, or box whose values it replaces. A record it adds uses its key only as a name: a segment that names it as From or To ends at the point added under that name.

- Commands: in the paragraph that starts "On a valid world, a command is accepted exactly when", after the sentence for SetTaperLength, add: "For RestoreIntent, it means each key it removes holds a path point, segment, or box; each record it sets has the key of a path point, segment, or box of the record's type that it doesn't remove; each record it adds has a key that holds no entity and that no other record it adds has; each segment it sets or adds names two different points, each one it adds or a path point it doesn't remove; no segment it leaves in place ends at a point it removes; every number it holds is finite; every radius, edge radius, and taper length is at least 0, and every width is above 0; kinds are known; and every box it sets or adds has a footprint that stays in the park and conflicts with no other solid and no piece of the surface it leaves."
- Commands: replace "Every point a command adds has DEFAULT_TAPER_LENGTH." with "Every point a command other than RestoreIntent adds has DEFAULT_TAPER_LENGTH."
- Commands: in the list of what an accepted command leaves, after the SetTaperLength item, add: "- RestoreIntent destroys each path point, segment, and box it removes. It sets each record it sets on the path point, segment, or box with that key, which keeps its key. It adds each record it adds with a key from the counter, in the order it lists them: path points first, then segments, then boxes. No join, split, or curve rule applies, so it leaves exactly the records it gives."
- Commands: in the editedSegments paragraph, replace "and for edits of boxes." with "for edits of boxes, and for RestoreIntent, which shows no ghost." In the editedSurface paragraph, make the same change.
- Commands: replace "A ParkEdit holds any one of the twelve commands. It is the value a tool shows as a ghost and commits." with "A ParkEdit holds any one of the thirteen commands. It is the value a tool shows as a ghost and commits, or the RestoreIntent an undo or redo commits."

src/tools/SPEC.md:

- Boundary: after "snapGuides and measuredLengths give what the app draws for snapping.", add "An EditHistory holds the committed edits to undo and redo. clearHistory, noteEdits, undoEdit, and redoEdit change it, and canUndo and canRedo read it."
- Add a section at the end:

  ## History

  The history holds a list of edits to undo and a list of edits to redo. Each edit is the intent it changed: the path points, segments, and boxes it removed, those it added, and those it changed, before and after. The entrance never changes.

  clearHistory empties both lists and takes the world's intent as the intent the history saw last. noteEdits takes the world after a cycle that applied commands. It compares the world's intent with the intent the history saw last, then takes the world's intent as the intent it saw last. If nothing changed, it records nothing. Otherwise the change is one edit, even when several commands made it in one cycle. If the change came from an undo, the edit goes on the redo list. If it came from a redo, it goes on the undo list. Any other change goes on the undo list and empties the redo list.

  undoEdit takes the last edit off the undo list and gives the RestoreIntent that reverses it. The command removes what the edit added, sets what it changed back to how it was before, and adds back what it removed. redoEdit does the same for the last edit on the redo list, so the command reverses the undo. If there is no edit to take, each gives none. While a command that either gave hasn't been noted, both give none. If noteEdits then sees no change, as when the world refused the command, the edit goes back where it was. canUndo and canRedo give whether undoEdit and redoEdit would give a command.

  What an undo or redo adds back takes fresh keys. If its command applies alone in its cycle, noteEdits finds the key each restored record took. From then on, the history uses the new key wherever an edit names the old one.

src/app/SPEC.md:

- Tools and the ghost: after the paragraph that starts "The Tools panel selects the tool.", add:

  The Tools panel has Undo and Redo buttons, each disabled while there is nothing to undo or redo. Ctrl+Z undoes, and Ctrl+Y or Ctrl+Shift+Z redoes, unless a text field has the keyboard. While a path is being drawn or the button holds an edit, undo and redo drop it, as Cancel path does, and change nothing else. Otherwise they apply at the next tick, with no ghost (decision 0029), in a tick of their own, so an edit committed while one waits applies at the tick after. While an edit or another undo or redo waits for its tick, undo and redo do nothing.

- Park files: replace "They also drop any queued edits and the tool's drawn points, close the Inspector, and frame the camera on the new park." with "They also drop any queued edits and the tool's drawn points, empty the undo and redo lists, close the Inspector, and frame the camera on the new park."

## Files affected

- Create: `src/tools/history.h`, `src/tools/history.cpp`
- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/SPEC.md`
- Modify: `src/tools/CMakeLists.txt`, `src/tools/SPEC.md`
- Modify: `src/app/session/park_session.h`, `src/app/session/park_session.cpp`
- Modify: `src/app/input/input_map.h`, `src/app/input/input_map.cpp`, `src/app/input/platform_input.cpp`, `src/app/input/interaction.h`, `src/app/input/interaction.cpp`
- Modify: `src/app/ui/tool_panel.h`, `src/app/ui/tool_panel.cpp`, `src/app/ui/tooling_ui.h`, `src/app/ui/tooling_ui.cpp`
- Modify: `src/app/application.cpp`, `src/app/SPEC.md`
- Modify: `tests/sim/park_world_checks_test.cpp` (test pass)
- Modify: `plans/effortless-building/path-network/MILESTONE.md` (deepening candidates)

## Dependencies

Features 1 to 10, merged: every edit command the tools commit. carrier-widths, complete: carry-over keeps guests on ground that survives under a new key.

## Out of scope

- Removing only the last drawn stop on undo. It goes to the milestone's candidates.
- Showing the ghost of an undo's inverse while the control is hovered. It is already a milestone candidate.
- A limit on how many edits the history keeps.
- Keeping a restored shop's stock and orders. A restored box is a new box under a fresh key, and operations derives it as new.
- Undoing the camera, the tool choice, or a save.
- RestoreIntent among tpj_scenarios' random edits. Leaving them as they are keeps the scenarios' output unchanged, which criterion 7 relies on. It goes to the milestone's candidates.

## Open questions

None.
