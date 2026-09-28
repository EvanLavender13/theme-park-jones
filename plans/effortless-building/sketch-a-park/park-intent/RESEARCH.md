# Research: park-intent

## How is a centripetal Catmull-Rom segment evaluated deterministically?

A segment from P1 to P2, with neighbors P0 and P3, has knots t0 = 0 and each next knot the previous one plus the square root of the chord between the two points, which is the centripetal choice, alpha one half. The Barry and Goldman pyramid evaluates it at a parameter t between t1 and t2 with three rounds of linear interpolation: A1, A2, and A3 between consecutive points over their knot intervals, B1 and B2 between the As over the wider intervals, and C between the Bs. It uses only subtraction, multiplication, division, and addition, and the knots use sqrt, which is exact, so one written order of operations gives the same bits on every build (decision 0022).

Mathematically, t = t1 gives P1, but in floating point the pyramid's weights, such as (t2 - t1) / (t2 - t0) and (t1 - t0) / (t2 - t0), need not sum exactly to 1, so the result can miss P1 by an ulp. The ground line therefore takes each clicked point as it is for the ends of its segments, and evaluates the pyramid only strictly inside them. The line then passes through the clicked points bit for bit, which is what junction snapping and navigable-networks' node placement rely on.

Rejected: the matrix form with tangents scaled by the knot intervals. It gives the same curve, but it reorders the arithmetic, and it needs the tangents' special cases handled separately.

## How densely is the ground line sampled?

The sample count per segment must depend on the points alone, so a line is the same wherever it is computed. A count proportional to the chord keeps long segments smooth, and a floor keeps a short, sharply turning segment from becoming a visible polygon. One sample per meter, with at least eight per segment, keeps the chord error under a few centimeters for paths a few meters wide. Points inside the park's square give chords of at most about 362 m, so no segment takes more than 363 samples and no cap is needed. A point closer than a centimeter to the one before adds nothing the player could see, and dropping it keeps every chord far above rounding. Checked by park-view's captures, and adjusted there if the paths look faceted.

The line's distances are the running sum of its straight steps, not the curve's true arc length. That is the length of the polyline the medium actually interpolates along (src/sim/medium/SPEC.md, groundPoint), so a carrier built from the ground line is consistent with itself, and its lengths fall short of the true curve by well under a percent at this density.

Rejected: Gauss-Legendre arc lengths as the line's distances. They measure the curve, not the polyline the medium walks, so a place's ground point would drift from where its distance says it is. navigable-networks' research proposed them before the ground line existed. A fixed count per segment, which is coarse on long segments.

## How is a box's facing normalized for any input?

Dividing by sqrt(fx * fx + fz * fz) overflows to infinity for very large components and underflows to zero for very small ones. Dividing both components by the larger magnitude first makes one of them exactly ±1 and the other at most 1 in magnitude, so the sum of squares lies between 1 and 2. Then dividing by its square root gives a unit direction for every finite nonzero input, using only exact operations. A facing whose larger magnitude is zero has no direction.

Sources: https://www.cemyuksel.com/research/catmullrom_param/catmullrom.pdf — the centripetal parameterization; https://en.wikipedia.org/wiki/Centripetal_Catmull%E2%80%93Rom_spline — the Barry and Goldman pyramid in knot form; https://qroph.github.io/2018/07/30/smooth-paths-using-catmull-rom-splines.html — evaluating a segment from its four points.
