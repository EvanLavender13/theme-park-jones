# Research: unit-surface

## What the surfaces and connections cost in a changed frame today

The question is how much of each drag's changed frame the two surfaces and the doors' connections take, and so what moving them onto units can save.

The eight Very Sleepy profiles derived-units took on windows-release tpj_bench (build/perf/derived-units-1.sleepy to -8) hold the timed frames of both drags. Inside resolveWorld, these take:

| Drag | Frame median (flat-ground report) | Paved surface | Walked surface | findConnections | Listing walkways | All four |
|---|---|---|---|---|---|---|
| shop-drag | 9.48 ms | 4.2% | 8.4% | 18.0% | 0.4% | 31.0%, about 2.9 ms |
| path-drag | 13.33 ms | 4.8% | 7.5% | 12.4% | 0.5% | 25.2%, about 3.4 ms |

In the surfaces, most of the time is hiding, not building:

- Hiding is about 8% of the shop drag's frame and 9% of the path drag's. hideJoined scans every piece's bounds for each edge line, so it does edges × pieces work.
- Building the paving is about 1.5% for each surface. Joins and copying the ground from the units are under 0.4%.

findConnections measures every door against every piece, so it does doors × pieces work.

Outside resolution, the path drag spends another 10% in whole-surface functions: editedSurface and surfaceAround in the acceptance check and the ghost. layered-candidate owns those.

If the surfaces and the connections derive only what an edit reaches, the shop drag should fall by about 2.8 ms and the path drag by about 3.2 ms. A moved shop changes no path intent, so only its own doors' connections and walkways change. A drawn path changes a few points and segments near it, and the connections of doors near them.

Sources: build/perf/derived-units-1.sleepy to -8.sleepy; build/flat-ground.report; src/sim/park/surface.cpp; src/sim/routes/networks.cpp.

## What each part of the paving reads

The question is how buildSurface's whole-layout passes split into per-point and per-segment derivations that give exactly what it gives.

buildSurface runs askPoints, then two passes of forEachPoint in ascending point key order, then a loop over the segments. Reading trimPoint, fitPoint, buildPoint, and Output shows these dependencies:

- What a segment end asks for at a point P depends only on P's own segments, P's corner, and P's taper. Asking uses each segment's whole length as its reach.
- The trims at P depend on P's segments, P's corner, P's taper, and what the far end of each of P's segments asks for. Shape::reach reads the far end's ask. Nothing else at another point enters.
- P's paving and edge lines depend on the trims at P, and on the trims at the far end of each of P's segments. Each edge line runs to the middle of a side, which lies between the trims at both ends.
- A segment's body piece depends on the trims at both of its points. Its two sides depend on those trims and on the turn halves buildPoint makes at both points.
- At a corner, P's paving belongs to the two segments, split by the line into the corner. A segment's pieces come in this order: the corner pieces at its lower-keyed point, then those at its higher-keyed point, then its body.
- Edge lines belong only to points. Sides belong only to segments.

So the paving splits into three point kinds and one segment kind, each reading only earlier kinds:

1. Each point's asks, from its own segments' ground lines.
2. Each point's fit: the trims at it, its pairs, its taper, and its turn halves. It reads the asks of its segments' far ends.
3. Each point's paving: its own pieces, the corner pieces it gives its segments, and its edge lines before hiding. It reads its own fit and the fits of its segments' far ends.
4. Each segment's paving: its corner pieces, its body, and its sides. It reads the fits and paving of its two points.

The pass order in buildSurface never feeds one point's result into another except through these reads, so each kind gives bit-for-bit what buildSurface gives, if it runs the same functions with the same inputs. Shapes must be made with shapesOf's expressions.

Sources: src/sim/park/surface.cpp (buildSurface, askPoints, trimPoint, fitPoint, buildPoint, Output, appendHalves).

## How hiding can read only nearby pieces and stay exact

The question is how one point's edge lines can be hidden without scanning the park, while giving what hideJoined gives.

hideJoined hides each edge line with the pieces whose owner is in its group and whose bounds overlap the edge line's bounds. It then tests every step of the edge line against each of those pieces. writeStep sorts the hidden stretches, so the order the pieces come in doesn't change the result. Two equal stretches change nothing either. So hiding gives the same result from any list that holds exactly those pieces, in any order.

A grid of cells, filed from each piece's bounds, finds them. File each owner under every cell its pieces' bounds cover, by floor of each coordinate over the cell size. Look up every cell an edge line's bounds cover. If two bounds overlap, some point lies in both, and its cell is in both ranges. Then filter by group and by the same bounds test hideJoined uses. That gives exactly hideJoined's list.

