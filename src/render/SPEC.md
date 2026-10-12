# render

Draws views of the park through SDL_GPU (decision 0009). It reads what it needs to draw and never changes simulation state. It builds its meshes on the CPU, separately from drawing, so what is drawn is a value the app can compare, bound, and reuse.

## Boundary

render reads:

- intent through sim/park's public queries and geometry
- the networks through parkNetwork, connectorWalkways, and the Network type's queries
- guests through parkGuests and guestRecord
- shops through shopRecord
- the furniture through parkFurniture and seatPose
- the time of day through parkTime

It depends on no module but sim and core. The paving overlay shades by values it is given, and views supplies them.

The app calls these headers:

- renderer.h to create the renderer, upload meshes, and draw frames
- park_mesh.h for buildParkMesh, buildGhostMesh, GhostMesh, appendEntity, appendHandle, appendSides, meshBounds, HIGHLIGHT_TINT, HANDLE_COLOR, EDGE_HANDLE_COLOR, and TAPER_HANDLE_COLOR
- guest_mesh.h for buildGuestMesh, appendGuestEntity, and SeatedGuests
- day_light.h for DayLight, dayLightAt, and dayLightOf
- graph_overlay.h for buildGraphOverlay, graphColor, and the GRAPH_ drawing constants
- paving_overlay.h for buildOverlayShape, overlayPlace, and OVERLAY_HEIGHT
- picking.h for groundAtCursor, pointAtCursor, and entityAtCursor

views calls paving_overlay.h for shadeOverlay.

## The renderer

createRenderer claims the window and sets up the GPU device and Dear ImGui's SDL_GPU backend. That backend needs an ImGui context to exist already. createRenderer logs through SDL and returns false on failure. destroyRenderer releases everything createRenderer made, and is safe to call after a failed create. beginUiFrame starts the renderer's part of each ImGui frame.

World space is right-handed and in meters, with +Y up. The terrain is a flat square of a size given at creation, centered on the origin, with 1 m and 10 m grid lines that fade with distance.

Each of setParkMesh, setGuestMesh, and setGhostMesh replaces the mesh set by its previous call, and setOverlayShape and setOverlayShades replace the paving overlay's shape and shades. If a mesh or the overlay's shape is empty, nothing is drawn for it. setGhostMesh takes a GhostMesh and replaces all three of its parts.

drawFrame draws one CameraView under a DayLight. It draws the terrain, the park mesh, and the guests, lit by that light, then the paving overlay, then a ghost's tints, its preview of the park lit as the park is, and its marks, each translucent over the one before, all depth tested. The light never changes the overlay's colors or the marks' colors, apart from the distance fade they share with the scene. The overlay draws over the guest paving and under its edge lines, and boxes and guests draw over it. Paths never flicker through the terrain, and guest paths never flicker through backstage paths where they cross. A ghost never hides what lies behind it. If a highlight lies exactly on a box or ribbon, it still shows. When ImGui's draw data is given, it draws over the scene. If the window is minimized, drawFrame skips the frame, and that counts as success. If the window is hidden, drawFrame draws the frame at the window's size without showing it, so a capture still holds it. With a capture path, drawFrame saves the frame, UI included, as a BMP before it returns.

Shaders are GLSL in shaders/. They are compiled to SPIR-V at build time and loaded from shaders/ next to the executable.

## The light

dayLightAt gives the light at a second of the day, and dayLightOf gives it at a world's time of day (sim/SPEC.md, The clock). The light depends on the second of the day alone. A world at a given tick is lit the same at any speed and after a save and load.

The light has a sun, a moon, and an ambient light. The ambient light mixes a sky color from above and a ground color from below by the way a surface faces. The frame's sky is the hour's sky color, and the ground, paths, boxes, guests, the overlay, ghosts, and marks fade to it with distance.

The sun rises at SUNRISE_SECOND on the +X side and sets at SUNSET_SECOND on the −X side, passing over the +Z side. It is never straight overhead, so a box's faces stay distinct at noon. Its light warms toward evening and fades out at the horizon. From dusk to dawn a dim, cool moon light shines from high in the sky. No light comes from below the ground. At night the ambient light keeps every kind of box, both kinds of path, and the ground distinguishable. The light changes continuously through the whole day, across midnight too.

