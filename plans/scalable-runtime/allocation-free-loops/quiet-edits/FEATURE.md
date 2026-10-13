# Feature: Quiet Edits

## Summary

quiet-edits makes the app's work at a player action refill the memory it keeps, and puts that work in the allocation check. Today, when an edit, an undo, or a redo applies, the app builds the park's intent records and the history's change as new lists. It fills the tool park from new lists, deriving every segment's rounded ground line four times. It builds a new park mesh, and a new overlay shape while an overlay is shown. It reads the park's points and segments into new lists for the look, deriving the ground twice more, and it frees the kept preview's lists. Taking an undo or redo builds a new command and drops the taken edit's memory. A click's pick reads the park and the guests into new lists, and looking up the picked subject makes new buffers. A path's release returns its edit with a new list of stops. Showing or hiding an overlay builds a new shape or frees the old one. Each of these breaks docs/conventions.md's Memory in loops rule.

After this feature, each of those refills lists the app keeps. The sim's intent readers that derive the ground gain forms that fill a list the caller keeps, keeping their working lists in TickBuffers. The look reads the tool park's lists instead of its own. The history reuses the taken edit's memory for the change an undo or redo makes. The allocation check undoes and redoes a guest path on full.park and watches the work after each, pausing while the session applies the command. What the player sees doesn't change. This covers criterion 11 of the milestone.

## Acceptance criteria

1. After its steady ticks, the allocation check adds a guest path in open ground on full.park. It then runs 4 rounds to warm up and 20 watched rounds, each an undo or a redo of that path, in turn. The watch pauses while the session takes and applies the command. In each round it watches: the history taking the undo or redo and noting its change, on a second history that follows the same world; the tool park filled again; the scene's sync with the park mesh, the guests, the emptied preview, the overlay's shape shown, hidden, and shown again, the look's marks, and their uploads and drawing; a click's pick, of a shop in one round and a bench in the next; and each tool's press and release. Freed blocks are reported as `the history`, `the tool park`, `the scene's sync`, `the picking`, `the presses`, `the uploads`, or `the drawing`. If nothing is freed, the check prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing, and 20 undos and redos with the app's work after each, freed no block they allocated` and exits with status 0. (check: at the start of noteEdits in src/tools/history.cpp, add `const IntentRecords probe = history.Seen;`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that say a block `the history` allocated was freed; then revert)
2. When an edit, an undo, or a redo applies, the app's own work frees no block it allocated once its lists have grown. That covers filling the tool park again, building the park mesh, emptying the kept preview, building the look's marks, and building the overlay's shape while an overlay is shown. Taking an undo or redo and noting its change in the history free no block they allocated either. A plain edit adds a new record to the undo list and empties the redo list, which stays outside this. Applying the command resolves the world and stays outside this. (ctest)
3. Showing and hiding an overlay frees no block it allocated, once the overlay's lists have grown. (ctest)
4. A click's pick, with the Look tool, frees no block it allocated, whether it picks a guest, a shop, a bench, or a toilet block. (ctest)
5. Each tool's press and release frees no block it allocated, once the tool's lists have grown. A press with a path tool after its first stop asks the world whether it accepts the path, which resolves, and stays outside this. (ctest)
6. The app shows, picks, and edits as it did before. (check: capture full.park paused after 300 ticks with the food overlay, before and after the feature, and see the two files are identical, commands in PLAN.md tasks 1 and 20; play, in the windows-release app on tests/parks/stress/full.park: draw a guest path and see it, its paving, its walkway, and its overlay shading appear; undo and redo it; move a shop and see its starved mark follow its supply; pick a guest, a shop, and a bench with Look; drag a point, a corner's radius handle, and a side with Edit path; show the Food overlay and hide it)
7. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md tasks 1 and 20)
8. docs/conventions.md says that work at a player action follows the Memory in loops rule. docs/testing.md and decision 0041 say the check runs the app's work after undos and redos, a click's pick, and each tool's press and release, and that applying a command stays outside it. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: read the three files; the three commands in PLAN.md task 20)

