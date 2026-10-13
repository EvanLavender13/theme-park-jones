# Research: quiet-edits

## What the app does at a player action, and what of it allocates

The question is which work runs when the player acts, and which of it throws away memory it could refill. The findings come from reading src/app/application.cpp's frame and every call it makes when the edit count changes, when a click picks, when a button goes down or up, and when an overlay is shown or hidden.

After an edit, an undo, or a redo applies, the frame runs this work. Applying the command itself resolves the world, and changing-previews owns that.

- ParkSession::step and applyWithoutTick call noteEdits. noteEdits builds the world's intent records as a new IntentRecords, builds the change as a new IntentChange, and moves the new records over the ones it saw last, which frees the old ones. recordsOf reads the segments through parkSegments, which derives every segment's rounded ground line only to copy its key, kind, ends, width, edge radii, and furnishing.
- An undo or redo: undoEdit and redoEdit move an edit off a list into Taken, which frees the edit Taken held, and build the reversing command as a new RestoreIntent over Sent. The session copies the command into its own optional, queues it on a local CommandQueue, and resets the optional after applying it. noteEdits then builds a new change for the opposite list, though the taken edit's memory is about to be dropped.
- Interaction::follow calls keepToolPark. It assigns new vectors from parkPathPoints, parkSegments, parkCorners, parkEdges, parkTapers, and parkDoors. parkSegments, parkCorners, parkEdges, and parkTapers each derive the ground again from layoutOf and groundOf, and every list inside groundOf is new: the straight lines, the arc points, the segment ends, the bends, the paving reaches, the arms, the roundings, and each segment's line. parkEdges and parkTapers also make new shapes, ends, fitting scratch, and found lists in surface.cpp's edgesOf and tapersAt. parkDoors makes new entrance and box lists and a new door list. parkBoxes and the frontage's edge steps already refill.
- SceneSync::syncWorld empties the kept preview with `Kept = {}`, which frees the footfall sample's lists.
- uploadWorldMeshes calls buildParkMesh, which returns a new ParkMesh. Inside it, parkEntrances, parkBoxes, and parkFurniture return new lists, and appendStarvedMarks calls parkBoxes again and nearestDepot for each shop. nearestDepot reads route lengths and the depots' keys into new lists.
- SceneSync::syncLook reads the park's points and segments into new lists with parkPathPoints and parkSegments, deriving the ground a fifth time. Its boxes already refill.
- SceneSync::syncOverlayShape, while an overlay is shown, calls buildOverlayShape, which returns a new OverlayShape. Inside it, parkSegments derives the ground a sixth time only to read each guest segment's key and ends. The guest segments' keys, their ends, the owners, and the overlap search's bounds, order, and pairs are new lists. Hiding the overlay assigns an empty shape, which frees the shape's lists.

At a click with the Look tool, entityUnderCursor calls entityAtCursor, which reads the entrances, boxes, and guests into new lists. pickSubject calls inspectorSubject, which calls the returning shopRecord, making new TickBuffers, and the returning parkBoxes for a bench or toilet block.

At a press, pressPointer allocates nothing for any tool except where a path tool asks the world whether it accepts the path with its next stop. That asks isAccepted, which resolves a candidate, and changing-previews owns it. A path tool's first stop and its finishing press don't ask. Edit path's press reads handles, corners, tapers, edges, points, sides, and inserts from the tool park. stopPosition of an OnPoint reads the point without deriving corners.

At a release, releasePointer returns the tentative edit by value. For a path, that is an AddPath with a new list of stops, freed once useButtons has queued its copy. Every other edit holds no list.

## What stays as it is

Queueing a command puts a copy of it in the CommandQueue's std::any, which holds any command larger than a pointer on the heap, and applying the command at the tick frees it. The command is the message the action sends to the world, made once for each action, as a save makes its file. Making the queue keep typed storage would be a change to how the simulation takes commands, not a refill of a list the app keeps.

A plain edit pushes its change onto the undo list and empties the redo list. The pushed change is the new record the history keeps. Emptying the redo list drops the edits it held, which can no longer be redone.

Opening a park replaces the world and empties the history, the scene, and the interaction's choices, as the milestone's Leftovers already say.

## Nested lists

ParkSegment holds its ground line as a vector. If a kept list of segments shrinks, as when an undo removes a segment, std::vector destroys the extra elements and frees their lines, and the check finds those frees. A kept list of segments therefore keeps its extra elements in a spare list and takes them back before making new ones. The ground's own list of lines does the same. Every other list the action fills holds flat values.

Rejected: keeping the vector at its largest size with a separate count — every reader of ToolPark.Segments iterates the vector, so each would need a span. Rejected: storing the ground in the world at each resolution — it would add derived state to every world copy and change the resolution, which changing-previews owns.

## Ground derived once or four times

keepToolPark derives the ground once for each of segments, corners, edges, and tapers. Deriving it once and sharing it would be faster, but speed is not this feature's aim. Each fill form keeps the same derivation its returning form runs, so the results stay the same by construction.

## How the check can watch an action

The allocation check repeats work and fails when a block is both allocated and freed during the repeats. Applying an undo or redo resolves the world, which allocates and frees by nature until changing-previews. The watch can be paused around the apply. While paused, it notes no allocation, and a noted block that is freed loses its note without a finding. That can hide a free, but never reports a false one.

ParkSession runs noteEdits inside step and applyWithoutTick, right after applying, so the check can't watch it there without watching the resolution. The check keeps a second EditHistory that follows the same world. It calls the same takeUndo, takeRedo, and noteEdits on it, watched, while the session applies its own undo or redo, unwatched. The session's own history runs the same functions on the same intent.

Undoing and redoing one guest path in open ground changes the path points, segments, paved surface, overlay shape, and park mesh, and leaves the places the check presses and picks unchanged. Redo adds the path back under fresh keys, so the history's renames grow by one entry per point and segment at each redo. They are kept, not freed.

Sources: src/app/application.cpp, src/app/session/park_session.cpp, src/app/input/interaction.cpp, src/app/scene/scene_sync.cpp, src/app/scene/scene_uploads.cpp, src/tools/tools.cpp, src/tools/history.cpp, src/render/park_mesh.cpp, src/render/paving_overlay.cpp, src/render/picking.cpp, src/legible/inspect.cpp, src/sim/park/intent.cpp, src/sim/park/layout.cpp, src/sim/park/surface.cpp, src/sim/routes/networks.cpp, src/sim/operations/operations.cpp, src/sim/command_queue.h, tests/checks/allocations/watch.cpp.
