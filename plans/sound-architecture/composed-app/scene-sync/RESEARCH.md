# Research: scene-sync

## How is the rebuild rule tested when the meshes go to the GPU?

main.cpp's updateParkMesh, updateGuestMesh, and updatePreviewMeshes each mix two things: deciding whether a mesh is stale, from what it was last built with, and building the mesh and handing it to the renderer, which needs a GPU device. Split along that line, the decision becomes a small class holding what was last built from (the intent, the tick, the look of the ghost and overlay, and the generation) that answers, each frame, which meshes to rebuild. It takes the world and plain values, so a test calls it with worlds it builds and steps. Building and uploading become stateless functions that take its answer, so all the state is in one place and none of it is reachable only through the renderer.

The kept preview belongs with the decision. It is a cache of the candidate world for the tool's edit, legible's KeptPreview, whose rule is that its owner empties it when the world is replaced. Holding it in the same class as the generation keeps that rule where the generation is compared.

Rejected: one class that decides and uploads — its state could be tested only with a renderer, which needs a window and a GPU. Recording, as now, separate locals in the loop — each is reset by hand when the world is replaced, the hazard 0027 names.

Sources: src/app/main.cpp updateParkMesh, updateGuestMesh, updatePreviewMeshes, and forgetOldWorld; src/legible/preview.h KeptPreview and keepPreview.

## Who owns the tool and the Inspector's subject?

The ToolState and the Inspector's subject are not derived from the world. They are the player's hold on it, written by the buttons, the Tools panel, a Look press, the Inspector's close button, and replacement. Today those writers are spread across useButtons, pickOnLookPress, drawPanels, buildUi, and forgetOldWorld, each taking the state by reference. One Interaction class owning both, with a member per writer, gives each write one home, and its follow of the generation replaces forgetOldWorld's two lines for them. Picking needs the entity under the cursor, a ray cast through the window's camera, so the class says whether the frame's press picks and takes the entity as a value, and the cursor work stays with the caller until frame-input moves it.

Rejected: the scene sync resetting the tool and the subject — it would write state it does not own, against 0027's one owner. Passing the entity under the cursor every frame — the ray cast runs only on a Look press today, and computing it every frame costs a pick per frame for nothing.

Sources: src/app/main.cpp useButtons, pickOnLookPress, drawPanels, buildUi, and forgetOldWorld; src/tools/tools.h; src/legible/inspect.h pickSubject.

## Does each owner comparing the generation keep one invalidation path?

The source of a replacement is the session alone, and its generation is the only signal. Each owner compares the generation it last saw with the session's and resets only its own state. No list of resets exists anywhere to fall out of date: a new cache added to the scene sync is reset because the scene sync compares the generation, and a new owner of world-dependent state compares it itself. The order main.cpp keeps today holds: the interaction follows the generation as soon as the request is used, before the buttons reach the tool, and the scene sync follows it after the ticks, before the preview is kept.

Rejected: a replaced event the session raises — each subscriber is registered by hand, and the order of calls becomes a hidden contract.

Sources: plans/sound-architecture/composed-app/RESEARCH.md — the generation as the one invalidation path; src/app/SPEC.md — the order of a frame.
