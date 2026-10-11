# Research: Best Seats

## Which pieces a best-seats field should keep at a place

A guest scores a seat as its own weight on the need's relief plus a fixed weight on the walk (src/sim/guests/guests.cpp, scoreOptions). The relief weight grows with the guest's need through needCurve, so two guests at one place can rank the same seats differently. A field computed in resolution can't know which guest will read it. It must keep enough pieces that every guest's best few are among them, whatever its need.

The database literature calls such a set a k-skyband. A piece dominates another at a place when its walk there is no longer, its relief is no lower, and it comes first in a fixed tie order: shorter walk, then higher relief, then lower key. Ties matter, because pieces on both sides of a segment can share a seat place. The k-skyband keeps every piece that fewer than k others dominate. For any score that improves with the field's relief and worsens with the walk, the k best pieces lie in the k-skyband, with ties broken by the same order. A guest that scores seats that way finds its own k best in the field, with its own weights, and the field never reads a guest weight.

The set stays small. Relief depends only on the piece's kind, so there are two relief levels today. Within one level, a piece is kept only while fewer than k nearer pieces have at least its relief. For rest, this keeps the k nearest benches and any table nearer than the k-th bench. A field keeps at most k pieces per relief level at a place, so at most 2k today.

A seated meal also rests the guest, because every seated visit comes back with a rest. If a guest scores a meal seat by both its meal relief and its rest relief, one field's skyband no longer covers it. Today the union of both fields still holds every such best. Relief depends only on kind, so within one kind the nearer piece always wins. The meal field holds the k nearest tables, and the rest field holds the k nearest benches. Once relief varies by piece, that stops holding. seated-meals decides how guests score a seat, and it should know this limit.

Rejected: a fixed exchange rate between metres and relief, owned by the field. It would rank seats differently from guests, so a guest could miss its own best seat. To avoid that, the field would need guests' weights, and guests sit in a layer above it. The k nearest pieces whatever their relief. The rest and meal fields would then hold the same pieces, and a bench just past the k-th table would be lost to a tired guest who prefers it. The k nearest of each kind. This is exact while relief depends only on kind, and it fails once relief varies by piece. The skyband gives the same guarantee with fewer pieces, and keeps it for scores on one relief when relief varies by piece.

Sources: https://www.mdpi.com/2220-9964/6/5/137 — distance-based skyline queries on road networks, with dominance by attributes and network distance; https://crad.ict.ac.cn/EN/10.7544/issn1000-1239.2020.20190026 — skyline variants on road networks, including k-dominance; Deng, Zhou, and Shen, "Multi-source skyline query processing in road networks", ICDE 2007 — the problem's original statement.

## How to compute it once for the whole network

One label-setting search per field does it, seeded from every piece at once. Each piece's seat place seeds the nodes at its edge's ends with the walk to them, or its own node if it sits on one. The search takes labels in order of distance, then higher relief, then lower piece key. It keeps a label at a node only if fewer than k labels already kept there have at least its relief, and it spreads only kept labels. Each node keeps at most k labels per relief level, so the work grows with the network times a small constant. This is the multi-source form of Dijkstra's algorithm, extended from the one nearest source to several.

The kept sets close along routes. Suppose a piece is kept at a node, and the next node on its shortest route is u. Any piece that dominates it at u also dominates it at the first node, since the walk from the first node is at most the edge plus the walk from u. So the piece has no more dominators at u, and it is kept there too. Following the next steps from any node with an entry therefore reaches the piece, and each node on the way holds it. The way toward a piece is chosen after the search, as route distance chooses its next steps (src/sim/routes/route_distance.cpp, routeEntries): of the steps that achieve the node's distance, the one first by a fixed rule. The result depends only on the network and the pieces, not on the search's order.

Floating-point rounding can make two routes of different lengths equal once an edge is added to both. Then a piece can come before another at u and after it at the first node, by key alone. In that case the argument above fails, and the search can keep a slightly different set from a brute-force skyband. It still gives one answer on every build, and every next step still leads to a node that keeps the piece, because the search spreads only kept labels. A spec should define the fields by what the search keeps, and a check against brute force should expect a match on real parks, not prove it for every park.

Cost: route distance runs one search per source over the whole network. full.park has 644 pieces, so one search per piece would cost hundreds of route distance searches. Two skyband searches with k = 4 cost about as much as 16 single-source searches. full.park's resolution is 14.1 ms today (build/seat-service/after.report).

Rejected: one route distance search per piece, then a merge. This is the cost the slice exists to avoid. Computing the fields in a tick. Decision 0031 puts whole-network work in resolution.

Sources: https://arxiv.org/pdf/1504.05140 — survey of route planning, label-setting searches, and one-to-all variants; https://ocw.mit.edu/courses/1-204-computer-algorithms-in-systems-engineering-spring-2010/4127883d13d246a9dbbda7cb05e4e545_MIT1_204S10_lec12.pdf — label setting against label correcting.

## How to read a place between nodes

Guests stand between nodes, and seat places lie between nodes. A place inside an edge reaches a piece through one of the edge's ends, or directly along the edge if the piece's seat place lies on it. The same domination argument shows that the place's k-skyband lies within the two ends' kept pieces and the pieces on the edge. The field's sampleEdge (sim/medium/SPEC.md, Fields) can therefore give the place's skyband from the entries at the ends and along the edge alone. Each piece also needs an entry at its own seat place that says the guest has arrived, so a guest walking along the edge stops there.

A next step can now end inside an edge, at a seat place, where route distance's steps always end at a stop. Walking a step to a place that isn't a stop is believable-guests' concern in seated-meals.

## Which source the entries go under

sampleField looks up every source's slot at a place (src/sim/medium/field.h, layeredSlots and sampleSlotsAt). Its cost grows with the number of sources, not with the entries found. If each piece were the source of its own entries, every guest's sample would look up all 644 of full.park's pieces, twice. The work of guests' choices would then grow with guests times pieces. The slice forbids that for the fields' cost, and the project treats such products as defects.

If each field has one source, a sample costs a few binary searches in one slot, whatever the number of pieces. sampleEdge then sees every piece's entries at the edge's ends at once, so it can give exactly the place's skyband. Each entry still names its piece, its segment, its place, and its relief, so a guest's choice and an overlay's hover trace to the piece (principle 8).

Rejected: each piece as its own source. Sampling would grow with pieces. Inside an edge, sampleEdge would run once per piece and couldn't drop pieces another piece beats, so a sample would give the union of both ends' sets. An index of entries by place across sources, in the medium. That would be a shared-medium change for every field, and the edge rule would still see one source at a time.

## Where the code goes

The layers (cmake/layers.txt, decision 0027) put routes below furniture and operations. Seat offers come from operations, so a module that reads them can't sit in routes. The search itself reads only a network and seeds, each with a place, a value, and a key. It can sit in routes beside route distance, where bins can reuse it with one value for every bin. A new module between operations and guests can then define the two fields and the resolver that seeds the search from seat offers and seat places. The layer table and the layer check's expected list both name the new module.
