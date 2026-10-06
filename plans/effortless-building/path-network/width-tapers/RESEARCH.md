# Research: width-tapers

## How should paving change from one width to another, and how should the player set it?

Road design changes pavement width with a taper, sized by a length. US state manuals use a straight-line taper for lane reductions. For redirecting lanes at speed, they prefer a short reverse curve, which is two arcs bending opposite ways. The required length comes from speed and offset, such as L = W·S²/60 below 45 mph. When a width change doesn't put an angle in the next lane, a 25:1 taper is enough. For a footpath, no speed rule applies, so the player sets the length directly.

Theme park and city builders don't offer this. Planet Coaster's paths have no width taper, and players fake one by placing and trimming path pieces. Cities: Skylines has transition pieces only for some built-in asymmetric roads.

For this game, the taper replaces the step where the paving's outline would jump straight across from one segment's side to the other's. That happens on both sides of a straight-on join and on the outside of a shallow bend. Each such side gets one cubic curve, from the narrower segment's side a taper length back from the point, into the wider segment's side where it begins. The curve leaves and meets each side along it, so there is no corner at either end. Between parallel sides, with each end direction scaled by how far the curve travels along it, the cubic's offset is exactly 3f² − 2f³ of the width difference. Scaling the end directions that way also keeps a short taper from looping back. The curve is cut into pieces within ARC_TOLERANCE using the bound M / (8n²), where M is the largest second derivative, which lies at one of its ends.

Sides that already join, like the inside of a bend, keep their fit. The user expected exactly this: the inside of a shallow bend already curves by default, because its inside edge can't be sharp there, and only the outside stepped.

With no length set, a curve runs twice as far along the narrower segment as it moves out, so nothing ever steps. The length is held on the point, so it follows the segment that is narrower. The handle works like the radius handle.

Rejected: a straight-line taper — it leaves an angle at each end. Narrowing the wider segment — built first; the user expected the narrower path to grow into the wider one. Refitting the whole bend at one width, then widening the narrower segment out from it — it made a knob, a second curve and a sharp corner at bends, and closed off holes at sharp turns. Holding the length on each segment end, as edge radii are held — the milestone puts it on the point. A step by default — the user saw it as broken next to the inside's automatic curve. A fan of triangles from the point — a curved outline isn't always star-shaped from the point, so those points are cut by ear clipping.

Sources: https://connect.ncdot.gov/projects/Roadway/Training/Lane%20Width%20and%20Taper%20Transitions.pdf — taper length formulas and the 25:1 rule; https://www.txdot.gov/manuals/des/rdw/chapter-4--basic-design-criteria/4-10-cross-sectional-elements/4-10-2-speed-change-lanes.html — straight versus reverse-curve tapers; https://planetcoaster.fandom.com/wiki/Paths — no width taper in Planet Coaster; https://steamcommunity.com/workshop/filedetails/discussion/812125426/2296220207936693647?l=english — Cities: Skylines transition pieces.
