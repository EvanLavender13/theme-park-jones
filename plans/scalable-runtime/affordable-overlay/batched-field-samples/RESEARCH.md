# Research: batched-field-samples

## Where does the overlay build spend its time after prepared-availability?

The report after prepared-availability puts the full park's food overlay build at a median of 28.6 ms on windows-release, and the app's frames with the overlay shown at 39.4 ms, against 6.0 ms without it. A Very Sleepy profile of tpj_bench on the full park (build/windows-profile/overlay-after1.sleepy, 1.25 s of samples over a 1.27 s run) puts 0.44 s under buildBandOverlay. About 81% of it is sampleField of guest route distance at each place, about 10% availabilityValues' own loop, and about 5% the band's geometry. Within the sampling, positionsAtAny, finding each source's entries at an edge's two end nodes, is about 32% of the build, positionsInside, entriesByPlace, and stopPlaces about 14%, heap allocation and freeing about 15%, and sampleRouteEdge, the arithmetic that differs between places, about 4%.

So most of what remains is still repeated lookups: every place strictly inside an edge looks up each source's entries at the same two nodes and along the same stretch, and only its offsets from the ends differ.

## How can places on one edge share lookups, and keep every sample exact?

sampleSlotInEdge (src/sim/medium/field.h) does two things for a source at a place inside an edge: it gathers the source's entries at the edge's ends and strictly inside it into an EdgeSample, which depends on the edge alone, and it sets the place's offsets and calls the field's sampleEdge. Splitting the two lets a run of consecutive places inside one edge gather each source's EdgeSample once and call sampleEdge at each place's offsets. Each place's entries are then built from the same gathered values, by the same rule, in the same source order, so they equal what sampling the place alone gives, bit for bit. A field without an edge rule reads entries at exactly the place, which has nothing to share, and a place at a node reads that node's entries, as today.

Making a single place's sample the batch of one place keeps one code path: sampleField, sampleResolvedField, and the kept field's sample all go through it, so the guests' choices, the operations' offers, and the scenarios exercise the batched code at every tick, and tpj_scenarios --compare shows any difference it makes. The work for one place is what it is today: one gathering and one rule call per source.

Rejected: grouping places by edge across the whole list, rather than consecutive runs. The band gives places along each line in order, so consecutive runs already share each edge's lookups, and grouping would hold every edge's samples at once. Caching each source's EdgeSample per edge between calls. It would be kept state whose sources change at every tick for stepped fields, and principle 1 asks a kept value for one owner and a test against a fresh rebuild.

## What is left after batching?

Per place, the batch still resolves the place, calls sampleEdge once per source, which returns a vector for route distance, and computes the availability's terms. The profile puts sampleRouteEdge at about 4% and availability's loop at about 10%, with part of the 15% in allocation from both. An estimate, not a measurement: the build falls to about a third of 28.6 ms, and the app's frames with the overlay shown to about 20 ms. If the report shows otherwise, the capability's candidate of keeping the overlay between builds is the next lever.

Sources: build/runtime-report/prepared-availability-after.txt — the report; build/windows-profile/overlay-after1.sleepy — the profile; src/sim/medium/field.h and src/sim/medium/kept_field.h — sampling; src/sim/routes/route_distance.cpp — sampleRouteEdge; src/legible/availability.cpp — the availability's loop.
