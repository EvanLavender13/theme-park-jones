# Feature: Connector Walkways

## Summary

The park mesh draws every connector navigable-networks derives as a walkway: a flat ribbon from its door to its path, laid by the rule that lays path ribbons, its kind's pathWidth wide and in its kind's pathColor, starting flush with its door's face and ending in a round joint of half that width beyond its end, so it meets the box and a path's end at any angle with no gap or notch. The renderer finds connectors through the networks' public queries alone: a carrier is a connector exactly when its first stop's node is anchored, which the routes spec makes true of connectors and of nothing else. buildParkMesh draws the walkways after the paths and before the boxes. An accepted edit's ghost appends the walkways of the edit's candidate world, from makeCandidate, at GHOST_ALPHA, so the player sees a box connect, or fail to, before committing, and the walkways committed are the ones the ghost showed (principle 8). Networks derive from intent alone, so the app's existing rule, rebuilding the park mesh when intent changes, rebuilds the walkways whenever they change, and its code does not change.

## Acceptance criteria

1. appendWalkway begins with a ribbon along the points it is given, for points each distinct from the next, as a connector's are: for n points its first 2n vertices are, at each point in order, the left edge and then the right edge, half of pathWidth(kind) either side of the point across the line, PATH_LIFT above the ground, with upward normals and the given color, and its first triangles are two facing up for each consecutive pair of points. Fewer than two points add nothing.
2. appendWalkway ends with a round joint beyond the ribbon's end: after the ribbon's vertices, one at the last point p and then WALKWAY_JOINT_SEGMENTS + 1, 17, on the half circle of half of pathWidth(kind) beyond the end, the k-th at p + h * (cos(πk/16) * r + sin(πk/16) * t), where h is half the width, t the unit direction of the last segment, and r = (-t.z, t.x), all PATH_LIFT above the ground with upward normals and the given color, and after the ribbon's triangles, one facing up from the center to each rim vertex and the next. So the joint's first and last rim vertices lie at the ribbon's end corners, and joint and ribbon never overlap.
3. appendWalkway given the unit normal n of the face its first point lies on starts flush with that face: with t the unit direction of the first segment, r = (-t.z, t.x), and h half of pathWidth(kind), its first left vertex lies at the first point minus h * r plus t times h * dot(r, n) / dot(t, n), and its first right vertex at the first point plus h * r minus t times the same, so both lie on the face's line through the first point. When dot(t, n) is 0, or that move along t is at least as long as the first segment, it is the walkway appendWalkway draws without a normal. Every other vertex and every index is the one appendWalkway draws without a normal. appendWalkway given no normal draws exactly what appendWalkway without the argument draws.
4. A network's connectors are its carriers whose first stop's node has an anchor. appendWalkways adds appendWalkway of each connector's kind and points, with the normal of its door's face, Forward of the footprint of the entity its first stop's node is anchored to, from parkEntrances with ENTRANCE_SIZE or parkBoxes with boxSize(kind), and with none when that entity has no footprint, which no connector's does, since an entity without one has no doors, for the guest network's connectors and then the backstage network's, each network's in carrier key order, in its kind's pathColor with alpha replaced by the given alpha, and nothing for any other carrier. A world with no networks, as before its first resolution, gets none.
5. buildParkMesh gives appendBox of each entrance, then appendPath of each path, then appendWalkways with alpha 1, then appendBox of each box, as src/render/SPEC.md gives each.
6. An accepted edit's ghost is the edit's own ghost followed by appendWalkways of makeCandidate(world, a queue holding the edit) at GHOST_ALPHA. So its walkways have, in order, the positions and normals of the walkways buildParkMesh draws for the world the edit gives (principle 8), and an edit that leaves a door out of reach shows no walkway for it. The edit's own ghost is the whole ghost of a refused edit, which shows no walkways.
7. The existing ghost properties hold of the edit's own ghost, the vertices before its walkways: an accepted AddBox or MoveBox's own ghost has the positions and normals of the box buildParkMesh draws for it in the world the edit gives, and an accepted AddPath's those of the ribbon it draws for the added path.
8. Building the park mesh or a ghost, walkways included, leaves the world's save and hash unchanged (principle 1).
9. A --capture of tests/parks/routes.park shows four walkways, the entrance's and the shop's front in the guest color and the shop's back and the depot's front in the backstage color, each from its door to its path. In the running app, a walkway meeting a path's end at an angle joins it with a round corner and no notch, a walkway leaving its door at an angle meets the box's face with no gap, a shop ghost within reach of a guest path shows its walkway, and one beyond reach shows none. (scripted capture, and manual)
10. src/render/SPEC.md and src/app/SPEC.md describe walkways as built. linux-debug builds without warnings and its tests pass, windows-debug builds, and the cross-build check passes.

