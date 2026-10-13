# Research: kept-preview

## Whether a kept candidate can follow ticks

It can't, short of resolving. makeCandidate's finishers re-place every guest and clear stepped field entries by comparing the world's resolution with the candidate's. An added path or box takes a key from the counter, and arriving guests take keys from it too. After one arrival, the candidate's new entity and everything derived from its key differ. The milestone's RESEARCH.md has the details. The user chose to leave the preview an overlay makes again at each tick, while it holds a candidate, to changing-previews, which makes resolution reuse its storage. Decision 0041 now says so.

Rejected: running only the edit and the finishers again on kept derived data — exact only until a guest arrives, and it splits every resolver. Resolving into kept storage now — that is changing-previews' work.

## What steady play repeats around the preview

The app calls these every frame (src/app/application.cpp):

- SceneSync::syncPreview runs keepPreview. With the edit unchanged and no overlay, it keeps the preview and reads a shop ghost's footfall from the world again. That read samples hungry footfall, a kept field, at a node. sampleKeptField returns a new list, and the node rule builds a NodeSample with lists of its own and returns a new list.
- While an overlay is shown, keepPreview follows ticks and makes the preview again at each tick. With no edit, that costs nothing, because previewEdit returns at once. With a refused edit, it runs isAccepted again, which allocates. isAccepted reads only intent and what sim/park derives from it, because sim/park sits below every module with state (cmake/layers.txt). A preview without a candidate therefore holds nothing a tick changes.
- SceneSync::syncOverlayShape runs twice a frame while an overlay is shown. Each call builds parkPathPoints and parkSegments to compare them with those the shape was built from. parkSegments derives every segment's rounded line.
- SceneSync::syncLook says whether to build the ghost and the shades again: when the preview was made again, when the look changed, or at each tick while the Inspector's subject is a guest. uploadPreviewMeshes then builds the whole ghost again in scene_uploads.cpp, which only tpj_app compiles: the edit's ghost with buildGhostMesh, and the look's marks. It also shades the overlay again whenever the ghost is built, even when only the look changed.
- Shading runs availabilityBends, availabilityValues, and shadeOverlay. availabilityBends makes about ten lists, including parkSegments, only to learn which carriers are guest segments. availabilityValues makes its values, its counted sources, and sampleFieldAtPlaces' slots and lists. shadeOverlay makes two lists and runs stable_sort, which allocates a buffer.
- The marks of a look use appendEntity and appendSides. appendEntity calls parkBoxes and parkPathPoints and copies surface pieces. appendSides(world, key) calls parkSegments. Both allocate, but they depend only on the look and the park's intent.

## What can be kept instead of built again

The edit's ghost depends on the world's intent, the edit, and the candidate. All three stay the same until the preview is made again, and the scene empties the kept preview when an edit applies. So the scene can build the edit's ghost only when the preview is made again.

A look's marks depend only on the look and the park's intent. So the scene can build them only when the look or the edit count changes. Pointer movement changes the look, and quiet-tools covers pointer movement.

The inspected guest's mark moves at each tick, and appendGuestEntity allocates nothing, as buildGuestMesh shows. A box subject's mark is fixed, so it joins the look's marks. Choosing appendGuestEntity or appendEntity by the subject's kind gives the same marks as calling both, since each adds nothing for the other's keys.

The frame's ghost is then put together from the three parts in today's order: the edit's ghost, then the look's marks, then the subject's mark. Copying into kept meshes reuses their memory.

The overlay's shape depends only on the previewed world's path points and segments (src/render/SPEC.md). The world's paths change only when the edit count does. The previewed world's paths differ from the world's only when the preview holds a candidate of an edit that can change paths. Box edits, shop stock, and opening hours touch no path intent (src/sim/park/edits.cpp). So the scene keys the shape on the edit count, and on the edit only for such a candidate. Moving a shop or a refused edit then rebuilds nothing, as comparing the paths does today. Treating an edit as changing paths when it doesn't only rebuilds an equal shape. The scene compares the kept key with the current values directly and copies the edit only when the key changes, since an AddPath holds a list.

The shades depend on the previewed world, the shape, and the need. So the scene shades again only when the preview was made again, the shape changed, the need changed, or the previewed world's tick changed.

Rejected: making buildGhostMesh, appendEntity, and appendSides allocation-free — steady play doesn't need to call them, and the edit's ghost is rebuilt only when the preview is, which changing-previews covers. Keeping the shape's comparison by path lists in kept buffers — it would still derive every rounded line every frame. Keying the shape on every edit — it would rebuild the whole shape every frame while a shop ghost moves, where comparing paths rebuilds nothing.

## How the overlay's lists are kept

The availability functions keep their working lists in a struct the caller holds, as sampleField's FieldSample does. sampleFieldAtPlaces gets a form that takes a FieldSample. The guest segments' keys come from a new fill form in sim/park that reads segment intent without deriving lines. It lists the same keys parkSegments lists, because layoutOf takes every entity with segment intent. shadeOverlay's stable sort becomes an insertion sort, which is also stable and allocates nothing. availabilityBends gives places already in order, so the sort does one pass.

A kept field's node rule appends to a list it is given, as sampleEdge already does. The sample keeps the node's edge ends it no longer uses for later ends, as swapSteppedEntries keeps retired slots. That way an end's list keeps its memory.

## Cost in the check

On windows-debug, one food overlay shading takes about 481 ms (tpj_bench, full.park). Shading four needs at all 600 watched ticks would add about 19 minutes. The check therefore shades each need at every 60th tick, warm-up included. That makes 15 shadings of each need, about 30 s in all. The rule catches a block allocated at one shading and freed at a later one, so sparse repeats are enough.
