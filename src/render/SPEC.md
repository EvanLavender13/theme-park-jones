# render

Draws views of the park through SDL_GPU (decision 0009). It reads what it needs to draw and never changes simulation state. Its meshes are built on the CPU, separately from drawing, so what is drawn is a value the app can compare, bound, and reuse.

## Boundary

render reads intent through sim/park's public queries and geometry, the networks through parkNetwork and the Network type's queries, guests through parkGuests and guestRecord, and shops through shopRecord. It depends on no module but sim and core. The band overlay shades by values it is given, which views supplies.

The app calls renderer.h to create the renderer, upload meshes, and draw frames; park_mesh.h for buildParkMesh, buildGhostMesh, appendEntity, meshBounds, and HIGHLIGHT_TINT; guest_mesh.h for buildGuestMesh and appendGuestEntity; graph_overlay.h for buildGraphOverlay, graphColor, and the GRAPH_ drawing constants; band_overlay.h for OVERLAY_BAND, the reach of the food tooltip; and picking.h for groundAtCursor and entityAtCursor.

## The renderer

createRenderer claims the window and sets up the GPU device and Dear ImGui's SDL_GPU backend, which needs an ImGui context to exist already. It logs through SDL and returns false on failure. destroyRenderer releases everything create made, and is safe after a failed create. beginUiFrame starts the renderer's part of each ImGui frame.

World space is right-handed and in meters, with +Y up. The terrain is a flat square of a size given at creation, centered on the origin, with 1 m and 10 m grid lines that fade with distance.

Each of setParkMesh, setGuestMesh, setOverlayMesh, and setGhostMesh replaces the mesh drawn before it, and an empty mesh draws nothing. drawFrame draws one CameraView: the terrain, then the food overlay, then the park mesh, then guests, all lit and depth tested, then ghosts and highlights translucent over them. Paths, boxes, and guests draw over the overlay where they lie above it. Paths never flicker through the terrain, nor guest paths through backstage paths where they cross. A ghost never hides what lies behind it, and a highlight lying exactly on a box or ribbon still shows. ImGui's draw data, when given, draws over the scene. A minimized window skips the frame and counts as success. With a capture path, the frame, UI included, is saved as a BMP before drawFrame returns.

Shaders are GLSL in shaders/, compiled to SPIR-V at build time and loaded from shaders/ next to the executable.

## The park

buildParkMesh gives a world's park as the player sees it: each entrance, path, and box in its place, at its size and height, and a walkway from each door that has a connector to the path it reaches, in its kind's width and color. Guest paths, backstage paths, shops, depots, and the entrance each have their own opaque color. Path ends are round, so paths and walkways meeting at a node join with no notch at any angle, and a walkway starts flush with its door's face. Each starved shop has a mark floating over its roof, in a color unlike any other in the scene.

meshBounds gives the least rectangle on the ground holding a mesh's vertices, or none for an empty mesh.

## Ghosts and highlights

A ghost shows what an edit would do before it is committed, drawn by the same code as the committed park, so an accepted edit's ghost has exactly the shape the park will have once the edit commits. buildGhostMesh(world, edit, candidate) gives the edit's ghost: an added or moved box or added path, translucent in its kind's color, or in an invalid tint when the world refuses the edit, and the entity an accepted delete would remove, in a delete tint. With the candidate world, the ghost also shows the candidate's walkways and starved marks. Where one lies on a committed one it blends into it, so only the walkways the edit adds or moves, and the shops it would starve, stand out. Hovering a backstage path with the delete tool marks each shop the deletion leaves with no depot. buildGhostMesh(world, edit) makes the candidate itself, so a caller already holding one passes it instead.

appendEntity adds the box or path a key holds, shaped as the park draws it, in a given color, such as HIGHLIGHT_TINT, and nothing when the key holds neither.

## Guests

buildGuestMesh draws each guest that has a position as an upright box where it stands, shaded from blue when sated to pink when hungry. appendGuestEntity adds the box of one guest, where buildGuestMesh draws it, in a given color, and nothing when the guest has no position.

## Band overlay

buildBandOverlay(world, values, full) draws a band along each guest path, shaded at each place by the value values gives for it, from gray for none through a perceptually uniform, colorblind-safe ramp from dark purple to yellow, which reaches yellow at full. It calls values once per build, with every place it shades, so whatever computes the values sees the whole build at once. A color means the same value in every park for the same full, and any positive value is distinguishable from none. The band covers the ground within OVERLAY_BAND of each guest path, lies over the terrain and under every path, and rises toward its own path, so where bands overlap the nearest path's value shows. Connectors and backstage paths have no band.

## Picking

groundAtCursor gives the point on the ground under a cursor for a camera view, or none when the eye is not above the ground or the ray does not descend. The ray through the cursor matches drawFrame's projection, so the point picked is the point drawn under the cursor.

entityAtCursor gives the key of the nearest entrance, box, or guest the cursor's ray meets, shaped as the park and guest meshes draw them, or none. Paths, walkways, and starved marks do not stop the ray, so a click meets what stands on the ground.

## Graph overlay

buildGraphOverlay gives both networks' carriers and nodes as lines and points in window coordinates for a camera view and window size, each naming its kind, with connectors marked and each node's anchor given. Lines are clipped at the near plane, so none runs to a point behind the camera, and nodes behind it are left out. A window with no area gives an empty overlay. The app draws lines in graphColor of their kind or the connector color, and nodes as circles, with anchored nodes set apart. The graph colors are opaque and differ from each other and from the path colors, so the graph stands out over the ribbons it follows.