## Medium

Networks (read): the guest and backstage networks path-networks derives, committed or in a ghost's candidate world, read through parkNetwork and the Network type's public carriers() and nodeAnchor (sim/routes/SPEC.md, sim/medium/SPEC.md). The renderer never reads how they were derived. Tentative intent: buildGhostMesh passes the ghost's edit to deterministic-simulation's makeCandidate through queueEdit, as decision 0025 has previews do. No fields or flows are sampled or moved.

## Principle checks

- Principle 1: walkways are drawn, never held. Building meshes and ghosts leaves the save and hash unchanged (criterion 8), and makeCandidate leaves the world unchanged.
- Principle 2: a world with no networks, an edit that is refused, and a door with no connector all build meshes and ghosts without error (criteria 4 and 6).
- Principle 6: connectors are found through the networks' public anchors (criterion 4), never through routes' internals or connectorKey.
- Principle 8: an accepted edit's ghost walkways are the walkways its commit draws (criterion 6), and its own ghost is still what its commit draws (criterion 7).

## Spec changes

In src/render/SPEC.md, Park mesh:

The first paragraph's "It reads intent only through sim/park's public queries and geometry (sim/park/SPEC.md), and changes nothing." becomes "It reads intent only through sim/park's public queries and geometry (sim/park/SPEC.md), and the networks only through parkNetwork (sim/routes/SPEC.md) and the Network type's public queries (sim/medium/SPEC.md), and changes nothing."

The appendPath paragraph becomes:

    A ribbon of a width along a line of n points, n at least 2 and each point distinct from the next, is flat and PATH_LIFT, 2 cm, above the ground. It has 2n vertices, two for each point in order: the left edge, at the point minus the right direction times half the width, and then the right edge, at the point plus it. The right direction of a unit tangent (tx, tz) is (-tz, tx). The tangent at the first point is the direction of the segment after it, and at the last point that of the segment before it. At a point between, it is the normalized sum of the unit directions of the segments before and after it, or the one before when that sum has zero length. The normals point up. Each consecutive pair of points adds two triangles covering the quad between their four vertices. Where the line bends tighter than half the width, or turns straight back, the ribbon folds over itself, and triangles there can face down and be culled.

    appendPath adds a path's ribbon: pathWidth(kind) wide along its groundLine, in pathColor(kind). An empty ground line adds nothing.

    A network's connectors are its carriers whose first stop's node has an anchor, which sim/routes/SPEC.md makes exactly the connectors path-networks derives, each from a door to its path. appendWalkway adds a walkway: the ribbon pathWidth(kind) wide along the points it is given, in a given color. Fewer than two points add nothing. Given the unit normal n of the face its first point lies on, a walkway starts flush with that face rather than square: with t the unit direction of its first segment, r = (-t.z, t.x), and h half the width, its first left vertex moves along t by h * dot(r, n) / dot(t, n) and its first right vertex by the negative of that, so both lie on the face's line through the first point. When dot(t, n) is 0, or that move is at least as long as the first segment, the ribbon would fold, and the walkway keeps its square start; a physically valid park never gives one, since its path stays half its width from the face. A walkway ends in a round joint beyond its last point, so it meets its path at any angle, even at the path's end, with no notch: after the ribbon, appendWalkway adds the joint's center, at the last point p, and then WALKWAY_JOINT_SEGMENTS + 1, 17, vertices on the half circle of half the width beyond the ribbon's end, the k-th at p + h * (cos(πk/16) * r + sin(πk/16) * t), where h is half the width, t the unit direction of the last segment, and r = (-t.z, t.x) its right direction, all PATH_LIFT above the ground with upward normals and the walkway's color, and a triangle facing up from the center to each rim vertex and the next. The first and last rim vertices lie at the ribbon's end corners, so the joint and the ribbon never overlap, and a translucent ghost walkway blends once everywhere. appendWalkways adds appendWalkway of each connector of parkNetwork(world, Guest) and then of parkNetwork(world, Backstage), each network's in carrier key order, with the network's kind and the connector's points, and the normal of its door's face: Forward of the footprint of the entity its first stop's node is anchored to, from parkEntrances with ENTRANCE_SIZE or parkBoxes with boxSize(kind), or none when that entity has no footprint, which no connector's does, since an entity without one has no doors, in the kind's pathColor with its alpha replaced by a given alpha. So a walkway runs from its door to its path in its kind's width and color.

