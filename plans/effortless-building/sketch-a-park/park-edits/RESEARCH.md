# Research: park-edits

## How does the check treat objects that only touch?

Two rectangles that share an edge are physically fine: a shop set flush against a depot, or an entrance whose back lies on the park's edge, as the template's does. The separating axis test calls them separated when some axis's projections meet but do not overlap, so exact arithmetic would accept them. With a facing at an arbitrary angle, the corners carry rounding of about 1e-13 m, so a flush placement computed by a tool can overlap by that much and be refused, which is a refusal nothing physical justifies (principle 5). Collision code handles this with a small tolerance, declaring shapes separated unless they overlap by more than it on every axis. One contact tolerance of 1 mm, applied to every conflict the check finds, lets flush boxes, a path running exactly half its width from a box, and a footprint on the park's edge through regardless of rounding, and it refuses nothing a player could see. It is far larger than any rounding at park scale, where coordinates are at most a few hundred meters and doubles resolve about 1e-13 m, and far smaller than any size a tool works in.

Rejected: exact comparison, which refuses flush placements by rounding. A tolerance scaled to the coordinates, which is harder to state and test and buys nothing at a fixed park size.

## How is the distance from a ground line segment to a footprint computed?

Moving the segment's ends into the footprint's own frame, along Forward and Right from its center, makes the rectangle axis-aligned, half its depth along one axis and half its width along the other. There, a slab clip (Liang and Barsky's) decides whether the segment enters the rectangle, which makes the distance zero. Otherwise the least distance between a segment and a disjoint convex polygon is reached at a vertex of one against the other, so it is the least of each end's distance to the rectangle, found by clamping the end to the rectangle's extent, and each corner's distance to the segment, found by projecting the corner onto the segment and clamping. Every step is a subtraction, multiplication, division, comparison, or clamp, and comparing squared distances with the squared threshold avoids sqrt entirely, so the answer is a fixed sequence of basic operations that both builds share under -ffp-contract=off (decision 0022).

The widened ground line stays inside the park's square exactly when each of its points lies at least half the width inside, since the line between points is straight and the shrunken square is convex. Likewise a footprint lies inside when its four corners do.

Rejected: sampling points along each segment, which misses a corner that pokes between samples. Distances from the segment to each of the rectangle's four edges as segments, which gives the same answer with more arithmetic and needs its own crossing test.

## How does the ghost's validity stay equal to what the commit does?

If the query tools call and the command's application are separate code, they can drift. Making application call the query first, and change nothing when it says no, makes them one decision by construction: a command is applied exactly when the query accepts it on the world it is applied to. Since makeCandidate applies commands to a copy through the same functions, a candidate's intent differs from the original's exactly when the query accepts. The remaining gap is time: the ghost queries the world the frame shows, and the commit applies to the world the next cycle has after earlier commands in the same queue. Systems never change intent, so only other commands in the same cycle can change the answer, which box-tools and path-tool handle by queuing one commit at a time.

A check of the whole world, every object against every other, gives a second statement to test against: from a valid world, a command is accepted exactly when the world it describes is valid. Tests can build that described world from save text, independent of the command's code.

Rejected: a refusal reason returned with the answer, which nothing reads yet and which the ghost does not show. Checking a command by applying it to a copy and checking the whole copy, which costs every object against every other on every ghost frame.

Sources: https://www.oreilly.com/library/view/game-physics-cookbook/9781787123663/ch11s09.html — tolerances make the separating axis test robust to touching and near-parallel cases; https://textbooks.cs.ksu.edu/cis580/04-collisions/04-separating-axis-theorem/index.html — projection overlap on edge normals for convex polygons; https://www.gamedev.net/forums/topic/671654-how-to-get-the-closest-point-from-a-aabb-to-a-line-segment-in-2d/ — segment to axis-aligned box distance by clipping and by vertex cases; https://github.com/juj/MathGeoLib/blob/master/src/Geometry/OBB.h — oriented boxes handled in their own frame.
