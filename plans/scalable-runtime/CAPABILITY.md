# Capability: Scalable Runtime

## Summary

scalable-runtime keeps the game running smoothly as parks get bigger. It covers the simulation's ticks and resolutions, and the work each frame does to draw the park, its overlays, and its previews.

It is an engineering capability (decision 0028). A player with an infinitely fast machine would never notice it. On a real machine, it decides whether a big park still plays smoothly.

It owns two things:

- measuring where the time goes, on parks as large as the game means to support;
- the speedups that the measurements point to.

A speedup changes how fast the game gets an answer, not the answer itself. There is one exception. Decision 0031 rebuilt one metric to follow its movers, and that metric's answer may change in its last bits, under the rule its spec states.

This matters because a park can get slow without anyone noticing. Drawing one 3 km guest path pushed the food overlay's rebuild to 38 ms a tick on windows-debug, past the 33 ms between ticks. On windows-release it took 2 ms, with one shop. Nothing measured it until someone played it, and nothing yet says what it costs with 30 shops.

## Foundation criteria

Stress parks live in tests/parks/stress/. The cross-build check and the integration tests don't read them.

The main stress park is the full park. It is 256 m across and holds:

- about 2,000 guests;
- 30 shops, supplied by a few depots over backstage paths;
- about 4 km of guest path, with about 100 junctions.

A generator makes the full park from a seed and its sizes. The same inputs always give the same file, and if the rules change, regenerating the park picks up the change. The generator creates guests through a public function of the guests module, not by writing the module's private components (principle 6). It saves the guests in the park file with the rest of the state. A run therefore starts with a full park instead of waiting for guests to arrive, and the park holds about 2,000 guests for every run.

Next to the full park are incident parks. Each one is a park that once played slowly, and each is named for what it holds, not for how it played. The first is winding-path.park, with a single 3 km guest path.

A headless runner loads a park and times each stage of the gameplay runtime through public headers:

- stepping ticks, with the median, the spread, and the worst tick;
- one resolution;
- making a candidate for an edit;
- building the food overlay;
- building the park and guest meshes.

The runner is the only code that reads the clock for this. Running it leaves the world's hash the same as stepping alone would.

A script builds windows-release and windows-debug and launches the runner several times on each stress park. For each park and stage, it prints each build's median and spread. Release decides what counts as slow, and debug is reported next to it.

The script also compares two reports. It marks a change as real only when the change is bigger than the spread both reports measured. Two reports of an unchanged tree show no real change.

No test, hook, or check fails because of a timing. Timings are reported and compared, but they never block anything.

Each milestone in this capability includes the script's before-and-after comparison on the stress parks. It must not change observable behavior: existing tests pass unchanged, and if it touches the simulation, tpj_scenarios's windows-debug output is identical before and after (decision 0030).

The exception is a metric rebuilt under decision 0031. Its spec may state a decay rule that differs from the old one in the last bits, and its saved form changes. Its tests are rewritten. Saves made before and after differ only in that metric's sections.

## Medium

This capability adds, samples, and publishes no fields or flows. It measures and speeds up the code that every other capability's fields and flows run in.

Its speedups reach into code that other capabilities own:

- shared-medium's sampling;
- navigable-networks' route distance;
- deterministic-simulation's resolution and candidates;
- legible-simulation's overlay.

Each speedup keeps that module's public contract. If it has to change the contract, it updates the module's SPEC.md in the same work.

If a fix would change what the player sees, such as an exact overlay band or a limit on how far route distance reaches, it belongs to the capability whose quality it changes. Hand it back to that capability.

## Principles

- Principle 10: the simulation is deterministic and runs independently of rendering. Speedups in the simulation keep tpj_scenarios's output identical before and after, line for line. The only exception is a metric whose rule decision 0031 restates, as the foundation criteria allow. The cross-build check must pass at release (decision 0030). Simulation code never reads the clock. Only executables' entry points and the runner do, and src/scenarios/SPEC.md already keeps timing out of compared output.
- Principle 1: everything visible is derived. If a speedup keeps derived state between frames, such as an overlay that is rebuilt only when its inputs change, give that state one owner and one path by which changes to its sources reach it (decision 0027). Show that the kept state equals a fresh rebuild in the way docs/testing.md allows.
- Principle 8: every change can be previewed before it is committed. A faster candidate must still equal, by worldsEqual, the world that committing the edit gives.

## Dependencies

- Decision 0028, engineering capabilities: met.
- Park files and loading, which already save every guest's state: met.
- A public function of the guests module that creates a guest: met. measured-runtime's created-guests added addGuest and updated src/sim/guests/SPEC.md.
- The first incident park: met. measured-runtime moved the untracked parks/lowfps.park to tests/parks/stress/winding-path.park.
- The layer table and its check: met (sound-architecture's layered-dependencies). The runner needed its own unit, because tpj_scenarios links only the simulation and shares a layer with render and legible, so it can't time the overlay or the meshes. measured-runtime added that unit to cmake/layers.txt, above render and legible and below the app.
- The app's scene sync as the one owner of the preview and overlay meshes: met (sound-architecture's composed-app).
- The windows-release preset: met.

## Foundation