The buildParkMesh paragraph's "then each path from parkPaths, then each box from parkBoxes" becomes "then each path from parkPaths, then appendWalkways with alpha 1, then each box from parkBoxes".

In src/render/SPEC.md, Ghosts:

The buildGhostMesh paragraph becomes:

    buildGhostMesh gives an edit's ghost on a world: the edit's own ghost, followed, when isAccepted(world, edit), by appendWalkways of the candidate world, makeCandidate(world, a queue holding the edit through queueEdit), with alpha GHOST_ALPHA. An edit's ghost color for a color c is c with alpha GHOST_ALPHA when isAccepted(world, edit), and INVALID_TINT when not. An AddBox's own ghost is appendBox at its pose with its kind's boxSize and boxHeight, in the ghost color of boxColor(kind). A MoveBox's is the same at its pose with the kind of the box its key holds, and nothing when its key holds no box. An AddPath's is appendPath of its kind and points in the ghost color of pathColor(kind). An accepted DeletePath or DeleteBox's is appendEntity of its key in DELETE_TINT, and a refused one's is nothing. So the own ghost of an accepted AddBox or MoveBox has the positions and normals of the box buildParkMesh draws in the world the edit gives. Likewise, the own ghost of an accepted AddPath has the positions and normals of the ribbon buildParkMesh draws for the path it adds, since a ribbon depends only on its kind and its ground line, and the ground line of the points the path keeps is that of the edit's points. And an accepted edit's ghost walkways have the positions and normals of the walkways buildParkMesh draws in the world the edit gives, so a door the edit leaves out of reach shows none. A ghost walkway lying on a committed walkway of the same kind blends to that walkway's color, so only the walkways the edit adds or moves stand out.

In src/app/SPEC.md, Park, after "it builds the park mesh and gives it to the renderer.": "The mesh holds the walkways of the world's networks, which derive from intent alone (sim/routes/SPEC.md) and are resolved in every world the app holds, so the mesh is rebuilt whenever they change, and the ghost, rebuilt with it, shows its candidate's walkways."

## Files affected

- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: src/render/SPEC.md
- Modify: src/app/SPEC.md
- Modify (test pass): tests/render/park_mesh_test.cpp, tests/render/ghost_mesh_test.cpp, and any new file under tests/render/

## Dependencies

- navigable-networks' box-connections: connectors and their anchored door nodes in the derived networks. Merged.
- sketch-a-park's park-view and box-tools: the park mesh, ribbons, ghosts, and the translucent pass. Merged.
- deterministic-simulation's makeCandidate and park-edits' queueEdit. Met.

## Out of scope

- Marking the walkways an edit removes, such as a deleted box's or a moved box's old one, in DELETE_TINT. Sent to the milestone's deepening candidates.
- Cleaner joins where a walkway crosses another ribbon: the milestone's crossings candidate.
- Changing the graph overlay's connector marking to the anchor rule. Sent to paths-become-routes' deepening candidates.

## Open questions

None.
