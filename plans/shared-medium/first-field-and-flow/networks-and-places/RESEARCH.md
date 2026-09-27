# Research: networks-and-places

## How should a network hold a carrier's ground geometry?

Linear referencing systems hold a line as a sequence of points with a measure at each, and interpolate between neighbours for anything in between. PostGIS locates the nearest point of a line to a given point as a measure along it, and turns a measure back into a point by interpolating along the segment that contains it. The two operations are exactly what places need: a ground position snaps to a carrier and a distance, and a distance names a ground point.

Games that move things along splines do the same with an arc-length table. They sample the curve, accumulate distances, and map a distance back to the curve by interpolating in the table. The table's entries can be as exact as the method that fills them, so a producer that measures arc length by Gauss-Legendre quadrature, as navigable-networks plans to, can put those exact distances at the points. Between points the ground position is a chord, which is close enough for snapping a click or carrying a guest across an edit, and the distance at every point, including every node, is the producer's own.

What this settles: a carrier is a polyline of ground points, each with its arc length, supplied by the producer. The network is then plain data, copied and hashed by the walk like any other component, and the medium needs no curve code of its own.

Rejected: a function the producer supplies to evaluate its curve, since the network would hold a reference to code, and the nearest-point query would need the producer's curve math. Chord lengths as distances, since they shorten curved paths, which navigable-networks' research already rejects.

Sources: https://postgis.net/docs/ST_LineLocatePoint.html — the nearest point of a line as a fraction along it; https://postgis.net/docs/ST_LineInterpolatePoint.html — a point from a measure, interpolating Z and M; https://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf — Catmull-Rom parameterizations, background for the curves the producer samples.

## How are edges tied to carriers so that every place resolves?

A network whose edges are listed freely can leave part of a carrier uncovered, or cover it twice, and a place there would resolve to nothing or to two edges. If instead each carrier lists its stops, the distances where it meets a node, starting at 0 and ending at its length, then its edges are the stretches between consecutive stops. They tile the carrier exactly, every distance from 0 to the length resolves to one node or one edge, and a junction is a node that stops on several carriers. OSRM's and Valhalla's snapped positions, an edge plus offsets to its ends, are then just a place resolved against this tiling.
