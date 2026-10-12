# Research: quiet-scene

## What the scene's sync allocates in steady play

The app calls SceneSync::syncWorld once a frame, after the frame's ticks (src/app/application.cpp). Three parts of it allocate:

- Finding whether intent changed. Each call builds a DrawnIntent from parkEntrances, parkPathPoints, parkSegments, parkBoxes, and parkHours, compares it with the last one, and drops it. parkSegments runs layoutOf and segmentsOf, so every frame derives every segment's rounded line again and allocates each line, only to find nothing changed.
- The seated guests. SeatedGuests::update runs whenever the guests are redrawn, so at every tick. It makes its next list, its newcomers list, and parkGuests' key list, then moves the next list into place, which frees the old one.
- The guest mesh. uploadWorldMeshes in src/app/scene/scene_uploads.cpp calls buildGuestMesh at every tick, which returns a new ParkMesh. That file is compiled only into the tpj_app executable, so the check can't run it.

guestRecord, seatPlaceOf, seatPose, and appendBox allocate nothing. Uploading the mesh to the GPU is outside the check, as the milestone says.

## How the scene can find an intent change without deriving the paving

Intent changes only when a command applies (src/sim/SPEC.md, Commands), and in the app commands apply in one place. ParkSession::step and ParkSession::applyWithoutTick apply the queued commands, or a waiting undo or redo, and each then notes the edit in the history. A count that ParkSession raises at each of those places changes exactly when intent may have changed. ParkSession already gives the scene its generation, which lives outside the world for the same reason. Comparing one number costs nothing, so steady play does no work to find that nothing changed. If an edit applies but changes nothing, the scene rebuilds the park mesh once for nothing, which is harmless.

Rejected: fill forms of parkPathPoints and parkSegments — parkSegments derives every rounded line, which repeats work every frame. Comparing the walk's words for the intent components — exact and free of allocation, but it walks about 1,000 components of full.park every frame to find no change. A counter inside the world — it would be part of the world's value, or an exception to it.

## Where the guest mesh is built

SceneSync already decides when the guest mesh is rebuilt and keeps the seated guests it is drawn with. If SceneSync builds the guest mesh into a list it keeps, the build moves into tpj_app_core, and the check can run syncWorld exactly as the app does. uploadWorldMeshes then uploads the kept mesh. The park mesh stays in uploadWorldMeshes, because it is rebuilt only when intent changes, which steady play doesn't do.

Rejected: keeping the mesh in uploadWorldMeshes' caller in tpj_app — the check can't reach tpj_app. Instanced guests, drawing every guest from one mesh — that is a rendering change, and it stays in the capability's backlog, gated on the runtime report.
