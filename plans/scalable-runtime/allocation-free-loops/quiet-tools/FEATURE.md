# Feature: Quiet Tools

## Summary

quiet-tools makes each tool's per-frame work reuse its memory, and adds it to the allocation check. That work is moving the pointer, with its snapping or frontage search, the tentative edit, the highlight, a path tool's start and finish marks, Edit path's handles and hovered handle, the snap's guides, and the measured lengths. Today almost every piece of it derives the park's segments, corners, and edges from intent again, several times a frame, and makes new lists of its own.

After this feature, the app keeps a ToolPark: the park's path points, segments, corners, edges, tapers, boxes, and doors, as the tools read them, with the lists the tools reuse. The app fills it again only when an edit, an undo, or a redo applies, or a park is opened. The tools read the park through it and keep their working lists in it. The snap in effect holds its guides and holds in fixed arrays. The app keeps the tentative edit, the frame's look, and the measured lengths, and the tools fill them in place. SceneSync builds the look's marks from the park's boxes, points, and segments, which it keeps and fills again only when an edit applies, so a look that changes as the pointer moves frees nothing either. sim/park gains a form of stopPositions that appends one stop's position and reads the corners from a list the caller gives. What the player sees and does doesn't change. This covers criterion 6 of the milestone.

## Acceptance criteria

1. The allocation check runs each tool's frame work as the app does, with the pointer held at fixed places on full.park, at every warm-up and watched tick. Before the warm-up it adds a guest path in open ground with a rounded corner and a taper, since full.park has none. It holds twenty-three tools: Look; Guest path with nothing drawn, on a point and on the curve's arc, with one stop drawn, and with two stops drawn and the pointer on the last; Backstage path; each of the four place tools beside paving of the kind it fronts; Move box over a shop and holding a dragged shop; Edit path over a point, an edge handle, the curve's radius and taper handles, a segment's side, and a segment's middle, and dragging a point; Remove point over a point; Delete over a shop and over a segment; and Furnish over a guest segment. At every tick it also builds the look's marks again for a look that alternates, as a pointer moving between a shop and a segment, and between two segments' sides, makes it. Freed blocks are reported as `the tools`. If nothing is freed, the check prints `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, and the tools freed no block they allocated` and exits with status 0. (check: in Interaction::movePointer, in src/app/input/interaction.cpp, add `const std::vector<uint64_t> held{world.Tick};` and `static_cast<void>(held);`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that start `allocation check: the tools freed`; then revert)
2. With the pointer held at fixed places, a frame's tool work frees no block it allocated over the check's watched ticks of full.park, for each tool. Building the look's marks again when the look changes frees none either. (ctest)
3. The tools behave as before. (play, in the windows-release app on tests/parks/stress/full.park: with Guest path, rest the pointer on a path point, then on a segment, and see the start mark and guides; click twice in open ground, rest the pointer straight on from the last stop, and see the guide, the length, and the ghost; rest it on the last stop and see the finish mark. With Place shop and Place depot, rest the pointer beside guest and backstage paving and see the box front the paving. With Move box, drag a shop. With Edit path, see the point markers and edge handles, rest on a handle, a point, a side, and a segment's middle and see each highlighted or the inserted point's ghost, and drag a point and see it snap. With Remove point, Delete, and Furnish, rest the pointer on a point, a shop, a segment, and a guest path and see each marked and ghosted. Sweep the pointer across shops and segments with Move box and Delete, and see the highlight follow it.)
4. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md task 17)
5. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 19)

## Tests

None. The allocation check covers the work, and criterion 1's check proves it sees it. tests/integration/frontage_test.cpp keeps testing frontagePose through its returning form.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. The tools read intent, the resolved surface, and the walked surface as they do today.

## Spec changes

src/tools/SPEC.md:

