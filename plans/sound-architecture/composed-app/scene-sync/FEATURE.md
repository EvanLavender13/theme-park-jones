# Feature: Scene Sync

## Summary

scene-sync gives the scene's caches and the player's hold on the park each one owner that follows the park session's generation, so no reset on replacement is left in main.cpp. SceneSync, in app/scene_sync.h, keeps what the park, guest, ghost, and overlay meshes were last built from, and the kept preview, and decides each frame which meshes to rebuild. A world of a generation it has not seen rebuilds every mesh, frames the camera, and empties the kept preview. scene_uploads.h, its platform edge, builds those meshes and gives them to the renderer. Interaction, in app/interaction.h, owns the tool and the Inspector's subject: it gives the tool the frame's buttons and queues the edit a release commits, picks on a Look press, and forgets the subject when the Inspector closes, and a generation it has not seen drops the tool's hold and the subject. Both live in tpj_app_core and are tested without a window. The app's behavior is unchanged.

## Acceptance criteria

- SceneSync's syncWorld, for a generation other than the one it last saw, as at its first call, asks for the park mesh, the guest mesh, and the camera framed. For the generation it last saw, it asks for the park mesh exactly when the world's intent, as parkEntrances, parkPaths, and parkBoxes give it, differs from the last call's, and for the guest mesh exactly when the world's tick differs from the last call's, and never for the camera framed.
- A generation syncWorld has not seen empties the kept preview, so the next syncPreview makes the preview again, even for the tick and edit of the preview it kept.
- syncPreview keeps the preview as keepPreview does: it returns keepPreview's answer for the kept preview, the world, and the edit, and preview() is the kept preview's Made.
- syncLook returns true at its first call, when told the preview was made again, or when the look (the highlight, the Inspector's subject, and whether the food overlay shows) differs from the last call's, and false otherwise. A new generation does not make syncLook's next call a first call: it empties the kept preview, so the next syncPreview makes the preview again, and that remade flag rebuilds the ghost.
- Interaction, constructed with a generation, has the tool at start (ToolKind None, nothing drawn or held) and no subject. follow with that generation changes nothing. follow with another generation leaves the tool as selectTool of its own kind leaves it, which drops any hold and drawn points, and forgets the subject.
- useButtons gives the tool the press, as pressPointer does, and then the release, as releasePointer does, and queues the edit a release commits with queueEdit. With neither, it changes nothing.
- picks is true exactly when the frame's buttons hold a press and the tool is Look. pick sets the subject as pickSubject does for the world and the entity, and forgetSubject leaves no subject. selectTool, movePointer, tentativeEdit, and highlighted give what tools.h's functions of the same names give for the interaction's tool.
- The app's behavior is unchanged. Every existing test passes with its source unchanged, a --capture of a park with --graph and --overlay food shows the same scene, and main.cpp holds no reset on replacement.

## Medium

None. The scene sync reads the world through parkEntrances, parkPaths, parkBoxes, its tick, and keepPreview, and the interaction through tools.h and pickSubject, as main.cpp did. Neither samples or emits a field or flow.

## Principle checks

- Principle 1: no mesh or kept preview outlives the world it was built from. A generation syncWorld has not seen asks for every mesh and empties the kept preview, which the first two criteria check.

## Spec changes

src/app/SPEC.md:

- "## Park", after its paragraph, a new paragraph:

  > The scene sync, SceneSync in scene_sync.h, keeps what the meshes were last built from (the intent, the tick, and the ghost's look, meaning the highlight, the Inspector's subject, and the Food overlay checkbox) and the kept preview, and decides each frame which meshes to rebuild. scene_uploads.h, its platform edge, builds them and gives them to the renderer. It follows the park session's generation: a world of a generation it has not seen rebuilds every mesh, frames the camera as at start, and empties the kept preview, so the first mesh is the first of each world the session holds.

- "## Tools", after its paragraph, a new paragraph:

  > The interaction, Interaction in interaction.h, owns the tool and the Inspector's subject. It gives the tool the frame's buttons and queues the edit a release commits, picks on a Look press, and forgets the subject when the Inspector closes. It follows the park session's generation: a world of a generation it has not seen selects the current tool again and forgets the subject.

- "## Park files", the sentence "They empty the command queue, select the current tool again so it drops any hold and drawn points, forget the Inspector's subject (Tooling UI), and frame the camera on the new park's mesh as at start." gains after it: "The session empties the queue as it replaces the world, and the interaction and the scene sync do the rest when they see its new generation."

## Files affected

- Modify: src/app/SPEC.md
- Create: src/app/scene_sync.h, src/app/scene_sync.cpp
- Create: src/app/interaction.h, src/app/interaction.cpp
- Create: src/app/scene_uploads.h, src/app/scene_uploads.cpp
- Modify: src/app/main.cpp
- Modify: src/app/CMakeLists.txt
- Create (test pass): tests/app/scene_sync_test.cpp, tests/app/interaction_test.cpp
- Modify (test pass): tests/app/CMakeLists.txt

## Dependencies

- park-session's ParkSession and its generation, and tpj_app_core: met.
- legible's keepPreview and pickSubject, and tools.h: met.

## Out of scope

- Moving the cursor's ray casts and the input mapping out of main.cpp: frame-input.
- Moving the panels and the Debug panel's numbers: tooling-ui.

## Open questions

None.
