# Research: place-carry-over

## When is a carrier unchanged, and how does a moved place find its new distance?

The milestone's research settles the policy: linear referencing's Stay Put for a carrier whose geometry changed, which keeps the ground position and recomputes the measure, and Retire for a deleted one. What is left is where the line between unchanged and changed falls, and how the new distance is found.

In linear referencing an event is tied to a route and a measure, and an edit that only adds or moves calibration points or intersections leaves every event where it was, because the route's line and measures are the same. A network carrier is the same: its points, each a ground position and the producer's arc length, are its geometry, and its stops are only where nodes cut it into edges. So a carrier is unchanged when its points are equal, and its stops are not compared. A new node splitting an edge then changes nothing a place holds, and the place keeps its distance exactly, which is the milestone's split criterion.

When the points differ, the old ground position is the place's ground point in the network before, and Stay Put's new measure is the nearest point of the same carrier's new line. That is the projection nearestPlace already makes, restricted to one carrier, so the same arithmetic and tie rule serve both, and a place never jumps to another carrier that happens to run nearer.

Rejected: Comparing stops as well as points — a split would count as a change and send the place through a projection it does not need. Reprojecting every place, changed or not — a ground point projected back onto its own segment can come back an ulp away from the distance it started at, so unchanged carriers would drift across repeated resolutions. Keeping the distance on a changed carrier (Move) — it keeps the place on the carrier but not on the ground, and the milestone lists it as a per-field deepening candidate. Snapping a place whose carrier was deleted to the nearest other carrier — linear referencing retires such events rather than guess, and a retired place is a legitimate state its holder handles (principle 2).

Sources: https://enterprise.arcgis.com/en/pipeline-referencing/10.5/get-started/event-behaviors.htm — Stay Put, Move, and Retire, and which edits leave events untouched; https://desktop.arcgis.com/en/arcmap/latest/extensions/roads-and-highways/event-behaviors.htm — behaviors per edit type, and retiring events on retired routes.