The scene is lit in linear light and tonemapped to the screen. The tonemap keeps the hue of every color it is given. Guests take the light's brightness but not its color, so their need shading keeps its hue at every hour.

## The park

buildParkMesh gives a world's park as the player sees it. Each entrance, path, and box is in its place, at its size and height. If a door has a connector, its walkway is drawn as part of the paved surface, joined to the segment it reaches (connectorWalkways). Guest paths, backstage paths, shops, depots, benches, toilet blocks, and the entrance each have their own opaque color. Paths draw as the paved surface parkSurface gives: each piece filled in its kind's color, and a strip EDGE_LINE_WIDTH wide along each edge line, on the paving's side, in a darker shade of that color, above the fills. Where an edge line turns sharply, its strip has no gap. Each piece of furniture is drawn as a low box over its footprint, benches and tables each in their own color. Each starved shop has a mark floating over its roof, in a color unlike any other in the scene.

meshBounds gives the smallest rectangle on the ground that holds a mesh's vertices, or none for an empty mesh.

## Ghosts and highlights

A ghost shows what an edit would do before it is committed. It is drawn by the same code as the committed park, so if the world accepts an edit, its ghost has exactly the shape the park will have once the edit commits.

buildGhostMesh(world, edit, candidate) gives the edit's ghost in three parts. Its Tints part holds the invalid and delete tints, under the rest. Its Lit part is what the park would look like, drawn in kinds' colors and lit as the park is. Its Marks part holds the marks, over the rest. Tints and marks keep their colors at every hour, apart from the distance fade:

- for every edit but a DeleteSegment, every piece and edge line of the surface the edit adds or changes, as editedSurface gives them and drawn as the park draws them, with the walkways of the segments they belong to joined as the park joins them when the world accepts the edit, and an added box or a moved box, translucent in its kind's color, or in an invalid tint when the world refuses the edit. If the world refuses an added path so that editedSurface gives none, the ghost shows the straight segments between its stops instead.
- a mark at each stop where an added path joins a point or segment, at a moved point's target when it is a point or segment, at the place where an inserted point goes, and at each place pathCrossings gives, in JOIN_MARK_COLOR
- the new sides of a segment whose width the edit sets, in JOIN_MARK_COLOR, so a narrowed segment shows inside the wider one beneath it
- the box or segment an accepted delete would remove, or both segments at a point an accepted RemovePoint merges, in a delete tint, under the segments the edit reshapes. A DeleteSegment's ghost is this tint, in the invalid tint when the world refuses it, the paving of each segment whose ground line the delete changes, as editedSegments gives them, with its walkways joined as the park joins them when the world accepts the delete, and the candidate's walkways and starved marks.

Whatever the edit adds or changes in a kind's color, the candidate's walkways, and its starved marks are in the Lit part. The invalid and delete tints are in the Tints part, and the join marks and sides in JOIN_MARK_COLOR are in the Marks part.

With the candidate world, the ghost also shows the candidate's walkways as strips over the paving, and its starved marks. Where one of these lies on a committed one, it blends into it. Only the walkways the edit adds or moves, and the shops it would starve, stand out. With the candidate world, the ghost also shows each piece of the candidate's furniture that the edit adds, moves, or changes the kind of, translucent in its kind's color, in the Lit part, and each piece of the world's furniture that the edit removes, moves, or changes the kind of, where it stands now, in the delete tint, in the Tints part. When the delete tool hovers a backstage segment, the ghost marks each shop the deletion leaves with no depot. buildGhostMesh(world, edit) makes the candidate itself. If a caller already holds the candidate, the caller passes it instead.

appendPathStart shows where a path would start before its first click: a translucent disc of the path's width, with a join mark when the click would join a point or segment. appendFinishMark marks the stop where a click would finish the path, in FINISH_MARK_COLOR, wider than a join mark and under it. Ghost and highlighted segments, and the candidate's walkways in a ghost, lie GHOST_LIFT above the park's paths, so they never flicker against the paths beneath them. appendEntity adds the box, segment, or path point a key holds, in a given color such as HIGHLIGHT_TINT. A box is shaped as the park draws it, a segment as its own pieces of the surface, which are its body and its halves of the corners at its ends, and a path point is a flat disc of JOIN_MARK_RADIUS. It adds nothing when the key holds none of these.

