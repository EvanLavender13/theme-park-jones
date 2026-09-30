# Milestone: World as Value

Slice: boxes-and-tubes

## Summary

world-as-value turns the skeleton's tick counter into a world that can be copied, compared, hashed, saved, and loaded through one registered walk. It adds the deterministic cycle every tick runs through, stable entity keys, keyed random draws, the simulation's own exp and log, and the check that the Windows and Linux builds simulate bit-identically. Everything is proven on synthetic component types and scenarios, with no park content. It comes first in the slice because every other member registers its state with this walk and runs inside this cycle. Once it lands, each later capability's state is copyable, hashable, and saveable with no further work, and its previews and tests can trust that Linux results are what the player sees on Windows.

## Acceptance criteria

1. Every entity in the world has a stable key that is unchanged by copy, save, and load. Entities created by commands or by stepping take their key from a counter saved with the world, and a key is never reused after its entity is destroyed. Resolvers never draw from the counter. They either add components to an existing entity, or create an entity whose key is derived from an existing entity's key, a purpose, and an index, in a key range disjoint from the counter's, reusing the entity when one already holds that key, as it does when a save carries state for it. So load plus resolve recreates the same keys. Two derived keys that collide fail in debug builds. A component that refers to another entity holds its key. A lookup from key to entity is available to simulation code.
2. Component types register with the walk by an explicit registration function, in a written order, never by static self-registration. Each type has a name, a class (intent, state, or derived), and functions from its owning module to copy, compare, hash, encode, and decode it. The walk visits types in registration order and entities in key order.
3. A copy of a randomized synthetic world equals the original, and has the same hash. Stepping both for 100 ticks gives equal worlds and equal hashes at every tick. Changing any value in the copy leaves the original's hash unchanged.
4. Changing any single registered value in a randomized synthetic world changes its hash, for every registered synthetic type and for the world's seed, tick, and key counter.
5. In a debug build, copying, hashing, or saving a world that holds a component of an unregistered type fails with an error naming the type, and so does a NaN in registered state. A test proves both.
6. Each tick runs the cycle in this order, and a test with recording synthetic systems shows it: step the registered systems in registration order, run the registered swap functions (shared-medium's buffers register here once it lands), apply the intent commands queued since the last cycle in submission order, then, only if a command was applied, run the registered resolvers in their declared dependency order. Resolvers whose dependencies form a cycle are rejected at registration. A new or loaded world is resolved in full before its first tick.
7. A candidate is made by copying a world, applying tentative commands, and resolving, with no stepping. Take two copies of a world. Run one through a cycle with no commands, which leaves it just after its swap, and make a candidate from it with some commands. Queue the same commands on the other and run it through a cycle. The candidate equals the second copy, and making it leaves the first copy's hash unchanged.
8. A save writes the line format below: a version header, the world's seed, tick, and key counter, then a section per registered intent or state type in registration order, one line per entity sorted by key. Numbers are written with std::to_chars shortest round-trip. No derived type appears in any save, as a test checks by type class.

    ```
    tpj-park 1
    seed 12345
    tick 3000
    next-key 57

    [box]
    12 kind=shop x=4.5 z=-2 facing=1.5707963267948966
    ```

9. Loading a save of a resolved world and resolving gives a world equal to the one saved, and saving the loaded world again gives an identical file, byte for byte, for randomized synthetic worlds, including state that is part-way through a multi-tick process. A malformed or unknown line fails the load with an error naming the line number, and never crashes.
10. A draw is a function of its key alone (world seed, entity key, purpose, tick, index), giving 64 bits, with a uniform double in [0, 1) from the top 53 bits and a pick from integer or double weights. Draws made in shuffled orders give the same values. Uniform draws pass a chi-square test over a million samples, weighted picks match their weights within a stated tolerance over a million samples, and a checked-in table of expected draws matches on every build.
11. The simulation's exp and log are ports of musl's, kept with their MIT notice. They match a checked-in table of correctly rounded reference values within 1 ULP, including special values (zero, negative, infinities, NaN, subnormals, and the overflow and underflow limits). Creating a world asserts that the rounding mode is the default.
12. A test runs nm -u on the built libtpj_sim.a and fails on any transcendental function from the C runtime's denylist. It is proven by a planted object that calls exp, which the check rejects.
13. tpj_scenarios, a headless executable in src/scenarios that links only tpj_sim, is built by every preset. It runs each registered synthetic scenario for a given number of ticks and prints its hash at every tick. It also runs any save file it is given, loading and resolving it before stepping. It prints the port's exp and log outputs over a fixed set of inputs, and the draws, uniform doubles, and weighted picks for the keys in the expected-draw table. At least one synthetic scenario's systems make draws. Its ticks-per-second report, the harness for the slice's timing question, goes to standard error, so it never enters the compared output.
14. scripts/cross-build-check.sh builds tpj_scenarios with the windows-debug and linux-debug presets, runs every registered scenario and every checked-in park file under tests/parks/ on both, and fails on the first differing line of standard output, naming the scenario or file and the tick. It prints each stage as it goes. The pre-push hook runs it when the pushed commits touch src/sim, src/core, src/scenarios, cmake/, the top-level CMakeLists.txt or CMakePresets.json, tests/parks/, or the script itself, and skips it otherwise. It says which it did either way.
15. src/sim/SPEC.md and a new src/scenarios/SPEC.md describe the walk, keys, the cycle, candidates, saves, draws, and simulation math as built. linux-debug builds without warnings and its tests pass.

## Medium

This milestone introduces no fields or flows. What its features provide to each other and to later capabilities:

- Stable keys and the key lookup (registered-walk): used by keyed-draws for draw keys, by text-saves for line order, and by every later capability for references between entities.
- Type registration with classes (registered-walk): used by tick-cycle (which types are intent), by text-saves (which types are saved), and by every later capability for its state. shared-medium's fields and ledger register as state or derived here.
- System, swap, resolver, and command registration (tick-cycle): used by every later capability. Commands are values of types their owning module registers and applies, which effortless-building's tools submit. Swap functions are where shared-medium's buffers join the cycle.
- Candidates (tick-cycle): used by legible-simulation's previews, through decision 0025.
- Save and load (text-saves): effortless-building owns the park file commands, and the encoding is this milestone's.
- Draws (keyed-draws), and exp and log (sim-math): used by believable-guests' softmax and variation.
- The scenario runner and cross-build script (cross-build-check): run the park files effortless-building's sketch-a-park checks in, which is how the slice's criterion 7 compares fed.park across builds. The app's --hash option serves scripted captures and manual checks.

The walk handles every type opaquely through its registered functions, so no feature or capability reads another's component layout (principle 6).

## Dependencies

- tpj_sim with EnTT and the fixed-tick skeleton: met.
- The Windows toolchain reachable from WSL through cmake.exe, and Windows executables runnable through interop: met.
- No other slice member. shared-medium's first-field-and-flow and effortless-building's sketch-a-park build on this milestone.

## Core feature

registered-walk is the core. It makes a world a value, with stable keys and registered types that can be copied, compared, and hashed. With the existing stepWorld, that is already enough to show two identically built worlds hashing equal every tick and a copy stepping in lockstep with its original. Every other feature consumes its keys or its registration.

## Features

1. `registered-walk`: stable entity keys from a saved counter or derived for resolved entities, with a key lookup, explicit type registration with name, class, and owner-supplied functions, and copy, equality, and hash over registered types in key order, with the debug failures for unregistered types and NaN. Depends on: none.
2. `tick-cycle`: registration of systems, swap functions, resolvers with declared dependencies, and command types, the intent command queue, the cycle in its fixed order with resolution only after a command, full resolution of new and loaded worlds, and candidates made by copy, apply, and resolve. Depends on: feature 1.
3. `keyed-draws`: the SplitMix64-chained draw over (seed, entity key, purpose, tick, index), uniform doubles from the top 53 bits, weighted picks, and the distribution and expected-value tests. Depends on: feature 1.
4. `text-saves`: the canonical line format through each type's encode and decode, intent and state only, load then resolve, byte-identical re-save, and load errors by line number. Depends on: features 1 and 2.
5. `sim-math`: the ports of musl's exp and log, the reference-value table, the rounding-mode assertion, and the nm-based symbol check with its planted violation. Depends on: none.
6. `cross-build-check`: tpj_scenarios with registered synthetic scenarios and save files, per-tick hashes, exp, log, and draw outputs, and ticks per second on standard error, the cross-build script over scenarios and tests/parks/, and its pre-push trigger by path. Depends on: features 1 through 5.

## Deepening candidates

- Release builds in the cross-build check: compare windows-release too, once players run release builds.
- A first-divergence report: when the script finds differing hashes, rerun both sides with per-type hashes to name the first diverging type and entity. Gated on: a divergence that is hard to find.
- Incremental resolution: run only the resolvers an applied command affects, in dependency order, instead of every resolver. Gated on: resolution cost showing up in the tick or in a preview's candidate. Met: candidate-previews measured a candidate of warm.park at 1.7 ms on windows-debug, paid for every new pose of a moving ghost (plans/legible-simulation/explained-food/candidate-previews/RESEARCH.md).
- Candidates off the frame's thread: copy the world on the main thread and resolve the candidate on a worker, showing the last finished one, so a moving ghost never stalls a frame. The copy is a value independent of the world, so the worker shares nothing with the tick. Gated on: incremental resolution leaving a candidate too slow for a frame.

## Open questions

- Copy and hash cost at slice scale, with several hundred guests and a candidate rebuilt every tick while a preview shows. Resolved by measurement with tpj_scenarios once believable-guests lands, alongside the slice's timing question.

## Research notes

- musl's exp and log port cleanly: their helpers become std::bit_cast or plain expressions, and on these builds they take the paths without intrinsics and without FMA.
- The symbol check is a denylist over nm -u, because GCC legitimately calls exact functions such as sqrt and floor.
- SplitMix64's finalizer, chained over 64-bit words, serves both the state hash and keyed draws.
- References between entities hold stable keys, never entt::entity values, and registration is explicit because static initialization order depends on the link.
- Pre-push reads the pushed ref ranges from stdin, and the script strips CRLF from Windows output.

Depth is in RESEARCH.md.
