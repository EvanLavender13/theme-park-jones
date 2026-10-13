# Research: quiet-tools

## What a frame's tool work does today

The app's frame (src/app/application.cpp) calls these on the Interaction, which passes each to src/tools:

- movePointer, which finds the snap for the path tools and Edit path's dragged point, or the frontage pose for the place tools.
- tentativeEdit, the edit the preview shows.
- highlighted, pathStart, pathFinish, editHandles, and hoveredHandle, which make up the tool's part of the frame's look.
- tooling_ui.cpp also calls snapGuides and measuredLengths every frame to draw the guides.

Almost every one of them reads the park through sim/park's returning queries. parkSegments and parkCorners derive every segment's rounded ground line from intent, and parkEdges and parkTapers derive the paving's edges. parkPathPoints and parkBoxes copy intent into new lists, and parkDoors builds its doors from new lists of entrances and boxes. A frame of Edit path calls them many times: handleAt alone calls parkCorners, parkEdges, and parkTapers, and highlighted, hoveredHandle, and tentativeEdit each call handleAt. On full.park, with its 233 segments, each call derives every line again.

On top of that, each piece makes lists of its own:

- Snapping builds a SnapScene of copies of the points, segments, and corners, the drawn stops' positions, two lists of left-out flags, a list of anchors each with its own list of arrivals, and a list of guides. It returns a PointerSnap holding two new lists. resnap resets the snap in effect, which frees those lists.
- The frontage search makes the edge lines' steps, the pieces near the box, and the places it tries.
- nextStop, pathFinish, and measuredLengths copy the drawn stops into an AddPath, and stopPositions returns a new list. The path tool's edit copies the drawn stops again.
- insertAt makes a list of at most three places. guestSegmentAt and segmentOf return a copy of a segment with its line.
- editHandles, snapGuides, and measuredLengths return new lists. The app then puts the handles in a new PreviewLook every frame.
- stopPosition of an OnBend stop calls parkCorners to find its corner.

## What the tools can keep

What the tools read of the park changes only when its intent does. Intent changes only when an edit, an undo, or a redo applies, or when a park is opened. The app knows both from the session's edit count and generation, as SceneSync already uses them. So the caller can keep the park's points, segments, corners, edges, tapers, boxes, and doors, and fill them again only when the edit count or the generation changes. That is a ToolPark. The queries then scan kept lists and derive nothing. An edit isn't steady play, so filling the ToolPark may allocate.

Snapping and the frontage search still need working lists each call, because what they hold depends on the pointer. They keep them in the ToolPark too, cleared and refilled each call, so they grow to the most the park needs and then allocate nothing. Those lists hold types private to the tools: anchors, guides, edge steps, and places tried. The ToolPark holds them through a pointer to a type declared in tools/internal, so tools.h shows none of them. It is made once, with the ToolPark.

A snap never has more than two guides or two holds, as the private Snap type already says. So PointerSnap can hold them in fixed arrays with counts, as seat service holds its seats. Then the snap in effect is a plain value, and resetting it frees nothing.

An anchor's arrivals can live in one flat list, with each anchor holding where its arrivals start and how many there are. Anchors are built one at a time, so each anchor's arrivals are the end of the list while it is built.

The drawn stops' positions come from stopPositions. Its only allocating step, apart from the list it returns, is finding an OnBend's corner through parkCorners. sim/park can give a form that appends one stop's position to the positions of the stops before it, reading the corners from a list the caller gives. With the ToolPark's corners, it allocates nothing. stopPositions and stopPosition then share its code, so the simulation gives the same positions.

The path tool's edit is an AddPath whose stops are the drawn stops and the next one. If the caller keeps the edit and the tools refill it in place, its list keeps its memory. copyGrowing in src/sim/tick_buffers.h copies into a kept list without the exact-size reallocation that assign makes. The app passes the kept edit to SceneSync::syncPreview, which already takes it by reference.

The look's handles can fill the kept look's list in place. PreviewLook lives in app/scene, which sits above app/input in cmake/layers.txt. So the function that fills the tool's part of the look goes beside PreviewLook in scene_sync.h, where the check can call it too.

Rejected: fill forms for parkSegments, parkCorners, parkEdges, and parkTapers in sim/park — they would still derive every line several times a frame, and the derivation makes many lists inside sim/park/layout.cpp and surface.cpp. A ToolPark derives once per edit. Keeping the frame's answers and recomputing them only when the pointer moves — the check would pass while the work itself still allocated, and the hover is steady play. Per-thread scratch lists — the milestone's research rejected hidden global state for caller-owned buffers. Making the private list types public in tools.h — it would show snapping's internals to the app.

## The look's marks

kept-preview builds the look's marks only when the look or the edit count changes. A moving pointer changes the look often: the highlight, the hovered handle or side, and a path's start mark. Each rebuild calls render's appendEntity, which calls parkBoxes and parkPathPoints and copies a segment's surface pieces into a new PavedSurface, and appendSides, which calls parkSegments. The other marks only add vertices to the kept meshes. The hover is steady play, so this is a loop allocation the conventions say to fix now. appendEntity and appendSides can take the boxes, path points, and segments as lists, and appendEntity can add a segment's own pieces straight from the resolved surface, as appendSurface adds each piece, without copying them. The copy held no edge lines, so the mesh is the same. SceneSync keeps those three lists and fills them again when the edit count changes, which it already tracks for the look. The check can't move the pointer, but it can alternate a held look between two looks at each tick, which rebuilds the marks as a moving pointer does.

## What full.park lets the check reach

full.park has 222 guest segments of width 3 and 11 backstage segments of width 2. Every path point has radius 0 and taper 0, and every segment has edge radii of 0. No path point joins exactly two segments, so the park has no corners and no tapers. Its junctions still have edges, since parkEdges gives every edge at a point of two or more segments, whatever its radius. It has 30 shops, 3 depots, 45 benches, and 15 toilet blocks. So the check can hover an edge handle, but not a radius or taper handle, and no snap lands on an arc. Adding curves to full.park would change the milestone's before and after runtime reports, so the park stays as it is. The check instead adds a guest path in open ground before its warm-up, rounds its corner, and widens one segment so a taper forms. The edits apply only in the check's own session.

The check can hold each tool still at places it finds from full.park's intent: on a guest segment, on one of its sides, on its point, just beside its paving, beside a backstage segment, on an edge handle, and on a shop. It can also hold a press: a path with one or two stops drawn, a shop held by Move box and dragged a meter, and a point dragged by Edit path. Pressing isn't steady play, so the presses happen before the warm-up.

## Cost in the check

After the change, a frame's tool work scans kept lists. Snapping looks at every point, segment, and door, and checks each guide against every segment. The frontage search walks every edge line. On full.park that is thousands of simple steps, well under a millisecond on windows-debug. Twenty-three held tools at each of 900 ticks adds about 21,000 tool frames. That should add seconds, not minutes, to the check's 286 seconds.
