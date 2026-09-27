# Research: effortless-building

## Which curve should a clicked path follow?

A path that passes through the points the player clicks is an interpolating spline, and Catmull-Rom is the standard family. How it spaces its parameter decides its behavior:

- Uniform spacing can form loops and cusps within a segment when the points are unevenly spaced, which is the normal case for clicks.
- Chordal spacing stays close to short chords but swings far from long ones.
- Centripetal spacing, the square root of the chord length, is proven to be the only one of the three that never forms cusps or self-intersections within a segment. It also stays at a bounded distance from the lines between the points, so it goes roughly where the player pointed.

The cost is a few square roots per segment, which are exact basic operations and deterministic (plans/deterministic-simulation/RESEARCH.md).

Cities: Skylines builds roads from Bézier segments between graph nodes, and snaps to existing roads, angles, and guides. Its sequel offers simple, complex, and continuous curve modes, which are different ways of placing Bézier control points. Bézier control points do not lie on the curve, so the player shapes the road indirectly. That suits a road tool with handles, and suits click-to-draw less.

Because the path's curve is intent, it is saved as the clicked points, never as sampled geometry (principle 1). Everything else, including arc length, sample points, the tube mesh, and the network graph, is derived by whoever needs it. navigable-networks samples the same curve through a shared evaluation function, so the rendered tube and the walked route agree.

Rejected:

- Uniform Catmull-Rom: loops and cusps on unevenly spaced clicks.
- Cubic Bézier with handles as the drawing model: the player places points that are not on the path, which is less direct for click-to-draw. It remains possible later as an editing refinement.
- Saving sampled polylines: that is derived geometry, and principle 1 keeps it out of the save.

Sources:

- https://en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline: definition, and the no-cusp, no-self-intersection property.
- https://splines.readthedocs.io/en/latest/euclidean/catmull-rom-properties.html: comparison of uniform, centripetal, and chordal behavior.
- https://www.sciencedirect.com/science/article/abs/pii/S0010448510001533: Yuksel, Schaefer and Keyser, the proof and its bounds.
- https://skylines.paradoxwikis.com/Roads: road snapping in Cities: Skylines.
- https://www.paradoxinteractive.com/games/cities-skylines-ii/features/road-tools: the curve tool modes of Cities: Skylines II.

## How do gridless builders make placement feel effortless?

Tiny Glade is the reference for dabbling. It lays out no grid and asks for little precision, and hand-authored procedural rules react to simple strokes: a path drawn to a wall makes an archway, a path over water becomes a bridge, a window at ground level becomes a door. The player never places those details. The game grew out of a procedural wall generator, and its developers were surprised how much people enjoyed simply drawing walls. Direct, forgiving strokes with rich derived results are the draw.

For this project, the foundation's derived results are modest: tubes from curves, and connections and junctions that navigable-networks derives. But the same stance applies. The player expresses a stroke or a box, and everything that can be inferred is inferred and previewed live, not placed by hand. Snapping serves that too: an endpoint that lands on an existing path snaps onto it, so the junction the player meant is the junction that is derived.

Rejected:

- Grid-based placement with footprints on cells: it contradicts the gridless vision (docs/vision.md) and principle 4's routes-first distance.

Sources:

- https://tinyglade.wiki.gg/wiki/Gridless_building: gridless building, and the derived reactions to paths and walls.
- https://80.lv/articles/exclusive-tiny-glade-developers-discuss-bevy-proceduralism-publishers-cozy-games: the origin in a procedural wall generator, and why drawing is the fun.

## How should a path render as a tube?

A tube is a circular cross-section swept along the curve, oriented at each sample by a frame. Frenet frames flip and twist where the curve straightens or bends the other way. Rotation-minimizing frames, also called parallel-transport or Bishop frames, carry the orientation along the curve with no twist and are stable through inflections.

On the foundation's flat terrain, the ground normal gives a trivially stable frame, so the renderer can use up and the curve's tangent directly. Rotation-minimizing frames become necessary once paths climb terrain or ramps.

The tube mesh is presentation derived from intent, built by the renderer or a mesh builder outside tpj_sim. It never feeds back into the simulation (principles 1 and 10).

Rejected:

- Frenet frames: twist and flip at inflections.
- Storing meshes in the save: they are derived (principle 1).

Sources:

- https://janakiev.com/blog/framing-parametric-curves/: Frenet frames against parallel-transport frames.
- https://www.semanticscholar.org/paper/Parallel-Transport-Approach-to-Curve-Framing-Hanson-Ma/ed416d01742e5e704357538c6817312ca6d8cb38: Hanson and Ma, parallel-transport framing for tubes.

## How should tools change the park?

A tool should never write to the world directly. It builds a tentative edit, which the ghost previews through a candidate copy of the world (decision 0025). On commit, it submits the edit as a command, and the simulation applies it between ticks, where resolution runs (plans/shared-medium/CAPABILITY.md). This is the Command pattern: an edit becomes a value that can be applied, previewed, logged, and later undone or replayed. That matches deterministic-simulation's replay candidate, which is a seed, a starting save, and a log of intent changes.

Rejected:

- Tools mutating world state from the input handler: edits would land mid-tick, depend on frame timing (principle 10), and leave nothing to preview or replay.

Sources:

- https://gameprogrammingpatterns.com/command.html: commands as values, for undo and replay.
