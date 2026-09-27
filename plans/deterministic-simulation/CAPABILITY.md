# Capability: Deterministic Simulation

## Summary

deterministic-simulation implements principle 10: the simulation is deterministic and runs independently of rendering. It also carries principle 1's guarantee that the saved park is intent plus simulation state, with everything else derived.

It owns the world as a value. That means:

- the tick and the fixed cycle each tick runs through;
- stable entity keys;
- one registered walk over all world state, serving copy, equality, hash, save, and load;
- keyed random draws;
- the simulation's own exp and log;
- the check that Windows and Linux simulate bit-identically (decision 0022).

Its value is trust. Tests on Linux are evidence for what a player sees on Windows. Every bug reproduces from a save. A preview's candidate copy behaves exactly like the committed park (principle 8). It deepens with replays, save migration, faster stepping, and sharper divergence diagnosis, and never finishes.

## Foundation criteria

- Two worlds built identically and stepped the same number of ticks have equal state hashes at every tick. The cross-build script gives equal hash sequences from the Windows and Linux builds for every registered scenario. The pre-push hook runs the script whenever the pushed commits touch anything tpj_sim's build depends on: src/sim, src/core, cmake/, the top-level CMakeLists.txt and CMakePresets.json, the scenarios, or the script itself.
- A copy of a world equals the original. Stepping each gives equal worlds at every tick, and changing the copy never changes the original.
- A save holds only intent and state, never derived data. Loading a save and resolving gives a world equal to the one saved, and saving the loaded world again gives an identical file, byte for byte.
- The hash covers everything the walk covers: changing any single registered value in a randomized synthetic world changes the hash. A component type that is stored in the world but not registered with the walk fails loudly in debug builds, so nothing can escape copy, hash, or save.
- A random draw depends only on its key (world seed, entity key, purpose, tick, index). The same key gives the same value on both builds whatever order draws are made in. Uniform draws pass a distribution check, and weighted picks match their weights within a stated tolerance over many draws.
- tpj_sim calls no C runtime transcendental function. A check on the built library's undefined symbols fails when one appears, and it is proven by planting one. The simulation's exp and log match reference values from a table, and are bit-identical across builds as part of the cross-build script.

## Medium

This capability introduces no fields or flows. It is the ground they live on:

- Every capability registers its component types with the walk. Each type has a name, a class (intent, state, or derived), and functions to copy, compare, hash, encode, and decode it. The functions live in the owning module, so another module never sees a layout (principle 6).
- Every capability registers its step systems and its resolvers with the cycle. Each tick runs in a fixed order: step the systems, swap the shared-medium buffers, apply the intent commands queued since the last cycle in the order they were submitted, then run resolvers in their declared dependency order if intent changed (plans/shared-medium/CAPABILITY.md).
- Stable entity keys and keyed random draws are available to all simulation code. believable-guests' seeded softmax (decision 0019) is the first consumer.
- Candidate copies serve legible-simulation's previews, through the resolution rules of decision 0025.
- effortless-building owns the park file as a player document: the save and load commands, and intent as what the player authored. The encoding of the file is this capability's walk.

## Principles

- Principle 10 is at risk from iteration order, floating-point differences, hidden inputs such as time or addresses, and unregistered state. It is checked by per-tick hash comparison within a build, the cross-build script, the undefined-symbol check, and the loud failure for unregistered types.
- Principle 1 is at risk when derived data leaks into a save or state is left out of one. It is checked by the class on every registered type, and by the round trip of save, load, and resolve that must equal the original.
- Principle 6 is at risk if the walk exposes component contents. Only the owning module's registered functions touch a type's layout, and the walk handles types opaquely.
- Principle 2 benefits here: any world at any tick can be copied, saved, and loaded, which makes every intermediate state inspectable and reproducible.

## Dependencies

- tpj_sim with EnTT, and the fixed-tick skeleton in src/sim: met.
- Both toolchains reachable from WSL, cmake.exe for Windows and the Linux GCC build, so one script can run both: met.
- shared-medium: its buffers register with the walk like any other state. It is planned (plans/shared-medium/CAPABILITY.md) and builds after this capability.

## Foundation

The foundation is the world as a value, proven on synthetic registered component types with no park content. It needs the cycle, stable keys, the registered walk with copy, equality, hash, and text save and load, keyed draws, exp and log, and the cross-build script. That is the smallest version that makes every later capability's state automatically copyable, hashable, and saveable. It produces value on its own: the determinism and round-trip properties are proven once, and every capability that registers its types inherits them.

## Milestones

1. `world-as-value`: the tick cycle with registered systems and resolvers and a queue of intent commands applied between the swap and resolution, stable entity keys, the registered state walk with intent, state, and derived classes, copy, equality, and hash, canonical text save and load, keyed random draws with uniform and weighted-pick distributions, a port of musl's exp and log, the undefined-symbol check, and the cross-build script with its pre-push trigger. All of it is tested on synthetic component types and scenarios. Member of the boxes-and-tubes slice. Depends on: none.

Later milestones are drawn from the deepening candidates once the slice shows where determinism strains.

## Deepening candidates

- Divergence diagnosis: when hashes differ, report which registered type and entity diverged first, and dump both worlds readably at that tick, as Factorio does.
- Replays: a seed, a starting save, and the log of intent changes reproduce a session, for bug reports and regression tests.
- Save versioning and migration: older park files load after registered types change. Gated on: a save worth keeping across a format change.
- Binary encoding: a compact encoding behind the same walk. Gated on: parks large enough for text saves to be slow.
- Faster stepping: parallel or batched systems that keep bit-identical results. Gated on: profiling showing the tick is the bottleneck.
- Further simulation math: pow, trigonometry, and others ported the same way as they are needed. Gated on: a simulation feature that needs one.

## Open questions

- Which medium contents count as state and which as derived? Field entries published while stepping and still readable at the start of a tick look like state. Entries produced by resolution look like derived data. A field can hold both. Resolved while planning shared-medium's first-field-and-flow.
- Whether 3000 ticks runs quickly enough under the sanitizers for the slice's criteria. Resolved by measurement once the slice's members step real content. This capability provides the harness.

## Research notes

- On x86-64 with SSE2, contraction off, and GCC on both builds, only the C runtime's transcendental functions still differ. Porting musl's exp and log closes that gap.
- Counter-based, noise-style hashing gives keyed draws with no state.
- Factorio's per-tick whole-state CRC shows why the hash must cover everything and be checked every tick.
- EnTT has no registry clone, so copying goes through a registered walk.
- std::to_chars gives a canonical text encoding for doubles.

Depth is in RESEARCH.md.
