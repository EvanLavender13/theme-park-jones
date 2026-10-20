# Research: candidate-ghost

## Where the path drag's frame goes after furniture-units

Four Very Sleepy profiles of windows-release tpj_bench on full.park, taken on main after furniture-units merged (build/perf/next-1.sleepy to -4), split the timed part of each drag frame. The stacks under timeShopDrag or timePathDrag were kept, without stepWorld, hashWorld, or the copy timeDrag makes before its first frame:

| Part | Shop drag | Path drag |
|---|---|---|
| The ghost's edited surface | 0% | 22.3% |
| Copying the world into the candidate | 22.7% | 13.6% |
| Best seats | 19.5% | 14.1% |
| Carrying guests and footfall | 14.2% | 11.4% |
| Guest ground | 11.4% | 5.6% |
| Furniture | 4.5% | 8.0% |
| Planning the edit | 2.5% | 5.8% |
| Route distance | 6.5% | 5.6% |
| Surface and connection units | 1.3% | 6.2% |
| Offers | 7.7% | 2.3% |
| The ghost's other parts | 3.7% | 2.2% |
| Path networks | 5.0% | 1.5% |

The frames' medians in build/furniture-units.report are 3.85 ms for the shop drag and 5.97 ms for the path drag. The ghost's edited surface is the largest cost in the path drag's frame, about 1.3 ms. In the shop drag's frame, copying the world is largest, about 0.9 ms, and the ghost's edited surface costs nothing, since a moved box changes no paving. The path drag is the frame closer to the display's 6.06 ms.

Inside the ghost's edited surface, editedBetween in drawing mode does this:

- It finds the points the edit changed by comparing every segment of the ground before with the ground after.
- It widens them with pointsAffected, before and after. Each call sorts every segment's reach and tests every segment against the changed ones.
- It derives the paving of those points twice, before and after, with surfaceAround. Each call builds the paving of the focus, then the paving of every point near its edge lines, then hides the edge lines.

surfaceAround alone is 18% of the path drag's frame. The ghost then uses only the owners this finds: with a candidate, it draws those owners' paving from the candidate's walked surface. Planning's own share, mostly keptCurves sorting both layouts' segment ends and deriving the after ground, is 5.8%.

## Finding what an edit changed from what resolution recorded

Incremental systems report what changed from the records their update already keeps, not by computing the result twice and comparing.

- Salsa stores, with each memoized value, the revision in which it last changed. When a value is recomputed and comes out equal, it keeps the old revision ("backdating"). A reader asks which values changed since a revision without recomputing anything.
- Differential dataflow and DBSP pass differences between operators. Each operator turns its input changes into output changes, so a consumer of the output reads a list of changes, not two whole collections to compare.
- Build systems with early cutoff (Shake, Bazel) know which outputs a build rebuilt and which came out the same. A tool that wants "what this build changed" reads that list.

The candidate already has such a record. The preview refills the candidate from the world, applies the edit, and resolves once. The units that resolution derives are listed in the candidate's derived units until it resolves again, and derivedUnits reads them by kind. Every paving unit whose value differs from the world's is among them, because the candidate's units equalled the world's before that resolution. Comparing each listed owner's run in the candidate's paved surface with its run in the world's gives exactly the owners whose paving the edit changed. The same holds for the ground-line units and the segments whose line or width changed.

What this suggests: with a candidate, the ghost can read the edited surface from the candidate's derived paving units and ground lines, and derive no paving. Finding the edited surface then grows with what the edit changed, not with the park. Without a candidate, when the world refuses the edit, nothing is resolved, so the ghost still derives the paving as it does now.

Rejected: Keeping a separate list of changed units on every resolution — the derived-units list already holds a superset, and comparing each listed unit with the world's is exact and local. Making pointsAffected and surfaceAround local through the cell grids — the work would still derive paving that the candidate's resolution has just derived. Diffing the whole paved surfaces of world and candidate — it reads every owner of the park on every frame.

Sources: https://git.joshthomas.dev/mirrors/salsa/raw/branch/gh-pages/reference/algorithm.html — Salsa's red-green algorithm, revisions, and backdating; https://ar5iv.labs.arxiv.org/html/2203.16684 — DBSP, incremental view maintenance by passing deltas between operators; https://api.pgxn.org/src/pg_trickle/pg_trickle-0.84.0/blog/differential-dataflow-explained.md — differential dataflow's weighted updates as changes to read.
