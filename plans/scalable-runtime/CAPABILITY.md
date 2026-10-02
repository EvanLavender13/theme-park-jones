# Capability: Scalable Runtime

## Summary

scalable-runtime deepens how gently the gameplay runtime's costs grow with the size of the park: the simulation's ticks and resolutions, and the work a frame does to show the park, its overlays, and its previews. It is an engineering capability (decision 0028). A player with an infinitely fast machine would never notice it, and on a real one it decides whether a park they built keeps playing smoothly. It owns the measuring that finds where time goes, on parks as large as the game means to support, and the speedups that measuring points at. Every speedup changes how fast an answer comes, never what the answer is. Its value is that building a bigger park never turns into a slower game unnoticed: drawing one 3 km guest path took the food overlay's rebuild to 38 ms a tick on windows-debug, past the 33 ms between ticks, and 2 ms on windows-release, with one shop. Nothing measured it until it was played, and nothing yet says what it costs with 30 shops.

## Foundation criteria

- Stress parks live in tests/parks/stress/, outside the files the cross-build check and the integration tests read. One is the full park: in the 256 m park, about 2,000 guests, 30 shops supplied by a few depots over backstage paths, and about 4 km of guest path with about 100 junctions. The generator creates its guests through a public function of the guests module, never by writing its private components (principle 6), and writes them into the park file with the rest of its state, so a run loads a full park and never waits for guests to arrive, and the park holds about 2,000 guests through every run the runner makes of it. It is generated from a seed and its sizes, so making it again from the same inputs gives the same file, and making it again after a rules change takes the change up. Beside it are incident parks, each a park that once played slowly, starting with lowfps.park and its single 3 km guest path.
- A headless runner loads a park and times each stage of the gameplay runtime through public headers: stepping ticks, with the median, the spread, and the worst tick; one resolution; making a candidate for an edit; building the food overlay; and building the park and guest meshes. It is the only code that reads the clock for this, and running it leaves the world's hash what stepping alone would give.
- A script builds windows-release and windows-debug, launches the runner several times on each stress park, and prints, per park and stage, each build's median and spread. Release decides what is slow, and debug is reported beside it.
- The script compares two reports and marks a change clear only when it exceeds the spread both reports measured. Two reports of an unchanged tree mark no change clear.
- No test, hook, or check fails on a timing. Timings are reported and compared, never gated.
- Each milestone this capability ships includes the script's comparison before and after its change on the stress parks, and changes no observable behavior: existing tests pass unchanged, and the cross-build check's output is identical wherever the simulation is touched.

## Medium

This capability introduces, samples, and emits no fields or flows. It measures and speeds up the code that every other capability's fields and flows run in.

Its speedups reach into code other capabilities own: shared-medium's sampling, navigable-networks' route distance, deterministic-simulation's resolution and candidates, and legible-simulation's overlay. Each keeps its module's public contract, or changes it only with that module's SPEC.md in the same work. A remedy that would change what the player sees, such as an exact overlay band or a cap on how far route distance reaches, belongs to the capability whose quality it changes, and is handed back to it.

## Principles

- Principle 10: the simulation is deterministic and runs independently of rendering. Speedups in the simulation keep the cross-build check's output identical, line for line, and simulation code never reads the clock: only executables' entry points and the runner do (src/scenarios/SPEC.md already keeps timing out of compared output).
- Principle 1: everything visible is derived. A speedup that keeps derived state between frames, such as an overlay rebuilt only when its inputs change, gives it one owner and one path by which a change of its sources reaches it (decision 0027), and a test checks that the kept state equals a fresh rebuild.
- Principle 8: every change can be previewed before it is committed. Faster candidates still equal, by worldsEqual, the world the committed edit gives.

## Dependencies

- Decision 0028, engineering capabilities: met.
- Park files and loading, which already save every guest's state: met.
- A public function of the guests module that creates a guest: unmet. measured-runtime adds it, with src/sim/guests/SPEC.md in the same work.
- lowfps.park, the first incident park: held untracked in parks/ until measured-runtime moves it into tests/parks/stress/.
- The layer table and its check: met (sound-architecture's layered-dependencies). The runner's own module is not: tpj_scenarios links the simulation alone and shares a layer with render and legible, so it cannot time the overlay or the meshes. measured-runtime adds a new unit to cmake/layers.txt above render and legible, beside the app's.
- The app's scene sync as the one owner of the preview and overlay meshes: met (sound-architecture's composed-app).
- The windows-release preset: met.

## Foundation

The foundation is the measuring: stress parks the size the game means to support, a runner that times each runtime stage on them, and a report that tells a real change from noise. It produces value on its own, since its first report says which costs matter at 2,000 guests, before any speedup is chosen. The overlay that set this off comes next, measured before and after by it.

## Milestones

1. `measured-runtime`: the stress parks, the full park's generator, the headless runner timing each runtime stage, and the script that reports both builds' medians and spreads and compares two reports against their noise. Depends on: none.
2. `affordable-overlay`: the food overlay rebuilt only when the guest network, the route distance field, or a reachable shop's offer changes, and each shop's offer found once per build rather than at every sample, so an idle park rebuilds nothing and a build's cost no longer multiplies samples by shops. Depends on: milestone 1, and its report showing the overlay costly on windows-release on the full park. A build finds every shop's offer at every sample, so its release cost of 2 ms with one shop is expected to grow many times over with 30.

## Deepening candidates

Unordered pool this capability draws later milestones from.

- Overlay rebuilt by region: when one shop's offer changes, rebuild only the band within the route distance at which that shop's term falls to zero, with the mesh kept in chunks, so a park with many busy shops does not rebuild everything every tick. Gated on: the report showing whole rebuilds costly on the full park after affordable-overlay.
- Entries indexed by place: sampling that visits only the entries at the sampled node or edge, not every entry of every source. Gated on: the report showing sampling's scan matters on the full park.
- Incremental resolution: run only the resolvers an applied command affects, in dependency order, so a candidate costs what its edit touches. Gated on: the report showing candidates costly on the full park.
- Candidates off the frame's thread: copy the world on the main thread and resolve the candidate on a worker, showing the last finished one. Gated on: incremental resolution leaving a candidate too slow for a frame.
- Sampling without allocation: sampleField builds its list of slots and each EdgeSample's vectors on every sample. Gated on: the report attributing a large share of sampling to allocation.
- Drawing measured: the GPU's share of a frame, from the app's --frames runs. Gated on: a slow frame the CPU stages do not explain.

## Open questions

None.

## Research notes

- Factorio and OpenRCT2 benchmark from saved worlds, headless, on the shipped build, report per-tick distributions, and measure rendering separately.
- Planet Coaster's players cap parks near 7,000 guests and Planet Coaster 2 caps them at 6,000, in parks far larger than 256 m; guest count is what slows them, and guests took three quarters of a tick in supply.park's profile before affordable-sampling.
- Timings vary by tens of percent between launches, changes under about 5% are rarely distinguishable from noise, and repeated launches, medians, and a measured noise floor tell real changes apart, which suits a report rather than a gate.

Depth is in RESEARCH.md.