appendHandle adds a radius, edge, or taper handle as a flat disc of RADIUS_HANDLE_SIZE, in a given color, above join marks. appendPointMark adds a path point's marker as a flat disc of POINT_MARK_SIZE, in a given color, under handles and above join marks. appendSides adds a strip along the inside of each of a segment's two sides, in a given color, above the segment's ghost. It adds nothing when the key holds no segment with a ground line.

## Guests

buildGuestMesh draws each guest that has a position. A guest is drawn as an upright box where it stands, or a box GUEST_SEATED_HEIGHT tall on its seat while it is seated. The box is shaded from blue when every need of the guest is at 0 to pink when its most pressing need is at 1. appendGuestEntity adds one guest's box, where buildGuestMesh draws it, in a given color. It adds nothing when the guest has no position. The light dims a guest's box but never changes its hue (The light).

SeatedGuests keeps the seat each seated guest is drawn on, from one update to the next. A guest is drawn seated when its record gives a seat place. It keeps its seat while it stays seated at the same piece, whoever else sits down or stands up there. Guests newly seated take the lowest free seat of their piece, in key order, with the seats seatPose gives. If no seat of its piece is free, a guest is drawn standing. The seats are drawing only, and no save holds them. After a park opens, its first update seats guests in key order.

buildGuestMesh can also fill a GuestMeshLists that the caller keeps. It clears the lists first, and allocates nothing once they have grown. Bringing SeatedGuests up to date allocates nothing once its own lists have grown.

## Paving overlay

buildOverlayShape(world) gives the guest paving's overlay shape: each piece of the paved surface that belongs to a guest segment or to a point where guest segments meet, without door walkways, the lines of the guest segments, and which pieces of different owners overlap. It depends only on the park's path points and segments, so it changes only when they do.

Each spot of the shape has an overlay place, which overlayPlace(shape, point) gives, or none when the shape holds no piece there. A spot of a segment's paving takes the place on that segment's line nearest it. A spot of a point's paving takes the nearest place on the lines of the guest segments at that point. Where paving of several owners holds a spot, the nearest of their places wins, and ties go to the lower segment key.

shadeOverlay(shape, places, values, half) gives the shades: for each of the shape's lines, the values at the given places on it, in ascending distance. Between two consecutive places on a line, a value blends linearly. The renderer colors each pixel of the shape by the value at its overlay place, in the same colors at every hour, apart from the distance fade: a perceptually uniform, colorblind-safe ramp from dark purple at 0, the worst, to yellow. A value v shades v / (v + half) of the way up the ramp, so `half` shades its middle, larger values approach yellow, and no value clips. For the same `half`, a color means the same value in every park. Every color of the ramp is distinguishable from both kinds of paving. The renderer finds each pixel's place in single precision, so a pixel within a millimeter of a tie between two lines may take either, and a pixel on the outline of an overlap may take the other piece's place.

The shading lies OVERLAY_HEIGHT above the ground: over the guest paving and under its edge lines, ghosts, boxes, and guests. The ground beside paths, door walkways, and backstage paths are not shaded.

## Picking

groundAtCursor gives the point on the ground under a cursor for a camera view. It gives none when the eye is not above the ground or the ray does not descend. The ray through the cursor matches drawFrame's projection, so the point picked is the point drawn under the cursor.

pointAtCursor gives the point at a height above the ground under a cursor, where its ray meets that level. It gives none when the eye is not above that level or the ray does not descend. groundAtCursor is pointAtCursor at height 0.

entityAtCursor gives the key of the nearest entrance, box, or guest that the cursor's ray meets, or none. It uses the shapes the park and guest meshes draw, with the same seated guests. Paths, walkways, and starved marks do not stop the ray, so a click meets what stands on the ground.

## Graph overlay

buildGraphOverlay gives both networks' carriers and nodes as lines and points in window coordinates, for a camera view and window size. Each line and point names its kind. Connectors are marked, and each node's anchor is given. Lines are clipped at the near plane, so none runs to a point behind the camera. Nodes behind the camera are left out. If the window has no area, the overlay is empty.

The app draws lines in graphColor of their kind or in the connector color. It draws nodes as circles, with anchored nodes set apart. The graph colors are opaque and differ from each other and from the path colors, so the graph stands out over the ribbons it follows.
