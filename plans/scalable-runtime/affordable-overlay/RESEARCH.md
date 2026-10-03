# Research: affordable-overlay

## Where does the full park's overlay build spend its time?

The runtime report after following-footfall puts the full park's food overlay build at a median of 37.4 ms on windows-release, over the 33 ms a frame has, and winding-path.park's at 1.9 ms. The app rebuilds the overlay whenever it rebuilds the ghost mesh, which is at every tick, since the kept preview is made again at every tick. So with the overlay shown, the full park asks for about 30 builds a second, more than a second of work for each second of play.

A Very Sleepy profile of tpj_bench on the full park, built for windows-release with -g and frame pointers, puts 0.59 s under buildFoodAvailabilityOverlay over its builds. About 53% of it is sampleField of guest route distance at each sample, mostly positionsAtAny finding each source's entries at the sampled edge's two end nodes, about 37% is finding each source's food offer again at each sample (sourceEntryAtNode, sourceSlot, World::findEntity, firstAnchoredNode), and about 8% is foodAvailability's own loop. The band's geometry, its sample distances, rows, and cones, is about 1%. A build samples about 7,000 places, each against 31 sources.

So the cost is the same lookups repeated. Every sample on one edge finds the same entries for each source at the same two nodes and on the same stretch between them, and every sample finds the same offer for each shop, since offers do not vary by place. Only the offsets along the edge differ between samples.

## What makes a build's cost stop multiplying samples by sources?

Two lookups can be made once rather than at every sample. A shop's offer depends on the shop alone, so a build can find each shop's offer once and look it up by source. A field's entries for one source on one edge, the EdgeSample sampleSlotInEdge assembles (src/sim/medium/field.h), are the same for every place strictly inside the edge except for FromOffset and ToOffset, so a sample of many places on one edge can assemble each source's EdgeSample once and call the field's sampleEdge rule at each place's offsets. That is a property of the medium, not of food: it holds for every field with an edge rule, route distance, hungry footfall, and any field added later, and for fields without one, which match entries at the exact place.

Sampled this way, a build's lookups grow with sources times edges rather than sources times samples. The full park's guest paths are sampled about every meter, so its edges hold tens of samples each, and the lookups that are about 90% of today's build would shrink by about that factor. The equality to keep is exact: the value at each place must be the one foodAvailability gives there, bit for bit, since the tooltip lists that place's terms and their sum is the shade (principle 8, legible/SPEC.md). Batching changes which lookups run, not the arithmetic of any value, so this holds when each place's value is computed by the same operations in the same order.

Rejected: caching route distance per sample place between builds. The places are fixed only while the network is, and the values change whenever an offer does, which is most ticks on a busy park, so the cache would be rebuilt nearly as often as the overlay. Interpolating colors between fewer samples. It changes what the player sees, which belongs to legible-simulation, and the batched lookups make the 1 m spacing affordable anyway.

## Should the overlay be kept and rebuilt only when its inputs change?

The capability's milestone line proposes rebuilding the overlay only when the guest network, route distance, or a reachable shop's offer changes, so an idle park rebuilds nothing. Offers are stepped entries, published every tick (src/sim/operations/operations.cpp), and an offer's Wait follows its queue, so on a park with 2,000 guests and 30 shops some offer changes most ticks. Keeping would therefore save the most where builds are already cheap, on small or idle parks, and little on the full park, where the build itself must be cheap. Knowing that inputs are unchanged also needs the inputs compared by content, since a candidate world is a fresh copy at every tick: each source's route distance entries and each shop's offer. That costs much less than a build, but it is a second path to the overlay's value that must agree with the first.

Principle 1 asks that kept derived state have one owner and one path by which its sources' changes reach it, and a test that the kept state equals a fresh rebuild (plans/scalable-runtime/CAPABILITY.md). The app's SceneSync already keeps what each mesh was built from, so a kept overlay would follow that pattern. It is worth doing once the report shows rebuilding at each tick still costly after builds are batched.

Rejected: a version counter per field, bumped when its entries are published. Stepped fields are published at every tick and candidates are resolved afresh, so counters would change when the content does not, and they would be state the world hashes and saves.

## What part of this is a foundation for every overlay?

Every overlay planned so far shades places on a network by a value derived from fields: the food overlay today, an overlay for any field (legible-simulation's deepening candidates), and docs/design-notes.md's rule that every field be viewable as one. The paths-and-plazas slice's ground-overlay replaces the band along path lines with shading of the paved ground by the value at the nearest network place, so the band's geometry is the part of today's overlay that will not last, while asking for the values at many network places will.

That suggests the foundation is the value side: an overlay gives its values for a run of places on one edge in one call, after preparing whatever it reads once per build, through the medium's batched sampling. Any geometry, the band today or the paved ground later, collects its sample places, groups them by edge, and asks. The band builder already takes its value as a function, so it is food's only in name and its ramp's scale. A menu of overlays, a table naming each with its values and scale and the app choosing among them, has one entry until a second overlay exists, and its shape is that second overlay's planning to settle.

Rejected: keeping each overlay's geometry and re-uploading only colors, or a scalar per vertex mapped through a ramp in the shader. Geometry is about 1% of a build today, so neither saves time now; ground-overlay, whose geometry may cost more, is where to weigh them.

Sources: build/runtime-report/following-footfall-after2.txt — the report's medians; build/windows-profile/overlay.sleepy — the profile; src/sim/medium/field.h — sampleSlots, sampleSlotInEdge, and EdgeSample; src/legible/food.cpp, src/render/food_overlay.cpp, and src/views/food_overlay.cpp — the build; src/app/SPEC.md — when the app rebuilds the overlay; plans/slices/paths-and-plazas/SLICE.md — ground-overlay; plans/legible-simulation/CAPABILITY.md — an overlay for any field.
