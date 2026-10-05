# render

Draws views of the park through SDL_GPU (decision 0009). It reads what it needs to draw and never changes simulation state. It builds its meshes on the CPU, separately from drawing, so what is drawn is a value the app can compare, bound, and reuse.

## Boundary

render reads:

- intent through sim/park's public queries and geometry
- the networks through parkNetwork and the Network type's queries
- guests through parkGuests and guestRecord
- shops through shopRecord

It depends on no module but sim and core. The band overlay shades by values it is given, and views supplies them.

The app calls these headers:

- renderer.h to create the renderer, upload meshes, and draw frames
- park_mesh.h for buildParkMesh, buildGhostMesh, appendEntity, appendRadiusHandle, appendSides, meshBounds, HIGHLIGHT_TINT, and HANDLE_COLOR
- guest_mesh.h for buildGuestMesh and appendGuestEntity
- graph_overlay.h for buildGraphOverlay, graphColor, and the GRAPH_ drawing constants
- band_overlay.h for OVERLAY_BAND, the reach of the food tooltip
- picking.h for groundAtCursor and entityAtCursor

## The renderer

createRenderer claims the window and sets up the GPU device and Dear ImGui's SDL_GPU backend. That backend needs an ImGui context to exist already. createRenderer logs through SDL and returns false on failure. destroyRenderer releases everything createRenderer made, and is safe to call after a failed create. beginUiFrame starts the renderer's part of each ImGui frame.

World space is right-handed and in meters, with +Y up. The terrain is a flat square of a size given at creation, centered on the origin, with 1 m and 10 m grid lines that fade with distance.

Each of setParkMesh, setGuestMesh, setOverlayMesh, and setGhostMesh replaces the mesh set by its previous call. If a mesh is empty, nothing is drawn for it.

drawFrame draws one CameraView. It draws the terrain, then the food overlay, then the park mesh, then guests, all lit and depth tested. Then it draws ghosts and highlights translucent over them. Paths, boxes, and guests draw over the overlay where they lie above it. Paths never flicker through the terrain, and guest paths never flicker through backstage paths where they cross. A ghost never hides what lies behind it. If a highlight lies exactly on a box or ribbon, it still shows. When ImGui's draw data is given, it draws over the scene. If the window is minimized, drawFrame skips the frame, and that counts as success. With a capture path, drawFrame saves the frame, UI included, as a BMP before it returns.

Shaders are GLSL in shaders/. They are compiled to SPIR-V at build time and loaded from shaders/ next to the executable.

## The park

buildParkMesh gives a world's park as the player sees it. Each entrance, path, and box is in its place, at its size and height. If a door has a connector, the door gets a walkway to the path it reaches, in its kind's width and color. Guest paths, backstage paths, shops, depots, and the entrance each have their own opaque color. Path ends are round, so paths and walkways meeting at a node join with no notch at any angle. A walkway starts flush with its door's face. Each starved shop has a mark floating over its roof, in a color unlike any other in the scene.

meshBounds gives the smallest rectangle on the ground that holds a mesh's vertices, or none for an empty mesh.

## Ghosts and highlights

A ghost shows what an edit would do before it is committed. It is drawn by the same code as the committed park, so if the world accepts an edit, its ghost has exactly the shape the park will have once the edit commits.

buildGhostMesh(world, edit, candidate) gives the edit's ghost:

- every segment the edit adds, or whose ground line or width it changes, as editedSegments gives it, and an added box or a moved box, translucent in its kind's color, or in an invalid tint when the world refuses the edit. If the world refuses an added path so that editedSegments gives none, the ghost shows the straight segments between its stops instead.
- a mark at each stop where an added path joins a point or segment, at a moved point's target when it is a point or segment, at the place where an inserted point goes, and at each place pathCrossings gives, in JOIN_MARK_COLOR
- the new sides of a segment whose width the edit sets, in JOIN_MARK_COLOR, so a narrowed segment shows inside the wider one beneath it
- the entity an accepted delete would remove, or every segment at a point an accepted DeletePoint deletes, in a delete tint, under the segments the deletion reshapes

With the candidate world, the ghost also shows the candidate's walkways and starved marks. Where one of these lies on a committed one, it blends into it. Only the walkways the edit adds or moves, and the shops it would starve, stand out. When the delete tool hovers a backstage segment, the ghost marks each shop the deletion leaves with no depot. buildGhostMesh(world, edit) makes the candidate itself. If a caller already holds the candidate, the caller passes it instead.

appendPathStart shows where a path would start before its first click: a translucent disc of the path's width, with a join mark when the click would join a point or segment. appendFinishMark marks the stop where a click would finish the path, in FINISH_MARK_COLOR, wider than a join mark and under it. Ghost and highlighted segments lie GHOST_LIFT above the park's paths, so they never flicker against the paths beneath them. appendEntity adds the box, segment, or path point a key holds, in a given color such as HIGHLIGHT_TINT. A box or segment is shaped as the park draws it, and a path point is a flat disc of JOIN_MARK_RADIUS. It adds nothing when the key holds none of these.

appendRadiusHandle adds a radius handle as a flat disc of RADIUS_HANDLE_SIZE, in a given color, above join marks. appendSides adds a strip along the inside of each of a segment's two sides, in a given color, above the segment's ghost. It adds nothing when the key holds no segment with a ground line.

## Guests

buildGuestMesh draws each guest that has a position as an upright box where it stands. The box is shaded from blue when the guest is sated to pink when it is hungry. appendGuestEntity adds one guest's box, where buildGuestMesh draws it, in a given color. It adds nothing when the guest has no position.

## Band overlay

buildBandOverlay(world, values, full) draws a band along each guest path. Each place on the band is shaded by the value that values gives for it. The shading runs from gray for none, through a perceptually uniform, colorblind-safe ramp from dark purple to yellow, and reaches yellow at full. It calls values once per build, with every place it shades, so whatever computes the values sees the whole build at once.

For the same `full`, a color means the same value in every park. The ramp keeps every positive value distinguishable from none. The band covers the ground within OVERLAY_BAND of each guest path. It lies over the terrain and under every path. Each band sits slightly higher near its own path, so where bands overlap, the nearest path's value is on top. Connectors and backstage paths have no band.

## Picking

groundAtCursor gives the point on the ground under a cursor for a camera view. It gives none when the eye is not above the ground or the ray does not descend. The ray through the cursor matches drawFrame's projection, so the point picked is the point drawn under the cursor.

entityAtCursor gives the key of the nearest entrance, box, or guest that the cursor's ray meets, or none. It uses the shapes the park and guest meshes draw. Paths, walkways, and starved marks do not stop the ray, so a click meets what stands on the ground.

## Graph overlay

buildGraphOverlay gives both networks' carriers and nodes as lines and points in window coordinates, for a camera view and window size. Each line and point names its kind. Connectors are marked, and each node's anchor is given. Lines are clipped at the near plane, so none runs to a point behind the camera. Nodes behind the camera are left out. If the window has no area, the overlay is empty.

The app draws lines in graphColor of their kind or in the connector color. It draws nodes as circles, with anchored nodes set apart. The graph colors are opaque and differ from each other and from the path colors, so the graph stands out over the ribbons it follows.