The foundation is measurement:

- stress parks as big as the game means to support;
- a runner that times each runtime stage on them;
- a report that tells a real change from noise.

It is useful on its own. Before any speedup is chosen, the first report shows which costs matter with 2,000 guests.

After the foundation, the tick comes next, because a profile of the full park found it over budget. Then comes the overlay that started this capability. The report measures each one before and after.

## Milestones

1. `measured-runtime`: the stress parks, the full park's generator, the headless runner that times each runtime stage, and the script that reports both builds' medians and spreads and compares two reports against their noise. Depends on: none.
2. `indexed-sampling`: the report steps through minutes of play, marks every stage that goes over its budget, and times the app's frames. Walking guests read only the route distance entry they are following between choices, without allocating. With this, the full park runs one tick per frame in the app on windows-release. Depends on: milestone 1, and a Windows profile that put four fifths of a full-park tick in guests' route distance samples.
3. `following-footfall`: the shared medium gets kept entries. A system changes a kept entry at one place, and the entry stays readable until it changes again. Hungry footfall moves onto kept entries, so its tick work grows with the number of guests instead of the size of the guest network (decision 0031). Depends on: milestone 2. Lands before paths-and-plazas grows the guest network.
4. `affordable-overlay`: the report times the app's frames with the food overlay shown. An availability overlay, starting with food's, finds each source's offer once per build. It reads route distance with the shared medium's sample at many places, which shares each source's lookups among the places on one edge. A build's cost no longer grows with samples times shops, and a later overlay of the same kind, such as thirst, gets both speedups. Depends on: milestones 2 and 3, and the report after following-footfall still showing the overlay costly on windows-release on the full park, at 37.4 ms.
5. `allocation-free-loops`: the allocation check of decision 0041 in tests/checks, covering the work the app repeats in steady play, and the fixes that make that work reuse its memory: the world's storage, the tick, the scene's sync, the kept preview, the tools, and the panels. Depends on: milestone 1.
6. `changing-previews`: previews of an edit that changes every frame, such as dragging a path, stop allocating. Resolution reuses its storage, or resolves only what the edit touches. Depends on: milestone 5.

## Deepening candidates

Unordered pool this capability draws later milestones from.

- Overlay kept between builds: rebuild the overlay only when the guest network, route distance, or a reachable source's offer changes, compared by content, so a quiet park rebuilds nothing. Gated on: the report showing that rebuilding every tick is still costly after affordable-overlay.
- Overlay rebuilt by region: when one shop's offer changes, rebuild only the band within the route distance where that shop's term falls to zero, and keep the mesh in chunks. Then a park with many busy shops doesn't rebuild everything every tick. Gated on: the report showing whole rebuilds costly on the full park after affordable-overlay.
- Candidates off the frame's thread: copy the world on the main thread, resolve the candidate on a worker, and show the last finished one. Gated on: incremental resolution still leaving a candidate too slow for a frame.
- Drawing measured: measure the GPU's share of a frame from the app's --frames runs. Gated on: a slow frame that the CPU stages don't explain.
- Faster stepping: run systems in parallel or in batches, with bit-identical results. Gated on: the report showing ticks costly on the full park.
- Resolution across cores: derive each source's route distance in parallel, with results identical to deriving them one after another. Gated on: the report showing resolution, and with it candidates, costly on a park with plazas.
- Instanced guests: draw every guest from one shared mesh with a small per-guest instance buffer, instead of rebuilding and re-uploading a mesh of all guests every tick. Gated on: the report showing the guest mesh and its upload as a real share of the app's frame after indexed-sampling, or guests being drawn as models instead of simple shapes.
- Ground field lookup by bucket: a ground field's by-cell lookup is rebuilt after each swap by sorting every entry, about 160 µs a tick for guest presence on the full park. Group entries into their cells in one pass instead.
- Faster component store: replace keyed-storage's first store with one that keeps key order but iterates densely, such as a key-sorted array with a key-to-index map, or a B-tree (decision 0039). Gated on: keyed-storage merged, and the report showing component lookup or iteration as a real share of a tick.
- Guests found by type: parkGuests walks every live key and checks each one for a guest, twice a tick and again in each resolution. Iterate the guest store in key order instead. Gated on: keyed-storage merged.
- full.park holds its crowd through hours of play: its 30 counters serve about 600 meals and drinks a game hour, well below what its 2,000 guests want, so thirst sends them home within a few game hours. Give it enough counters for its guests. Gated on: a run long enough to show it, such as play or a longer runtime report.

## Open questions

None.

## Research notes

- Factorio and OpenRCT2 benchmark from saved worlds, headless, on the shipped build. They report per-tick distributions and measure rendering separately.
- Planet Coaster's players cap parks near 7,000 guests, and Planet Coaster 2 caps them at 6,000, in parks much larger than 256 m. Guest count is what slows them down. In this game, guests took three quarters of a tick in supply.park's profile before affordable-sampling.
- Timings vary by tens of percent between launches, and changes under about 5% are rarely distinguishable from noise. Repeated launches, medians, and a measured noise floor separate real changes from noise. That suits a report better than a gate.

The details are in RESEARCH.md.