## Tests

None. The allocation check covers the work, and criterion 1's check proves the rounds see the history's allocations.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows.

## Spec changes

src/sim/park/SPEC.md:

- In the paragraph that starts "Other modules read intent only through", after "parkSegmentKeys puts its keys in such a list, without deriving the segments' lines.", add "parkPathPoints can also put its values in such a list. So can parkSegments, parkCorners, parkEdges, and parkTapers, keeping the lists that deriving the segments' lines works with in TickBuffers the caller keeps. Once the lists have grown, they allocate nothing. segmentRecords puts each segment's SegmentRecord in such a list: what parkSegments gives for it, without its line. It doesn't derive the lines, and allocates nothing once the list has grown."
- After "segmentLine gives the straight ground line between any two positions.", add "It can also put the line in a list the caller keeps, and then allocates nothing once the list has grown."

src/sim/routes/SPEC.md:

- After "A ParkDoor has its entity, its face, its position, the unit direction out of its face, and the width of its face.", add "parkDoors can also put the doors in a list the caller keeps, clearing it first, keeping the lists it works with in TickBuffers the caller keeps. It then allocates nothing once they have grown."

src/sim/operations/SPEC.md:

- After "nearestDepot returns the depot with the shortest backstage route, or none.", add "It can also keep the lists it works with in TickBuffers the caller keeps, and then allocates nothing once they have grown."

src/sim/furniture/SPEC.md:

- After "parkFurniture gives every piece, in key order.", add "It can also put them in a list the caller keeps, clearing it first, and then allocates nothing once the list has grown."

src/render/SPEC.md:

- At the end of the paragraph that starts "buildParkMesh gives a world's park as the player sees it.", add "buildParkMesh fills a mesh the caller keeps, with the lists it works with. Once they have grown, it allocates nothing."
- Replace "buildOverlayShape(world) gives the guest paving's overlay shape:" with "buildOverlayShape fills the guest paving's overlay shape in lists the caller keeps:", and at the end of that paragraph add "Once the lists have grown, building it allocates nothing."
- At the end of the paragraph that starts "entityAtCursor gives the key of the nearest entrance, box, or guest", add "It reads the park and the guests into lists the caller keeps, and allocates nothing once they have grown."

src/legible/SPEC.md:

- Replace "If a caller replaces the world, or changes it without a tick, as edits applied while paused change it, the caller must empty the kept preview." with "If a caller replaces the world, or changes it without a tick, as edits applied while paused change it, the caller must clear the kept preview's tick, so the next keepPreview makes it again. Clearing the tick keeps the preview's lists."
- At the end of the paragraph that starts "An inspector explains one guest, shop, bench, or toilet block", add "pickSubject can also keep the lists it works with in TickBuffers the caller keeps, and then allocates nothing once they have grown."

src/tools/SPEC.md:

- In the first paragraph, after "keepToolPark fills it from the world's intent, and the app fills it again whenever an edit, an undo, or a redo applies, or a park is opened.", add "Once its lists have grown, filling it again frees no memory it allocated." After "The app drives it with four calls: selectTool, movePointer, pressPointer, and releasePointer.", add "releasePointer puts its edit in an edit the caller keeps. That edit keeps a path's stops while it holds another edit, so switching between a path and another edit frees no memory."
- After the paragraph that starts "undoEdit takes the last edit off the undo list", add the paragraph "takeUndo and takeRedo do what undoEdit and redoEdit do, but give the command the history keeps, until its next undo or redo, instead of a copy. Where those give none, they give null. Once the history's lists have grown, taking an undo or redo and noting its change free no memory they allocated. Noting any other edit adds a new change to the undo list and empties the redo list."

src/app/SPEC.md:

