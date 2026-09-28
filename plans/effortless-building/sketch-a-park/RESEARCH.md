# Research: sketch-a-park

## How is a box's facing stored when the simulation cannot call sin and cos?

Decision 0022's denylist keeps sin, cos, and atan2 out of tpj_sim, and the physical-validity check runs in the simulation, because a commit is refused when the command is applied. A facing saved as an angle would need sin and cos to place a footprint's corners. A facing saved as a direction on the ground, two doubles, needs none. Intent keeps the direction exactly as the command gave it, and geometry normalizes it with sqrt wherever it is used, so the committed intent equals the command bit for bit and the ghost can show the command's own value. A zero or non-finite direction describes no box, and the command is refused like any other physically impossible one. Corners, the front and back faces navigable-networks connects from, and the box's rendering all come from the direction and the footprint with basic operations only. It also leaves rotation free, as the gridless vision asks. The place tool sets the direction by dragging from where the box lands, and computes it in the app, outside the simulation.

Rejected: an angle with a port of musl's sin and cos, which is more ported code for a value nothing else needs as an angle. Facing quantized to a fixed set of steps with a table of exact sines, which puts a grid back into rotation.

## How can the physical-validity check be exact on a curve?

The capability asks for bounds to be checked on the whole curve, since a curve swings between its points, and for paths to be refused when they pass through a box. Exact tests against the cubic itself need root finding for every box. A simpler route with no approximation to argue about is to define one canonical ground line for each path: the curve sampled by a fixed rule that depends only on the clicked points, with the path's width around it. The check, the tube the renderer sweeps, and the carrier navigable-networks builds can all use that same line, so what is refused is exactly what would be drawn and walked. The check is then plain 2D geometry. A path passes through a box when some segment of its ground line comes within half the path's width of the box's rectangle. A path stays in bounds when every point of its ground line lies at least half its width inside the park's square.

Two boxes overlap when no separating axis exists between their rectangles. For two rectangles only four axes need testing, the two edge directions of each, and each test projects both rectangles' corners, which is basic arithmetic. The distance from a segment to a rectangle is zero when the segment crosses it, and otherwise the least of the distances from the segment's ends to the rectangle and from the rectangle's corners to the segment.

Rejected: sampling the curve at a fixed count per segment regardless of its length, which is coarse on long segments. Bounding the curve by its Bézier control polygon, which is conservative and would refuse valid paths, a gate that principle 5 forbids.

## How does an open Catmull-Rom path begin and end?

A Catmull-Rom segment needs a point on either side of it, so an open path through clicked points needs a phantom point before its first and after its last. The usual choice reflects the neighbor through the end point, which makes the end segment continue straight out of its neighbor. A centripetal parameterization divides by the distances between consecutive points, so repeated consecutive points must not reach it: the command drops a point equal to the one before it, and a path left with fewer than two distinct points is not a path.

Rejected: duplicating the end point as its own phantom, which gives a zero chord and so a division by zero under the centripetal parameterization.

## How does the app open and save park files?

SDL3 has native open and save dialogs. SDL_ShowOpenFileDialog and SDL_ShowSaveFileDialog return at once and later call back with the chosen paths, an empty list when the player cancels, or null on an error. The callback may run on another thread, so it should only hand the path to the main loop, which reads or writes the file between frames. Loading replaces the world and its command queue, and the park file itself is saveWorld's text, so the app adds nothing to the encoding. The --park option takes the same path without a dialog.

Rejected: a path typed into an ImGui text field, which is enough for tooling but is worse than the native dialog SDL already provides at no cost.

## How do tools stay testable?

The tools belong outside tpj_sim (principle 10), but principle 8 asks for a test that a commit derives what its ghost showed. If a tool's logic takes ground positions and button presses, not SDL events, it can live in a library that links tpj_sim alone, and tests can drive it. The tool produces a tentative command, a value. The ghost is that command's intent drawn with its validity, and committing queues the same value, so the only thing to prove is that validity before the commit equals acceptance at it. The app turns the mouse into a ground position by casting a ray from the camera through the cursor and intersecting it with the ground plane.

## How are ghosts drawn distinctly?

A ghost is drawn after the opaque scene, with alpha blending and a depth test that does not write depth, so it shows through where it overlaps committed geometry and never hides it. An invalid ghost takes a different tint. The same pass can draw the delete tool's highlight over the hovered path or box. SDL_GPU sets these per pipeline, through a color target's blend state and the depth-stencil state's write flag.

Sources: https://wiki.libsdl.org/SDL3/SDL_ShowOpenFileDialog and https://wiki.libsdl.org/SDL3/SDL_ShowSaveFileDialog — asynchronous dialogs and their callback's threading; https://wiki.libsdl.org/SDL3/SDL_GPUColorTargetBlendState — per-pipeline blending; https://dyn4j.org/2010/01/sat/ — the separating axis test and the axes rectangles need; https://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf — centripetal parameterization and its end segments; https://qroph.github.io/2018/07/30/smooth-paths-using-catmull-rom-splines.html — evaluating centripetal segments and choosing end points.
