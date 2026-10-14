# Research: derived-units

## How units record reads, find what changed, and stay exact

The question is how resolution can keep derived values in small keyed units and re-derive only those whose inputs changed, in this codebase, with results identical to resolving from nothing. The milestone's RESEARCH.md covers the designs this draws on: build systems' minimality and early cutoff, Salsa's recorded reads per keyed call, and Bevy's change ticks.

Where changes come from. Only commands change intent. A command writes through World's non-const accessors: get, tryGet, emplace, emplaceOrReplace, getOrEmplace, remove, and destroyEntity. Loading a save writes through the stores directly, but a loaded world has no units yet, so it derives everything. Bevy marks a component changed when code takes it mutably, without comparing values. The same works here: a world logs a key whenever one of these accessors is used on a store that units read. A key marked without a real change re-derives its readers, and early cutoff stops there, because their values come out equal.

What a unit may read. A derivation that could read the world directly could miss recording a read. So a derivation gets only a recorder, through which it reads three things, each by key:

- an intent component,
- the unit of an earlier kind,
- the keys an index files under a key.

Scans are not offered. Each read is recorded as a table and a key. A read of something absent is recorded too, so adding it later re-derives the reader.

Why indexes. The ground at a point depends on the segments that end there. Finding them by key needs an index from point to segments. Otherwise the derivation scans every segment and reads the whole segment store, so every edit would dirty every point again. An index is kept from the items it files: when a segment changes, only the entries under its old and new points change. This is what differential dataflow calls an arrangement.

Order and scheduling. Unit kinds and indexes run as steps in the schema's resolver order, as resolvers do. A kind reads only components, indexes, and kinds registered before it. So one pass in registration order, deriving each kind's dirty keys in ascending key order, settles everything, with no restarts. A resolver registered after a kind may read its units without recording. Resolvers run as they do today.

Exactness. A unit's value is a pure function of what it read. Values compare by the walk's words, bit for bit, because operator== calls +0.0 and -0.0 equal while the walk tells them apart. When nothing it read changed, deriving it again would give the same value. When the value comes out equal, nothing that read it can change. So keeping every unit whose reads are unchanged gives exactly what deriving every unit gives. A loaded world derives every unit, so the save checks compare incremental resolution with resolution from nothing. That holds only if the shared geometry is computed by the same functions, in the same floating-point order, on both paths. So groundOf for a layout, which edits still use for hypothetical layouts, is rebuilt on the same per-point functions the units call.

Rejected:

- Comparing every intent store with a snapshot at each resolution. It is exact, but it grows with the park.
- Letting derivations read the world and recording reads by hooking const accessors. Any read through another path would go unrecorded.
- Whole-store reads, so a derivation could still scan. One scan of the segment store would make every edit dirty everything again.
- Hand-written reach rules per unit kind. A missed reach gives a wrong world that no single function shows.

## How the path ground splits into units

The question is how the ground lines and corners, which groundOf derives for the whole layout, split into units whose values equal groundOf's.

layout.cpp derives the ground in four passes:

1. Each segment's straight line, from its two points.
2. Each point's bend, where exactly two segment ends with straight lines meet, and each point's paving reach along each of its segments, from its segments' directions, half widths, and edge radii.
3. Each corner's rounding. What a far end needs is the larger of its bend's tangent and its reach along the segment. The rounding reads this for each of its two segments' far ends.
4. Each segment's ground line, from its straight line and the roundings at its two ends.

Each pass reads only its own point or segment and their neighbours. Order matters in only two places:

- pavingReaches sorts a point's arms by direction, then by segment index. Segments are indexed in ascending key order, so ordering by key gives the same sort.
- Ends are counted per segment end, so a segment whose two points are the same counts twice at its point.

Arc halves sit in one shared buffer, by index. A unit has to hold its own halves. Both paths therefore keep the halves with each rounding.

So the ground splits into five steps:

- An index of segments by their two points.
- One straight line per segment.
- One set of arms per point: its bend and its reaches.
- One corner per point.
- One ground line per segment.

A segment's change dirties its own straight line, the arms of its two points and of the points one segment away, the corners up to two segments away, and the ground lines that meet those corners.

The world's readers of the ground all go through layoutOf and groundOf: parkSegments, parkCorners, parkSurface, parkEdges, parkTapers, and the box acceptance check's piecesNear. They can take one shared ground from the units whenever the units are current. Edits on a hypothetical layout keep calling groundOf.

## What the path ground costs today

Four Very Sleepy profiles of tpj_bench on full.park after lean-routes (build/perf/lean-routes-after-1.sleepy to 4) put these shares of resolveWorld's samples on deriving the ground:

| Part | Share of resolveWorld |
|---|---|
| fillGround | 3.0% |
| parkSegments | 2.1% |
| layoutOf | 0.2% |

The surfaces are much larger:

| Part | Share of resolveWorld |
|---|---|
| parkSurface | 18.8% |
| hideJoined inside it | 11.3% |
| buildSurface | 5.2% |

The rest of the large shares:

| Part | Share of resolveWorld |
|---|---|
| findConnections | 18.5% |
| Route distance | 21.6% |
| Furniture | 12.1% |
| Best seats | 10.7% |
| Carrying the guests | 9.1% |

Moving the ground onto units saves about 3% of a resolution on its own. Its value is the mechanism, and the ground lines every later unit kind reads. The surfaces come next, in unit-surface.

Sources: src/sim/world.h; src/sim/walk.cpp; src/sim/schema.h; src/sim/cycle.cpp; src/sim/park/layout.cpp; src/sim/park/intent.cpp; src/sim/park/surface.cpp; src/sim/park/edits.cpp; build/perf/lean-routes-after-1.sleepy to 4.sleepy.
