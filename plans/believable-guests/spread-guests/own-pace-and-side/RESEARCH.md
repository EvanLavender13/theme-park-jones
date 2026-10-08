# Research: own-pace-and-side

The milestone's RESEARCH.md gives the reasons for varied speeds, lanes by direction, and shares that thin toward the edges. This file covers how guests do it in today's code.

## Where does the side shift apply?

Today a guest's aim is a spot on its route ahead, and pointAt gives the spot's ground position on the network's line. advanceAim string-pulls along those positions, walkTowardAim walks toward the aim's position, and nearestSpot finds the guest's place from the same positions. The shift belongs in the positions the aim reads, not in the place. If each point of the route ahead gets a second position, moved to the guest's side, then advanceAim, walkTowardAim, and reachedEnd read the moved positions through pointAt, and nearestSpot keeps reading the line. The place, and everything that reads it, stays on the network as before (decision 0038). A spot between two route points lies between their moved positions.

The aim spot is saved as a place, and the moved position is worked out again from it on each pass. The guest then always walks toward the same point it last found a clear line to, as it does today.

Rejected: moving only the aim's final position — the string-pulling would find the farthest spot on the middle line, and its moved position might not be in sight, so the guest would switch between its side and the middle from tick to tick. Moving the place too — route distance, footfall, and choices would read a point beside the network, which decision 0038 rules out.

## How does a guest keep walking when its side has no room?

advanceAim stops at the first route point a straight line can't reach, and halves along the stretch before it. If a moved route point lies where the guest's box doesn't fit, no line reaches it. The guest walks up to the farthest clear spot short of it and then stands there for good. This happens wherever the paving is narrower than the route's width at a route point:

- At a door, whose end is a line the box keeps clear of.
- At a path's free end, whose end edge runs through the point.
- Around the outside of a junction, whose rounded outer edge curves in before the point (sim/park/SPEC.md, Paved surface).

So a moved point is kept only where the guest's box fits there (GuestGround::fits, with door ends counted). Otherwise the point stays on the middle line, where today's guests already walk. A route point on the line fits wherever guests walk today. At a door, this leaves the aim at the door itself, which the milestone asks for. The same rule covers a path's free end and the outside of a junction, so a door needs no rule of its own.

Inside a stretch, both ends are moved by the same width, so the line between them runs parallel to the network's line within the segment's paving. Where one end stays on the middle, the line between them still runs inside that segment's paving.

Rejected: moving the point by half as much, and then not at all, until it fits. It is smoother at the outside of a junction, but it needs more tests for each point. A route point that stays on the middle is only a waypoint, and string-pulling usually passes it. Offsetting the route as a polyline with mitred joins — at a sharp inside bend with short pieces, the offset line folds back on itself.

## Which way is the guest's right, and how wide is the route?

sim/park's leftOf turns a direction a quarter turn counter-clockwise seen from above, to (Z, −X). The right of a direction (X, Z) is therefore (−Z, X). Each route point is moved to the right of the stretch of route that runs into it, and the first point to the right of the stretch after it. A stretch with no length on the ground takes the direction of the nearest stretch that has one. The direction comes from the stretch's own two ground positions. Every bend of a carrier is a route point, so a stretch is one straight piece. directionAt gives the same direction inside a piece. At a bend, though, it gives the piece ahead in the carrier's own order, which is the piece behind a guest walking toward lower distances, and it resolves the place each time.

A carrier's width is the same along its whole length, and walkStep already finds the carrier for each step it adds. Each route point can take the width of its stretch's carrier there. This is the width widthAt gives. A point takes its width from the same stretch it takes its direction from. A guest standing on a junction's node has its place on the carrier it arrived by, while its route runs out along another. Taking the arriving carrier's width with the leaving stretch's direction would move it along the street instead of into the side path. The width a guest's box can use is that width less twice GUEST_CLEARANCE.

Rejected: directionAt at each point — it resolves a place per point and points the wrong way at bends for guests walking toward lower distances. widthAt at each point — it resolves a place to find a width the step's carrier already holds.

## What side and speed values?

A guest's side runs from 0 at the left of the usable width, seen the way it walks, to 1 at the right. Its side is SIDE_BIAS plus its share times (1 − SIDE_BIAS), and its share is the mean of two uniform draws. With SIDE_BIAS 0.5, sides run from the middle to 1 and peak at 0.75. On the 10 m main street the usable width is 9.1 m. A guest's offset from the middle runs from 0 to 4.55 m right and is most often 2.3 m right. The two streams' peaks lie 4.5 m apart, and they don't overlap. On a 3 m path the usable width is 2.1 m, so offsets run from 0 to 1.05 m right, and guests walk close together. The plan started at 0.4, with peaks 3.6 m apart, and the user found the streams too close in play.

Speed is drawn uniformly from WALK_SPEED − 0.3 to WALK_SPEED + 0.3, 1.0 to 1.6 m/s. Keeping WALK_SPEED as the mean keeps food availability's REFERENCE_SPEED (src/legible/availability.h) equal to how fast guests walk on average.

Each guest saves its speed and its share, not its side, so changing SIDE_BIAS doesn't need the parks made again. Both are drawn in addGuest on the guest's key, as its hunger rate is.

Rejected: a normal speed distribution — it needs more than the simulation's + − × ÷ and sqrt to draw (decision 0022). A uniform draw has the same mean, and the milestone fixes it.

## How much does it cost?

Moving the route costs one fits test for each route point that moves, and a square root for each stretch, per trace. It is proportional to the route ahead, which AIM_LOOKAHEAD bounds, not to the park (decision 0031). fits looks only at the pieces and walls listed in the point's cells. The runtime report on full.park measures it.
