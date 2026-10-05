# Capability: Deterministic Simulation

## Summary

deterministic-simulation implements principle 10: the simulation is deterministic and runs independently of rendering. It also carries principle 1's guarantee. The saved park is intent plus simulation state, and everything else is derived.

It owns the world as a value. The world as a value covers:

- the tick and the fixed cycle each tick runs through;
- stable entity keys;
- one registered walk over all world state, used for copy, equality, hash, save, and load;
- keyed random draws;
- the simulation's own exp and log;
- the check that Windows and Linux simulate bit-identically (decision 0022).

This capability matters because it makes the simulation trustworthy:

- The tests that run on Windows show what players get on every release build.
- Every bug reproduces from a save.
- A preview's candidate copy behaves exactly like the committed park (principle 8).

Later milestones add replays, save migration, faster stepping, and sharper diagnosis of divergence. The capability never finishes.

## Foundation criteria

If two worlds are built identically and stepped the same number of ticks, their state hashes are equal at every tick.

The cross-build check builds tpj_scenarios with windows-debug, windows-release, and linux-release. It runs each build over the registered scenarios and every park file in tests/parks/, and fails if a release build's output differs from windows-debug's. It runs at release through scripts/release-check.sh (decisions 0030 and 0036). It first ran from the pre-push hook, whenever pushed commits touched anything tpj_sim's build depends on, until decision 0030 moved it to release.

A copy of a world equals the original. If both are stepped, they stay equal at every tick. Changing the copy leaves the original unchanged.

A save holds only intent and state, and no derived data. Loading a save and resolving it gives a world equal to the one saved. Saving the loaded world again gives an identical file, byte for byte.

The hash covers everything the walk covers. If any single registered value changes, the hash changes. If a component type is stored in the world but not registered with the walk, debug builds fail loudly. Nothing can escape copy, hash, or save.

A random draw depends only on its key: the world seed, entity key, purpose, tick, and index. The same key gives the same value on both builds, whatever order the draws are made in. Uniform draws pass a distribution check. Over many draws, weighted picks match their weights within a stated tolerance.

tpj_sim calls no C runtime transcendental function. A check on the built library's undefined symbols fails if one appears, and planting one proves that the check works.

The simulation's exp and log match reference values from a table. The cross-build script also checks that they are bit-identical across builds.

## Medium

This capability adds no fields or flows. It provides the registration, keys, and cycle that every other capability's state and systems use:

- Every capability registers its component types with the walk. Each type has a name, a class (intent, state, or derived), and functions to copy, compare, hash, encode, and decode it. The functions live in the owning module, so no other module sees a type's layout (principle 6).
- Every capability registers its step systems and its resolvers with the cycle. Each tick runs these steps in a fixed order (plans/shared-medium/CAPABILITY.md):
  1. Step the systems.
  2. Swap the shared-medium buffers.
  3. Apply the intent commands queued since the last cycle, in the order they were submitted.
  4. If intent changed, run the resolvers in their declared dependency order.
- All simulation code can use stable entity keys and keyed random draws. The first consumer is believable-guests' seeded softmax (decision 0019).
- Candidate copies serve legible-simulation's previews, through the resolution rules of decision 0025.
- effortless-building owns the park file as a player document. It owns the save and load commands, and intent as what the player authored. This capability's walk is the file's encoding.

## Principles

- Principle 10 is at risk from iteration order, floating-point differences, hidden inputs such as time or addresses, and unregistered state. Four checks guard it: the per-tick hash comparison within a build, the cross-build script, the undefined-symbol check, and the loud failure for unregistered types.
- Principle 1 is at risk if derived data leaks into a save, or if state is left out of one. Two things check it: the class on every registered type, and the round trip of save, load, and resolve, which must equal the original.
- Principle 6 is at risk if the walk exposes what a component holds. Only the owning module's registered functions touch a type's layout. The walk handles each type without seeing inside it.
- Principle 2 benefits from this capability. Any world at any tick can be copied, saved, and loaded, so every intermediate state can be inspected and reproduced.

## Dependencies

- tpj_sim with EnTT, and the fixed-tick skeleton in src/sim: met.
- Both toolchains reachable from WSL (cmake.exe for Windows and the Linux GCC build), so one script can run both: met.
- shared-medium (first-field-and-flow): its buffers register with the walk like any other state. Met.

## Foundation

The foundation is the world as a value, proven on synthetic registered component types with no park content. It needs:

- the cycle;
- stable keys;
- the registered walk, with copy, equality, hash, and text save and load;
- keyed draws;
- exp and log;
- the cross-build script.

This is the smallest version that makes every later capability's state copyable, hashable, and saveable with no further work.

It is useful on its own. The determinism and round-trip properties are proven once. Every capability that registers its types gets them. If you add registered state, put it in a park file in tests/parks/, where the park world checks cover it (docs/testing.md, decision 0037).

## Milestones

1. `world-as-value`: the tick cycle, with registered systems and resolvers, and a queue of intent commands applied between the swap and resolution. It adds stable entity keys and the registered state walk, with intent, state, and derived classes, and with copy, equality, and hash. It adds canonical text save and load, and keyed random draws with uniform and weighted-pick distributions. It ports musl's exp and log, and adds the undefined-symbol check and the cross-build script with its pre-push trigger. The milestone was tested on synthetic component types and scenarios. Member of the boxes-and-tubes slice. Depends on: none.

Later milestones come from the deepening candidates, once the slice shows where determinism is hardest to keep.

## Deepening candidates

- Divergence diagnosis: when hashes differ, report the registered type and entity that diverged first. Dump both worlds in readable form at that tick, as Factorio does.
- Replays: a seed, a starting save, and the log of intent changes reproduce a session. Replays serve bug reports and regression tests.
- Save versioning and migration: older park files still load after registered types change. Gated on: a save worth keeping across a format change.
- Further simulation math: port pow, trigonometry, and other functions the same way, as they are needed. Gated on: a simulation feature that needs one.

## Open questions

- Whether 3000 ticks run quickly enough under the sanitizers for the slice's criteria. Measurement settles this once the slice's members step real content. This capability provides the harness.

## Research notes

- On x86-64 with SSE2, contraction off, and GCC on both builds, only the C runtime's transcendental functions still differ between builds. Porting musl's exp and log closes that gap.
- Counter-based hashing, in the style of noise functions, gives keyed draws that need no stored state.
- Factorio computes a CRC of its whole state every tick. It shows why the hash must cover everything and be checked every tick.
- EnTT has no way to clone a registry, so copying goes through a registered walk.
- std::to_chars gives a canonical text encoding for doubles.

The details are in RESEARCH.md.
