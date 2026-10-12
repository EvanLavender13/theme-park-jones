# Research: explained-seating

## How a seat overlay's value comes from the best seats fields

Today's rest overlay sums a term for every bench box the place can reach. Each term is the bench's relief discounted by the walk and the wait, which needs a route distance from every bench. Furniture has no route distance of its own. That is the point of the slice: a piece adds an entry to the two best seats fields, not a route search (navigable-networks' best-seats). A seat overlay can therefore read only what those fields give at a place. They give the pieces that fewer than SEAT_CHOICES others dominate there, each with its relief and its route distance. One piece dominates another when it is no farther and gives no less relief.

The value that fits is the sum of the SEAT_CHOICES largest terms, where a piece's term is its relief discounted by DISCOUNT_CURVE at its walk in game minutes. Seats have no wait term, because a guest scores a seat without one (seated-meals' criterion 1). This sum has three properties that matter:

- It is exact from the field. A dominated piece is no closer and relieves no more, so its term is no larger than the term of each piece that dominates it. If SEAT_CHOICES pieces dominate a piece, it can't change the sum of the SEAT_CHOICES largest terms. The sum over the field's entries equals the sum over every piece in the park. The hover lists the terms that add up to the value, each naming its piece, kind, and segment (principle 8).
- It is continuous along the paving. Each piece's term changes continuously with the place, and the sum of the few largest of many continuous values is continuous. The shading never steps where a piece joins or leaves the field's entries, as a plain sum over the entries would.
- It counts the seats that serve a place best. Guests score every entry of the field, which holds at least these, so the overlay shows the best of what a guest standing there weighs.

The overlay's shading is linear between bends, as availabilityBends gives them for today's needs. Inside an edge, the field takes a piece's walk as the shortest of three: through the edge's From end, through its To end, and, if the piece's seat place lies on the edge, along the edge to it (routes' SPEC.md). The pieces a place inside the edge can be given are therefore the entries at its two ends and the entries along the edge at their own seat places. A piece's term bends where its shortest walk switches between these, at its own seat place, where its walk is 0, and where its time crosses a point of the curve. The largest few terms also bend where two pieces' terms cross. Furnished segments lay a piece every few meters, so most furnished edges hold seat places, and each one is a bend. The feature that builds the overlay works out how to find them all.

The fields are resolved from intent only, so a seat overlay doesn't change from tick to tick, and a preview of a furnishing setting shows exactly the coverage the setting would add.

Rejected: summing every piece's term, as today's overlays sum every facility — it needs a route distance from every piece, the per-seat route search the slice removes. Summing the field's entries — the sum jumps where a piece joins or leaves the entries, so the shading would step along a path. Showing only the best term — it is continuous too, but a path with one bench shades like a path lined with them, and a guest who finds that bench full has nowhere else to go.

## How other games show where seats are missing

Planet Coaster 2 has heatmaps of the guests' state, such as their energy, with problems in red. They show where guests are tired, not where seats would serve them. That is the demand map this slice leaves out. Parkitect snaps benches and bins to the path and has no seating overlay. Its overlays rate scenery. Cities: Skylines shades each service's coverage, but the shading is a radius for happiness that differs from how the service actually reaches buildings, and players find the mismatch confusing. Here the overlay reads the fields guests choose by, so where it is dark, guests do find seats hard to reach.

Sources: https://www.planetcoaster.com/news/2024-09-25/deep-dive-mastering-management — Planet Coaster 2's heatmaps of guest state; https://store.epicgames.com/news/planet-coaster-2-guide-to-help-you-run-your-dream-park — energy, benches, and rest areas; https://www.gamingonlinux.com/2017/10/making-your-park-look-pretty-in-parkitect-is-going-to-get-important — Parkitect's scenery overlay; https://www.coasterforce.com/forums/threads/specialized-studios-parkitect-final-release.41239 — Parkitect's benches and bins snap to the path; https://steamcommunity.com/app/255710/discussions/0/523890681410776922 — Cities: Skylines' coverage shading is a happiness radius, not real coverage.

## What the rest overlay shows while bench boxes remain

Bench boxes remain only in facilities.park, stocked.park, and full.park (45 of them), and the slice removes them right after this milestone. Guests still weigh a bench box's rest offer until then. If the rest overlay read only the best rest seats, it would leave those bench boxes out for one step. Keeping them needs a second kind of source in the rest overlay, which the retiring step then removes again.