- In the paragraph that starts "The park's walkways and shop marks are rebuilt", after "The allocation check holds them to it too.", add "When an edit, an undo, or a redo applies, the app's own work frees no memory it allocated, once its lists have grown. That covers filling the tool park again, building the park mesh, emptying the kept preview, building the look's marks, and building the overlay's shape while an overlay is shown. Taking an undo or redo, noting its change in the history, showing or hiding an overlay, and a click's pick free no memory they allocated either. A plain edit adds a new record to the history's undo list and empties its redo list. Applying the command resolves the world, which stays outside this. The allocation check holds this work to it over undos and redos of a guest path on full.park."
- In the paragraph that starts "The Tools panel selects the tool.", after "The allocation check holds every tool on full.park to it, resting on paths, paving, boxes, and handles, and holding a drawn path, a dragged shop, and a dragged point.", add "Each tool's press and release frees no memory it allocated either, once the tool's lists have grown. The exception is a path tool's press after its first stop, which asks the world whether it accepts the path. The allocation check presses and releases each tool after each undo and redo."

docs/conventions.md, docs/testing.md, and docs/decisions/0041-allocation-check.md: see PLAN.md task 3.

## Files affected

- Modify: `src/sim/tick_buffers.h`
- Modify: `src/sim/park/geometry.h`, `src/sim/park/geometry.cpp`, `src/sim/park/internal/layout.h`, `src/sim/park/layout.cpp`, `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/park/surface.h`, `src/sim/park/surface.cpp`, `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/SPEC.md`
- Modify: `src/sim/routes/networks.h`, `src/sim/routes/networks.cpp`, `src/sim/routes/SPEC.md`
- Modify: `src/sim/operations/operations.h`, `src/sim/operations/operations.cpp`, `src/sim/operations/SPEC.md`
- Modify: `src/sim/furniture/furniture.h`, `src/sim/furniture/furniture.cpp`, `src/sim/furniture/SPEC.md`
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`, `src/render/paving_overlay.h`, `src/render/paving_overlay.cpp`, `src/render/picking.h`, `src/render/picking.cpp`, `src/render/SPEC.md`
- Modify: `src/legible/inspect.h`, `src/legible/inspect.cpp`, `src/legible/preview.h`, `src/legible/SPEC.md`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/internal/tool_lists.h`, `src/tools/history.h`, `src/tools/history.cpp`, `src/tools/SPEC.md`
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/input/cursor.h`, `src/app/input/cursor.cpp`, `src/app/session/park_session.h`, `src/app/session/park_session.cpp`, `src/app/application.cpp`, `src/app/SPEC.md`
- Modify: `src/bench/stages.cpp`
- Modify: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`, `tests/checks/allocations/allocation_check.cpp`, `tests/checks/allocations/CMakeLists.txt`
- Modify: `docs/conventions.md`, `docs/testing.md`, `docs/decisions/0041-allocation-check.md`
- Modify: `plans/scalable-runtime/allocation-free-loops/MILESTONE.md`

## Dependencies

- quiet-tick: the allocation check and TickBuffers. Merged.
- quiet-scene, kept-preview, quiet-tools, quiet-panels, and quiet-uploads: the scene's sync, the kept preview, the tools' kept lists, the Inspector's kept lists, and the renderer's kept buffers the rounds run. Merged.

## Out of scope

- Applying a command, which resolves the world, and a path tool's press after its first stop, which asks the world whether it accepts the path. changing-previews owns resolution.
- Queueing a command. The CommandQueue holds each command in a std::any, so a command larger than a pointer is put on the heap, and applying it frees it. The command is the message the action sends to the world, made once for each action.
- A plain edit's new change on the undo list, and the redo list it empties. The change is the record the history keeps, and the emptied edits can't be redone.
- Opening a park, which replaces the world and empties the history, the scene, and the interaction.
- The ghost of an edit, which is built again when the preview is made again. changing-previews owns it.

## Open questions

None.
