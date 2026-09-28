# Feature: Graph View

## Summary

The app can draw the park's guest and backstage networks over the scene, to check what resolution derived from the drawn paths. A new part of the renderer, render/graph_overlay.h, builds the overlay on the CPU: for a world, a camera view, and a window size, the window positions of every carrier segment and every node of both networks, projected as drawFrame projects the ground, with segments that pass behind the camera clipped to the near plane. The Debug panel gains a Graph checkbox, and --graph checks it at start. While it is checked, the app draws the overlay on ImGui's background draw list each frame, edges in their kind's graph color and nodes as filled circles, so a --capture of tests/parks/routes.park with --graph shows its networks.

## Acceptance criteria

1. For a camera view whose eye is above the ground, a window size, and a ground point with positive view depth, windowPoint gives a position that, turned back into normalized device coordinates, gives the ground point back from groundAtCursor with the window's aspect ratio.
2. windowPoint gives none exactly when the point's view depth is not positive or the window's width or height is not positive.
3. For a world, a view whose NearZ is positive, and a window of positive size, buildGraphOverlay has exactly one line for each segment of each carrier of parkNetwork(world, kind), for both kinds, that has an end with view depth at least the view's NearZ, naming that kind, carrier, and segment, and no other lines.
4. For such a view, each line's ends are windowPoint of its segment's ends, where an end with view depth below NearZ is replaced by the segment's point at view depth NearZ.
5. For such a view, buildGraphOverlay has exactly one node for each node of each kind's network whose ground position, groundPoint of its nodePlace, has view depth at least NearZ, naming that kind and node, at windowPoint of that position, and no other nodes.
6. graphColor of each kind and GRAPH_NODE_COLOR are opaque, and differ in red, green, or blue from each other and from pathColor of both kinds.
7. A command line with both --graph and --hash, in either order, prints the usage and exits with a nonzero status and no hash line.
8. In the running app, the Debug panel's Graph checkbox is off at start unless --graph is given, and while it is checked the overlay is drawn over the scene, behind the panels. A --capture of tests/parks/routes.park with --graph shows both networks' edges and nodes over their paths. This is checked by looking at the capture, not by a test.

## Medium

None sampled or emitted. The overlay reads the guest and backstage networks path-networks derives, through parkNetwork (sim/routes/SPEC.md) and the Network type's public queries, carriers, nodeCount, nodePlace, and groundPoint (sim/medium/SPEC.md). It draws nothing into the simulation.

## Principle checks

- Principle 6: the overlay reads the networks only through parkNetwork and Network's public queries, never how path-networks derived them. The private header check enforces that render includes only public headers.
- Principle 8: the overlay changes nothing. buildGraphOverlay takes the world as const, so drawing it never changes the world or its hash.
- Principles 1, 2, 3, 4, 5, and 10 do not apply: the feature adds no simulation state, flow, rule, or draw, and runs only in the app.

## Spec changes

In src/render/SPEC.md, add after the Ghosts section:

    ## Graph overlay

    graph_overlay.h builds the graph view's overlay on the CPU, so it can be tested without a GPU. It reads the networks only through parkNetwork (sim/routes/SPEC.md) and the Network type's public queries (sim/medium/SPEC.md), and changes nothing.

    A ground point's view depth, for a CameraView, is dot(p - Eye, f), where p is the point at height 0 and f the unit direction from Eye to Target. windowPoint gives a ground point's position in a window of a width and height, measured in the same units from the window's top left corner: when drawFrame's projection, multiply(perspective(FovY, width / height, NearZ, FarZ), lookAt(Eye, Target, +Y)), takes p to normalized device coordinates x and y, the position is ((x + 1) / 2 * width, (1 - y) / 2 * height). It gives none when the view depth is not positive, or the width or height is not. So for an eye above the ground, groundAtCursor at a window point's normalized device coordinates, with the aspect ratio width / height, gives the ground point back.

    buildGraphOverlay gives the overlay of a world's networks for a CameraView whose NearZ is positive, as drawFrame's perspective needs, and a window's width and height: lines and nodes for both kinds, each GraphLine and GraphNode naming its kind. A carrier's segment runs from its point at an index, which the line names as its segment, to the next point. A segment whose two ends both have view depth below NearZ has no line, since the part of it in front of the near plane is empty. Every other segment has one line, from windowPoint of its first end to windowPoint of its second, where an end a with view depth d_a below NearZ, whose other end b has view depth d_b, is replaced by a + t * (b - a), with t = (NearZ - d_a) / (d_b - d_a), the segment's point at view depth NearZ. So a line never runs to a point behind the camera. Each node whose ground position, groundPoint of its nodePlace, has view depth at least NearZ has one GraphNode at that position's windowPoint. A width or height that is not positive gives an empty overlay.

    The app draws lines in graphColor(kind), GRAPH_LINE_THICKNESS thick, and nodes as filled circles of GRAPH_NODE_RADIUS in GRAPH_NODE_COLOR, both in window units. The graph colors are opaque, and differ from each other and from both kinds' pathColor, so the graph stands out over the ribbons it follows.

In src/app/SPEC.md, the Tooling UI section becomes:

    A Debug panel shows the frame rate, the simulation tick, and the camera focus and distance, and a Graph checkbox, off at start unless --graph is given. While it is checked, each frame, after the panels are built, the app draws buildGraphOverlay for the world, the frame's CameraView, and ImGui's display size on ImGui's background draw list, which lies over the scene and behind every panel: each line in graphColor of its kind, GRAPH_LINE_THICKNESS thick, and then each node as a filled circle of GRAPH_NODE_RADIUS in GRAPH_NODE_COLOR (render/SPEC.md). ImGui docking is enabled.

In src/app/SPEC.md's Command line section, after the paragraph on --frames and --capture, add:

    --graph checks the Debug panel's Graph checkbox at start, so a capture shows the networks.

and in its last paragraph, "or --hash given with --frames or --capture, whatever their values," becomes "or --hash given with --frames, --capture, or --graph, whatever their values,".

## Files affected

- Create: `src/render/graph_overlay.h`
- Create: `src/render/graph_overlay.cpp`
- Modify: `src/render/CMakeLists.txt`
- Modify: `src/render/SPEC.md`
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`
- Modify: `src/app/main.cpp`
- Modify: `src/app/SPEC.md`
- Tests from the test pass, under `tests/render/` and `tests/app/`, with `tests/render/CMakeLists.txt`.

## Dependencies

- path-networks: parkNetwork and the networks it derives, and tests/parks/routes.park.
- first-field-and-flow: the Network type and its public queries.
- sketch-a-park: groundAtCursor, pathColor, CameraView, --capture, and the Debug panel.

## Out of scope

- Marking connectors and anchored nodes, which box-connections adds.
- Route distances in the view, a milestone deepening candidate.
- Drawing the graph with depth, so boxes hide it. The view is tooling, and seeing a line through a box is the point.
- Clipping against the sides of the window or the far plane. ImGui clips to the display, and the park lies well inside the far plane.
- A test that --graph is accepted without --hash. Running the app with it opens a window, so criterion 8's capture checks it, and criterion 7's test cannot tell a refusal of the pair from a refusal of an unknown option.
- Caching the overlay between frames. It is rebuilt each frame the checkbox is on, since the camera moves.

## Open questions

None.