The grid has to be filed from unit values, the pieces, not from intent. derived-units' indexes file entities by keys an intent component names. A grid index needs the same mechanism with a unit kind as its source: when a unit's value changes, the index files that key again. A hiding unit records a read of each cell it looks up, so it derives again when an owner joins or leaves one of its cells. It also reads each owner's pieces, so it derives again when they change.

Groups need a different mechanism. Joins numbers groups by union-find over every segment with a ground line, and the numbers depend on the whole park. A group's label can be its lowest point key instead. Then a path drawn from an existing path leaves every existing label as it was, since a new point's key is higher. Labels are kept in their own table and updated after the ground lines:

- Only a segment whose link changes counts. A link is the segment's two points, while it has a ground line.
- After the links change, flood fill each component that holds an end of a changed link, and set each point's label to the lowest key in it.
- Put a change on the list only for a label that changed, so only units that read it derive again.

The milestone's criterion 3 allows finding which paths join which, once per edit, to grow with the park. A shop drag changes no link and fills nothing. A path drag fills the network it joins, about 155 points on full.park.

Sources: src/sim/park/surface.cpp (hideJoined, insideOf, writeStep, Joins, Bounds); src/sim/units.h (updateIndex); plans/scalable-runtime/changing-previews/RESEARCH.md (Parkitect's flood fill).

## How the surface can be stored per owner and still walk the same words

The question is how the derived surface can change only its changed owners while the walk, and so every hash, stays as it is.

PavedSurface holds five flat lists today. EdgeLine.First and SegmentSide.First are offsets into the flat point lists. Changing one owner means moving everything after it and fixing the offsets, which is whole-park work.

Every list is in ascending owner order:

- Pieces are stable-sorted by owner.
- Edge lines are sorted by owner. After hiding, the Visible writer writes their points in edge order with no gaps.
- Sides are in segment key order, and Output::side writes their points in order.

So the flat lists are the per-owner runs joined in owner order. If the surface stores each owner's run with offsets inside the run, its visitFields can emit each list as its size, then its elements owner by owner, adding each run's start to the offsets as it goes. emitField writes a list as its size followed by its elements, so that gives the same words. Derived components are never saved or loaded (save.cpp), so visitFields never has to read a surface back.

The runs need storage that a copy refills without allocating. std::map's copy assignment frees the nodes it doesn't reuse, and the next frame allocates them again. KeptMap keeps removed nodes for later adds, but it can't be copied, since its spare nodes are move-only. Giving it a copy that copies only its values, and an assignment that reuses its nodes, makes it usable inside a component.

Every reader reads the lists in order today. If it reads the runs in owner order, it sees the same pieces, edge lines, and sides in the same order, so what it derives is the same. Readers in 14 files touch the surface:

- Resolvers: the path networks (all pieces), guest ground (the walked surface's pieces and edge lines), and furniture (each segment's sides).
- The acceptance check and ghost in edits.cpp and park_mesh.cpp, which compare or select owners' runs.
- Tools (frontage, the paving hover), the render meshes and overlay, and one integration test.

Sources: src/sim/park/surface.h; src/sim/schema.h (emitField); src/sim/save.cpp; src/sim/unit_space.h (KeptMap).

## How the walked surface can change only where walkways change

The question is how the walked surface can derive only what changed, when its walkways come from the doors' connections.

walkedSurfaceOf builds the paving, adds each walkway's piece and three edge lines to the segment it joins, and hides every edge line in one pass with every piece. For an edge line that no walkway piece of its group overlaps, the list of hiding pieces is the paved surface's list, so its result is the paved surface's. Only owners near a walkway differ. Hiding the walkways after the paving instead isn't the same, because hiding twice cuts edge lines at different places.

Units read only intent and earlier units. Today the walkways come from the path-networks resolver, so no unit can read them. The connections they come from are local, though (next section). If each door's connection and walkway is a unit, the walked surface is units all the way down:

- A grid index files the walkway pieces.
- Each point's walked edge lines are its paved ones when no walkway piece of its group lies near them, and are hidden again with the walkway pieces when one does.
- Each segment's walkway edge lines are hidden with the pieces near them.
- The walked surface stores full runs per owner, so its walk is unchanged, and changes only the owners whose units changed.

Rejected:

- A table the walked-surface resolver fills with each segment's walkway pieces, for units to read. It works, but it is a second way into units that exists only until the walkways are units, and it still lists every walkway on every frame.
- Hiding again, at each resolution, every owner near any walkway. Each walkway lies near its segment and the edge lines of that segment's two points, so most of full.park's owners would be hidden again on every frame. That saves little of what the walked surface's hiding costs today.
- Keeping last resolution's walkways in the caller's buffers. A resolver's result must depend on the world alone (principle 10).
- Storing only the owners walkways change, and reading the rest from the paved surface. The walk sees one component at a time, so the walked surface's words would change.

Sources: src/sim/routes/networks.cpp (resolveWalkedSurface, keyedWalkways); src/sim/park/surface.cpp (walkedSurfaceOf, addWalkway, walkwayPiece); docs/principles.md.

## Whether a door's connection and walkway can be units

The question is whether findConnections and keyedWalkways give, for each door, a result that depends only on what lies near the door, so that a unit can derive it exactly.

A door comes from its entrance's or box's intent alone (doorsServing, addDoor). findConnections then, for each door:

- Measures the distance from the door to every piece of the kind, keeping each owner's least (ownerDistances). A carrier's paving distance is the least of its segment's and its two points' (pavingDistances). If the least is over CONNECTION_REACH, the door doesn't connect.
- Of the carriers whose paving lies within JUNCTION_TOLERANCE of the least, takes the one whose line lies nearest the door, the lower key winning a tie (nearestLine).
- Leaves the door unconnected when a path holds the connector's key.
- Routes the connector straight out of the face to the paving, using the first piece a ray from the door meets within CONNECTION_REACH plus JUNCTION_TOLERANCE (rayEntry), then to the line.

Only pieces within CONNECTION_REACH plus JUNCTION_TOLERANCE of the door can change any of that. A lookup of the piece grid over that square, filtered by kind, gives every owner that matters, and the carriers' lines come from the ground-line units by key. The path holding the connector's key is a lookup by key. So each door's connection is local, and a unit per entrance or box can derive it exactly. findConnections then stops measuring every door against every piece, which is 18% of the shop drag's frame and 12% of the path drag's.

keyedWalkways gives a connector's walkway to the lowest-keyed segment at the node where it meets its path. That node is the connector's stop group on the path. groupStarts groups a carrier's sorted stop distances, each group holding every distance within JUNCTION_TOLERANCE of its first. The carrier's ends and the other connections on the same path are stops too. So a walkway depends on its own connection, on the other connections that meet the same path, and, when its group holds an end, on the segments at that point. An index of connections by the path they meet gives the other connections. The segments at a point come from the existing segment-ends index.

The path-networks resolver still builds the whole network and numbers its nodes from every carrier and connection. It reads the connections from the units, in the order doorsServing lists the doors, so it builds what it builds today. spatial-units still owns the network's numbering, furniture, guest ground, and the offers.

Sources: src/sim/routes/networks.cpp (doorsServing, addDoor, findConnections, ownerDistances, pavingDistances, nearestLine, rayEntry, connectorRoute, pathCarriers, pointMeetings, groupStarts, addStops, keyedWalkways).

## How the allocation check can watch changed frames before the whole frame is clean

The question is how the allocation check can watch drag frames now, when only some of a changed frame's work is free of allocations.

The check watches work between its own calls and can pause and resume (tests/checks/allocations/watch.h). Resolution runs every resolver in one call, inside keepPreview. To watch only the resolvers that the milestone has moved, the check has to know when each one starts and ends. If resolveWorld told a trace held in the caller's TickBuffers the name of each resolver as it starts and ends, the check could resume the watch only inside the named ones. KeptPreview's Buffers are public, so the check can set the trace for the preview's resolutions. Unit kinds and indexes are resolvers named after themselves, so their names are known.

Then each feature of the milestone adds the resolvers it moves to the watched list, and watched-previews extends the watch to the whole changed frame. Decision 0041 says changing previews wait for watched-previews, so it needs amending in place.

Rejected: running the resolvers one by one from the check. resolveWorld's setup of the units and its resolving flags aren't public, and copying them into the check would test a copy.

Sources: tests/checks/allocations/watch.h; tests/checks/allocations/allocation_check.cpp; docs/decisions/0041-allocation-check.md; src/sim/cycle.cpp; src/legible/preview.h.

## What stays whole-park after this feature, and who owns it

The milestone splits this research's design over two features. unit-surface moves the paving, its hiding, and the per-owner storage of both surfaces. unit-connections moves the connections, the walkways, and the walked surface's hiding onto units, using unit-surface's grid. After unit-surface:

- The walked-surface resolver still lists every walkway and builds and hides the whole walked surface, and findConnections still measures every door against every piece: unit-connections.
- The acceptance check, editedSurface, surfaceAround, pointsAffected, and the ghost derive surfaces on hypothetical layouts with buildSurface: layered-candidate.
- Guest ground and furniture read the walked surface whole, and the path networks, guest ground, and furniture still copy the ground with worldGround: spatial-units.
- The path networks' node numbering, built from every carrier and connection: spatial-units.
- keepToolPark fits every edge and taper with parkEdges and parkTapers, once per world change, not in a changed frame. The tools' lists hold every point and segment by design.
