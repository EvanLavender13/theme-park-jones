# Research: navigable-networks

## How should movers find their way along a network?

Brogue's Dijkstra maps store, at every place, the distance to a set of goals. An actor moves by stepping to its lowest neighbor. One map serves every actor heading for the same goal, and the same maps serve many behaviors: approach, flee, find food, explore. Flow fields, which Elijah Emerson built for Supreme Commander 2, extend the idea. Each cell also stores the direction to move, so thousands of units share one computation instead of each running A*.

Both suit this project. A route-distance entry for a source, holding the distance to that source and the next edge toward it, is a Dijkstra map on a graph. A guest's choice reads the distances (decision 0019), and its movement follows the next edges, so choice and movement agree about which way is best. Nothing is stored per mover except its place, and a place stays meaningful across edits because it is a carrier (a drawn path or a connector, keyed from intent) and an arc length along it, not an edge. Resolution recomputes the entries, and a moving guest follows the new ones from where it stands. If its carrier's geometry changed, as when a box moves and its connector with it, the guest moves to the nearest point on the same carrier. Only a place whose carrier was deleted becomes no place, and the mover's owner decides what happens then.

On a graph, a place partway along an edge takes the better of its two ends. Its distance is the smaller of the offset plus the distance at the edge's start node, and the remaining length plus the distance at its end node. Its next direction is toward whichever end won. Computing the node values is one Dijkstra run per source over that source's network. At slice scale, with a handful of sources and graphs of hundreds of nodes, that is trivial. At larger scale the per-source cost is the known limit, and flow-field practice answers it with hierarchy, as Emerson did with HPA*, or with coarser maps.

Ties between equal routes must break deterministically, by a fixed rule such as the lower carrier key and then the lower arc length, or movement would depend on iteration order (principle 10).

Rejected:

- A* paths stored per mover: they go stale when the network changes, they are a second routing mechanism that can disagree with the distances used for choice, and a mover would be calling another capability rather than sampling the medium.
- Grid-based flow fields: the park's movement lives on drawn paths, not open ground, so a graph is the natural domain and principle 4 measures along it.

Sources:

- https://www.roguebasin.com/index.php/The_Incredible_Power_of_Dijkstra_Maps: Dijkstra maps, stepping downhill, and one map shared by many actors and behaviors.
- https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter23_Crowd_Pathfinding_and_Steering_Using_Flow_Field_Tiles.pdf: flow fields for many units, combined with HPA*.
- https://howtorts.github.io/2014/01/04/basic-flow-fields.html: the basic flow-field construction from an integration field.

## How do park games structure path networks for guests and staff?

Parkitect keeps a connectivity labelling of its paths, updated quickly by flood fill, so it can tell whether a place is reachable before searching. It has separate pathfinding for guests and staff, because guests can also ride transport rides. Guests are not supposed to use employee paths, but they may when there is no other way out, for example from a ride exit connected only to one. That is a gradient where a gate might have been, which fits principle 5.

For this project, guest and backstage networks stay separate in the foundation. The slice depends on it: cutting the backstage path must starve the shop, so supplies must not fall back to guest paths. Parkitect's fallback suggests a later deepening: crossing into the other network at a high cost, instead of never. Connectivity labels, meaning which component of a network each node belongs to, fall out of the per-source Dijkstra runs, since an unreached node has no entry. They need no separate pass until something wants reachability without a source.

Rejected:

- One shared network with per-kind costs in the foundation: the slice's cut scenario needs supplies to have no route when the backstage path is gone.

Sources:

- https://themeparkitect.tumblr.com/post/136005228372/devlog-update-77: flood-fill connectivity and its debug view.
- https://steamcommunity.com/app/453090/discussions/1/357285562487671391/: guests using employee paths only when there is no alternative.

## How are drawn curves turned into a graph with lengths?

A path's curve (centripetal Catmull-Rom, plans/effortless-building/RESEARCH.md) is cut into edges at every node:

- its endpoints;
- snapped junctions, where an endpoint lies on another curve;
- crossings of same-kind curves;
- connection points, where a box's connector meets it.

Each edge's length is its arc length. The standard method sums Gauss-Legendre quadrature over each polynomial segment of the spline. It is exact for polynomials, and very accurate for the square-root speed a curve's arc length integrates. Adaptive subdivision can refine it further. With a fixed number of quadrature points and a fixed order of operations, the result is deterministic under the simulation's flags, since it uses only basic operations and square roots.

Finding crossings between curves needs a robust curve-curve intersection. For short Catmull-Rom segments, a practical method is recursive subdivision of bounding boxes down to a tolerance, followed by a few Newton steps. The tolerance also decides when two nearly touching curves count as meeting. A place's offset along an edge is measured in arc length, so fields are sampled at physically meaningful positions.

Rejected:

- Chord-length polylines as edge lengths: they underestimate curved paths and would let the drawn tube and the walked route disagree.
- Closed-form arc length: none exists for cubic splines in general.

Sources:

- https://homepage.divms.uiowa.edu/~kearney/pubs/CurvesAndSurfacesArcLength.pdf: arc-length parameterization of splines by Gauss-Legendre per segment.
- https://www.saccade.com/writing/graphics/RE-PARAM.PDF: Peterson, arc-length reparameterization with adaptive quadrature.
- https://medium.com/@all2one/how-to-compute-the-length-of-a-spline-e44f5f04c40: practical spline length computation.
