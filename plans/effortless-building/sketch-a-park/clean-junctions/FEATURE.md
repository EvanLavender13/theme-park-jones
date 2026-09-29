# Feature: Clean Junctions

## Summary

clean-junctions fixes two flaws in how the park mesh draws where ribbons meet. Path ribbons end square, so two paths whose ends meet at an angle leave a notch at the outer corner. And every ribbon lies at the same height, so where a guest ribbon crosses a backstage one the two tie in depth and flicker. appendPath now closes both ends of every path with the round joint walkways already end in, a half disc of half the width beyond the end, so ends meeting at any angle join with a round corner and a dead end is round. Each kind gets its own lift, pathLift(kind): 4 cm for guest paths and 2 cm for backstage ones, a gap equal to the backstage ribbon's height over the terrain, so a guest ribbon always draws over a backstage one as a ribbon draws over the grass. Walkways lie at their kind's lift, and ghosts and highlights stay on what they mark, since they are built by the same functions. The joint's segment count, formerly WALKWAY_JOINT_SEGMENTS, becomes JOINT_SEGMENTS, since paths use it too. Only src/render changes.

## Acceptance criteria

1. pathLift(kind) is 0.04 m for a guest path and 0.02 m for a backstage path, so a guest ribbon lies above a backstage one by at least as much as a backstage ribbon lies above the terrain.
2. appendPath begins with the ribbon along its ground line of n points: its first 2n vertices are, at each point in order, the left edge and then the right edge, half of pathWidth(kind) either side of the point across the line, pathLift(kind) above the ground, with upward normals and the color, and its first triangles are two facing up for each consecutive pair of points. An empty ground line adds nothing.
3. After the ribbon, appendPath adds a round joint beyond the line's first point and then one beyond its last point, each by the joint rule of src/render/SPEC.md: its center at the end point p, then JOINT_SEGMENTS + 1, 17, rim vertices, the k-th at p + h * (cos(πk/16) * r + sin(πk/16) * t), with h half of pathWidth(kind), t the unit direction pointing away from the ribbon (the first segment's direction reversed at the first point, the last segment's direction at the last point), and r = (-t.z, t.x), all pathLift(kind) above the ground with upward normals and the color. After the ribbon's triangles come the first joint's and then the last joint's, one facing up from the center to each rim vertex and the next. So each joint's first and last rim vertices lie at the ribbon's corners at that end.
4. Two paths of the same kind whose ribbons end in full-width rectangles reaching at least half their width back from a common end point, as straight ends do, leave no notch at any angle between them: every ground point within h * cos(π/32) of that end point, where h is half of pathWidth(kind) and h * cos(π/32) is the distance from a joint's center to its nearest rim edge, lies inside an upward-facing triangle of the mesh the two appendPath calls add.
5. appendWalkway lies at its kind's lift: every vertex it adds is pathLift(kind) above the ground, and its round joint beyond its last point follows the joint rule with JOINT_SEGMENTS. Its vertices' ground positions, normals, colors, and indices are otherwise as src/render/SPEC.md already gives them, flush start included.
6. In buildParkMesh of a world holding guest and backstage paths with walkways, every vertex in pathColor(Guest) lies at pathLift(Guest) and every vertex in pathColor(Backstage) at pathLift(Backstage). No box or entrance face, lightened front included, takes a path color, so a kind's path color marks exactly its paths and walkways. So wherever ribbons of the two kinds cross, the guest ribbon is the higher.
7. The ghost properties still hold with joints and lifts: appendPath given a color draws, vertex for vertex, the ribbon and joints appendPath draws with that color replaced; appendEntity of a path key draws them in the color given; and an accepted AddPath's own ghost has, in order, the positions and normals of what buildParkMesh draws for the path it adds (principle 8).
8. In the running app, two guest paths drawn to meet end to end at an angle join with a round outer corner and no notch, a path's free end is round, and where a guest path crosses a backstage path the guest ribbon draws over it without flicker as the camera orbits and zooms across its range. (manual)
9. src/render/SPEC.md describes joints and lifts as built. linux-debug builds without warnings and its tests pass, windows-debug builds, and the cross-build check passes.

## Medium

None. The feature changes how the renderer draws intent and the networks' connectors, which it already reads through sim/park's public queries and geometry and the networks' public queries. No fields or flows are sampled or moved, and nothing in tpj_sim changes.

## Principle checks

- Principle 1: joints and lifts are drawn, never held. The existing property that building the park mesh and ghosts leaves the save and hash unchanged still holds.
- Principle 8: an accepted AddPath's own ghost, joints included, has the positions and normals its commit draws, and a delete highlight lies exactly on the path it marks (criterion 7), since both are built by appendPath at the kind's lift.
- Principle 10: the change stays in tpj_render, and tpj_sim's outputs are unchanged, which the cross-build check confirms.

## Spec changes

In src/render/SPEC.md, Contract, the setParkMesh paragraph's last sentence becomes:

    Depth is reversed, the near plane at 1 and the far plane at 0 in a float depth buffer where the device has one, so precision holds across the camera's range and paths lying just above the terrain never flicker through it, nor ribbons of one kind through those of the other where they cross.

