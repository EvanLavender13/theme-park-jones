# Research: lean-overlay-rebuild

## Where does a rebuild spend its time after batched-field-samples?

After batched-field-samples, the full park's food overlay build takes 10.9 ms on windows-release. With the overlay on, the app's worst frame is 31.5 ms, against 13.3 ms with it off (build/runtime-report/batched-field-samples-after.txt).

Five Very Sleepy runs of the windows-release tpj_bench on the full park, one tick each (build/perf/overlay-after2-1.sleepy to -5.sleepy), put 0.94 s under buildBandOverlay:

- 45%: allocating and freeing memory. sampleRouteEdge returns a new one-entry vector for every place and source (0.20 s allocating, 0.15 s freeing). availabilityWith builds each place's Contributions list, which availabilityValues throws away after reading Value (about 0.06 s allocating, plus freeing).
- 18%: availabilityWith's own work, including a binary search of the found offers for every entry.
- 13%: finishRun's own work. Most of it is copying each entry into the run's per-place lists and reading it back out.
- 11%: sampling lookups and the route rule (positionsAtAny 5.7%, sampleRouteEdge 3.8%, positionsInside 1.3%).
- 12%: the band's geometry. buildBandOverlay allocates a distance list and a row list for every carrier, and grows the mesh's vertex and index lists without reserving them.

A 10-second Very Sleepy run of the windows-release app on the full park with the overlay shown (build/perf/app-overlay.sleepy) puts 3.5 s of 10.2 s in the overlay build and 0.01 s in uploading it. The upload doesn't matter.

The same app profile puts 2.9 s in summarizePark, which the debug panel calls every frame. It costs about 3 ms a frame whether the overlay is on or off, because shopRecord scans every box for each shop. It isn't the overlay, so this feature leaves it alone.

## What does the fix remove?

- The route rule writes into a list the sampler gives it and reuses (`sampleEdge(sample, out)`), instead of returning a new vector. Hungry footfall's rule changes the same way.
- sampleFieldAtPlaces hands each entry to the visitor as soon as it's sampled, along with its place's index. It no longer copies entries into per-place lists first. Within a run, entries arrive one source at a time, so the next entry usually has the same source as the last one.
- availabilityValues adds each entry's term straight into its place's value. It caches the last source's offer, so the binary search runs once per source per run, not once per entry. It never builds a Contributions list. The term is computed by the same function availabilityAt uses, and each place still adds its terms in ascending source order, so the values stay bit for bit what availabilityAt gives.
- buildBandOverlay reuses one distance list and one row list for all carriers, and reserves the mesh before filling it.

## How fast will it get?

This is an estimate, not a measurement. Removing the allocations and the copying should take the build to about 4–5 ms. About 220,000 place and source pairs (7,000 places, about 31 sources: 30 shops and the entrance) still each need a few additions, a division, and a lookup on the discount curve. Together with the geometry, that puts the floor near 3 ms. Getting well under 3 ms means computing fewer pairs, which is the Monte Carlo candidate in the milestone.

The app's median frame with the overlay shown (4.0 ms) is already below its median with it off (5.9 ms). So the median says nothing about the overlay. What the player feels is the hitch on frames with a tick, which shows up in the worst frame.

## Rejected

Keeping the band's geometry between rebuilds and changing only the colors. The milestone requires a fresh build every time, and the geometry is only 12%.

Skipping sources too far away to contribute anything. It saves little on the full park: most shops are within the curve's four minutes of most places. It would also put availability's curve into the sampler.
