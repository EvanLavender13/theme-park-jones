# Research: explained-food

## What kind of measure is food availability, and how is it attributed?

Food availability as the capability defines it is what transport planners call a gravity accessibility measure: a sum over destinations, each weighted by a decay function of the travel cost to reach it, between 1 at no cost and 0 far away. The alternative, cumulative opportunities, counts every destination within a threshold at full weight and none beyond it. Studies find the two highly correlated, but the threshold is a gate, and principle 5 rules gates out, so the decay is continuous. Planners commonly use a linear decay to a cutoff, which is what decision 0020's piecewise-linear curves give.

Because the measure is a sum, attribution needs no apportioning, unlike the geometric mean decision 0020 names for quality: each shop's term is its contribution. Exact reconstruction then follows from defining the value as 0.0 with each term added in a fixed order, ascending shop key, so the value and the sum of the listed terms are the same arithmetic. Tooling may use the C runtime freely (decision 0022), but availability needs only multiplication, division, and addition.

Rejected: cumulative opportunities within a walking-time threshold. It is a gate on reach, and the overlay would show a cliff where the park has none. Rejected: a geometric mean over shops. It suits combining different inputs to one quality, not adding the food many shops offer.

Sources: https://docs.conveyal.com/learn-more/decay-functions, gravity and cumulative measures, and linear decay with a cutoff; https://cran.r-project.org/web/packages/accessibility/vignettes/decay_functions.html, decay functions as weights between 0 and 1 falling with travel time; https://findingspress.org/article/32444-cumulative-versus-gravity-based-accessibility-measures-which-one-to-use, the two measures' correlation.

## What should the tuning values be?

The reference walking speed is the guests' own WALK_SPEED, 1.3 m/s, so an effective time is what a guest would feel. With it, fed.park's farthest guest path point, the end of the path running south-east, about 60 m by path from the shop, is about 47 s away. A shop's expected wait is ORDER_DELAY plus the shipment's delay before it has stepped, about 20 s over fed.park's 35 m backstage path, and SERVICE_INTERVAL, 3 s, per guest queued once stocked, so waits run from a few seconds to about 20 s. Effective times across the slice's parks therefore run from near 0 to about 70 s.

A curve through (0 s, 1), (60 s, 0.5), (120 s, 0.2), and (240 s, 0) spreads those times over most of its range: a guest at the shop's door with a short wait reads about 0.95 of MEAL_RELIEF, and one at the far end of fed.park about 0.5 to 0.6 of it, so the overlay shows a gradient across fed.park rather than a flat color. The curve is flat at 0 only beyond four minutes, far past any route in the slice, so no reachable supplied shop reads as zero.

The band is 6 m from a guest path's line, twice the guest path's 3 m width, so it shows 4.5 m of shading either side of the ribbon and reaches past CONNECTION_REACH, 4 m, to the doors of the boxes the path serves.

These are starting values. The capability sets them here and adjusts them by looking at captures.

Sources: src/sim/guests/SPEC.md, WALK_SPEED; src/sim/operations/SPEC.md, ORDER_DELAY, SUPPLY_SPEED, SERVICE_INTERVAL, and the offer's wait; tests/parks/fed.park, the layout; src/sim/park/intent.h, pathWidth.

## How should the band be drawn?

City builders that measure influence along roads, such as Cities: Skylines' service coverage view, color the roads themselves rather than the ground, so the view reads as the network. The capability asks for a band of ground around each guest path, shaded by the value at the nearest path place, which carries the same idea onto the ground next to the path.

The cheapest faithful form is a ribbon along each guest path's ground line, the band's width on either side, with each vertex colored by the value at the path place it stands beside, drawn over the terrain and under the paths. It reuses the park mesh's ribbon, so it can be built and tested on the CPU like every other mesh, and it samples one place per vertex: about a hundred places at a 1 m spacing for fed.park. Within the band, the nearest path place of a ground point beside a straight stretch is the place the ribbon's vertices interpolate, so the ribbon is exact there. Where two paths' bands overlap, as at a junction or where two paths run within twice the band of each other, several ribbons cover the same ground, and only one may show: translucent ribbons would blend twice, and opaque ones at one height would flicker. Drawing each ribbon as a low tent settles it: a middle row of vertices on the line at the ribbon's highest lift, and its edges lower by an amount proportional to their distance from the line, so at any ground point each ribbon's height falls with the point's distance from its own line, and the depth test shows the ribbon of the nearest line. That is the depth-buffer construction of Voronoi regions, with lines as sites, and the rule the capability asks for: the value at the nearest path place. It is checkable on the CPU from the mesh's heights. With a drop of a centimeter across the band, two lines' ribbons at a point differ in height by a centimeter per band width of difference in distance, which reversed float depth resolves, as it resolves paths 2 cm apart. Only within one line's own bend, where its ribbon folds over itself, can the far side of the bend show.

Route distance inside an edge is the lesser of two straight-line terms along the carrier, so availability along an edge is piecewise linear with kinks where the nearer end changes, and a 1 m spacing keeps the ribbon's interpolation within a small fraction of the curve's slope.

Rejected: a grid of ground cells, each sampling the value at nearestPlace. It is exact everywhere, but each cell projects onto every carrier segment and samples every field each tick, thousands of samples on a small park, and its cells show as steps. Rejected: a fragment shader measuring distance to the paths. The values would go to the GPU as a texture, and the result could not be tested without a GPU.

Sources: https://doi.org/10.1145/311535.311567, Hoff and others, Voronoi regions of points and lines by depth-tested cones and tents; https://steamcommunity.com/app/255710/discussions/0/490125737459421655/ and https://forum.paradoxplaza.com/forum/threads/does-it-matter-where-i-put-city-service-buildings.988039/, Cities: Skylines' coverage shown along roads and measured along them; src/render/SPEC.md, the ribbon and the ghost pipeline.

## Can a candidate be rebuilt every tick?

makeCandidate copies the world and runs one resolution with no stepping. Loading warm.park and resolving it, process start included, takes about 30 ms on windows-debug, an upper bound on one resolution. Stepping it runs about 0.6 ms a tick. A resolution derives both networks and runs route distance from every anchored source, which on the slice's parks is a few dozen nodes, so a copy and a resolution per tick at 30 ticks a second is expected to stay interactive. The feature that keeps the candidate current measures it in the app. The fallbacks are rebuilding at most once a frame, since frames can hold several ticks, throttling, and world-as-value's incremental resolution candidate.

Sources: build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park with --ticks 0 and 300 --hash; src/sim/SPEC.md, makeCandidate.

## How should a click find a guest or a shop?

The app already finds the ground point under the cursor, and tools' boxAt finds the box whose footprint holds a ground point. A guest is drawn as a 0.6 m by 0.6 m box 1.8 m tall, built from the ground position its inspection record publishes, so the ground point under the cursor falls inside a guest's footprint only when the click lands near its feet. From a low camera, a click on its body lands on the ground behind it. A ray tested against each drawn box, by the slab method, with the nearest hit winning, matches what the player sees, and still matches a guest only by its published position, since the box is built from it. The same test on a shop's 4 m box picks it by its roof or sides.

Rejected: picking by ID rendering, drawing each entity's key into an offscreen buffer and reading back the pixel under the cursor. It is exact for any shape, but it needs a GPU pass and a readback, cannot be tested without a GPU, and boxes do not need it.

Sources: src/render/SPEC.md, groundAtCursor and the guest mesh; src/tools/SPEC.md, boxAt.