In src/render/SPEC.md, Park mesh, the ribbon paragraph's first sentence becomes:

    A ribbon of a width along a line of n points, n at least 2 and each point distinct from the next, is flat and lies at a lift above the ground.

and after that paragraph come two new paragraphs:

    A kind's lift is pathLift(kind): 4 cm for a guest path and 2 cm for a backstage path. Paths and walkways lie at their kind's lift, and so do their ghosts and highlights, which appendPath and appendWalkway build. The guest lift exceeds the backstage lift by the backstage lift itself, so where ribbons of the two kinds cross, the guest ribbon draws over the backstage one without flicker, as a ribbon draws over the terrain.

    A round joint closes a ribbon's end at a point p, for the unit direction t pointing away from the ribbon there: its center at p, then JOINT_SEGMENTS + 1, 17, vertices on the half circle of half the ribbon's width beyond the end, the k-th at p + h * (cos(πk/16) * r + sin(πk/16) * t), where h is half the width and r = (-t.z, t.x), all at the ribbon's lift with upward normals and the ribbon's color, and a triangle facing up from the center to each rim vertex and the next. Its first and last rim vertices lie at the ribbon's corners at that end, so the joint and the ribbon never overlap, and a translucent ghost blends once everywhere. Where the ribbon's quads within half its width of p are full-width rectangles along t, as on a straight end, ribbon and joint together cover every ground point within h * cos(π/32) of p, the distance from the joint's center to its nearest rim edge, so ribbons of one width whose ends meet at a point join there with a round corner and no notch, at any angle. A sharp bend closer to p than that narrows the ribbon on its inner side, as any bend does, and can leave a sliver uncovered there.

The appendPath paragraph becomes:

    appendPath adds a path: the ribbon pathWidth(kind) wide along its groundLine at pathLift(kind), in pathColor(kind), then a round joint beyond the line's first point, where t is its first segment's direction reversed, and then one beyond its last point, where t is its last segment's direction. So every path end is round, and path ends meeting at a node join with no notch. An empty ground line adds nothing.

In the walkway paragraph, "appendWalkway adds a walkway: the ribbon pathWidth(kind) wide along the points it is given, in a given color." becomes "appendWalkway adds a walkway: the ribbon pathWidth(kind) wide along the points it is given at pathLift(kind), in a given color.", and the sentence beginning "A walkway ends in a round joint beyond its last point" through "blends once everywhere." becomes:

    A walkway ends in a round joint beyond its last point, where t is its last segment's direction, so it meets its path at any angle, even at the path's end, with no notch.

In src/render/SPEC.md, Ghosts, the appendPath sentence's "appendPath given a color draws the ribbon appendPath draws, in that color" becomes "appendPath given a color draws the ribbon and joints appendPath draws, in that color", and in the buildGhostMesh paragraph "the own ghost of an accepted AddPath has the positions and normals of the ribbon buildParkMesh draws for the path it adds, since a ribbon depends only on its kind and its ground line" becomes "the own ghost of an accepted AddPath has the positions and normals of the ribbon and joints buildParkMesh draws for the path it adds, since they depend only on its kind and its ground line".

The public interface in src/render/park_mesh.h:

    // A path's ribbon lies this far above the ground: guest paths above backstage ones, by as much as
    // backstage ones lie above the terrain, so crossing ribbons of the two kinds never tie in depth.
    constexpr float pathLift(PathKind kind) { return kind == PathKind::Guest ? 0.04f : 0.02f; }

replaces PATH_LIFT, and

    // A round joint beyond a ribbon's end is a half disc of this many segments.
    inline constexpr uint32_t JOINT_SEGMENTS = 16;

replaces WALKWAY_JOINT_SEGMENTS. appendPath's and appendWalkway's comments name the joints and the lift.

## Files affected

- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: src/render/SPEC.md
- Modify (test pass): tests/render/park_mesh_test.cpp, tests/render/ghost_mesh_test.cpp

## Dependencies

- sketch-a-park's park-view: ribbons, appendPath, and the park mesh. Merged.
- sketch-a-park's connector-walkways: appendWalkway, its round joint, and the ghost's walkways. Merged.

## Out of scope

- Miter or bevel joins inside a path, where a sharp bend narrows the ribbon or folds it: park-view left folds out of scope, and the player rarely draws one.
- Ribbons of the same kind overlapping where they cross or meet: they are opaque in one color, so the overlap does not show, and only a translucent ghost crossing a same-kind ghost of itself would blend twice, which a single AddPath does only where its own line crosses itself.
- A path whose whole ground line is shorter than its width, whose two joints overlap each other: rare, and it shows only as a darker translucent ghost.
- Joints only at ends another path shares, with square dead ends: Evan chose round ends everywhere.
- Depth bias or a draw order per kind in the pipelines: the lift settles it in the mesh (RESEARCH.md).

## Open questions

None.