- In Boundary, after "The app drives it with four calls: selectTool, movePointer, pressPointer, and releasePointer.", add "movePointer, pressPointer, releasePointer, tentativeEdit, highlightedEntity, the picking queries, frontagePose, editHandles, hoveredHandle, measuredLengths, pathStart, and pathFinish read the park's path points, segments, corners, edges, tapers, boxes, and doors through a ToolPark, which the app keeps. keepToolPark fills it from the world's intent, and the app fills it again whenever an edit, an undo, or a redo applies, or a park is opened. A ToolPark also holds the lists the tools reuse from call to call."
- Replace "The app reads tentativeEdit, the edit the ghost shows," with "The app reads tentativeEdit, the edit the ghost shows, which can also fill an edit the caller keeps,".
- Replace "editHandles and hoveredHandle give the handles Edit path shows. snapGuides and measuredLengths give what the app draws for snapping." with "editHandles puts the handles Edit path shows in a list the caller keeps, and hoveredHandle gives the one under the pointer. snapGuides gives the guides of the snap in effect, and measuredLengths puts the lengths the app draws in a list the caller keeps."
- At the end of Boundary, add "Once a ToolPark's lists and the caller's lists have grown, a frame's tool work frees no memory it allocated while the pointer rests. That covers movePointer with its snap or frontage search, tentativeEdit filling the caller's edit, highlightedEntity, pathStart, pathFinish, editHandles, hoveredHandle, snapGuides, and measuredLengths. The allocation check holds every tool on full.park to it (decision 0041)."

src/render/SPEC.md:

- After "It adds nothing when the key holds none of these.", add "appendEntity can also read the boxes and path points from lists the caller keeps, as parkBoxes and parkPathPoints give them. Then it allocates nothing once the mesh has grown."
- After "It adds nothing when the key holds no segment with a ground line.", add "appendSides can also find the segment the key holds in a list the caller keeps, as parkSegments gives it, and then allocates nothing once the mesh has grown."

src/sim/park/SPEC.md:

- In the list of public geometry, replace "stopPosition, stopPositions, pathCrossings," with "stopPosition, stopPositions, appendStopPosition, pathCrossings,".
- After "stopPositions gives each stop's ground position, and stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.", add "appendStopPosition appends one stop's position to a list the caller keeps, which holds the positions of the path's earlier stops, as stopPositions would give it. If stopPositions would give none, it appends nothing. It reads the park's corners from a list the caller gives, as parkCorners gives them, and allocates nothing once the list has grown."

src/app/SPEC.md:

- At the end of the paragraph that starts "The Tools panel selects the tool.", add "Once its lists have grown, a frame's tool work frees no memory it allocated while the pointer rests (tools/SPEC.md). That includes putting the tool's highlight, path marks, and handles in the frame's look, and building the ghost's marks again when the look changes, as when the pointer moves onto another box, point, handle, or side. The allocation check holds every tool on full.park to it, resting on paths, paving, boxes, and handles, and holding a drawn path, a dragged shop, and a dragged point."

## Files affected

- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/park/SPEC.md`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/snapping.cpp`, `src/tools/frontage.cpp`, `src/tools/internal/snapping.h`, `src/tools/SPEC.md`
- Create: `src/tools/internal/frontage.h`, `src/tools/internal/tool_lists.h`
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`, `src/render/SPEC.md`
- Modify: `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/application.h`, `src/app/application.cpp`, `src/app/ui/tooling_ui.cpp`, `src/app/ui/snap_guides.h`, `src/app/ui/snap_guides.cpp`, `src/app/SPEC.md`
- Modify: `tests/checks/allocations/allocation_check.cpp`

## Dependencies

- quiet-tick: the allocation check. Merged.
- kept-preview: the check's held previews and the look SceneSync keeps. Merged.

## Out of scope

- Moving the pointer in the check. It holds the pointer still, as the milestone's criterion 6 says. The tools' own work and the look's marks allocate nothing while it moves too, once their lists have grown. But a path tool's edit changes as the pointer moves, so the preview resolves a new world, which changing-previews takes.
- Presses and releases, which aren't steady play. They read the ToolPark too, but a press may still ask the world whether it accepts a path, which allocates.
- Drawing the snap guides and their length labels, which is ImGui drawing.
- Rounded corners and tapers in full.park itself. Changing the park would change the milestone's runtime comparison, so the check adds its own curve.

## Open questions

None.
