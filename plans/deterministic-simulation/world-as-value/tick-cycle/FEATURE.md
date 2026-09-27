# Feature: Tick Cycle

## Summary

tick-cycle gives every tick one fixed cycle. The schema that registers component types also registers systems, swap functions, resolvers, and command types, each in a written order. Commands wait in a CommandQueue outside the world. stepWorld runs one cycle: the systems step, the tick advances, the swaps run, the queued commands are applied in submission order, and the resolvers run if resolution is pending. Resolution is pending for a new or loaded world and after any applied command. makeCandidate copies a world, applies commands, and resolves it without stepping, which is the candidate a preview samples. This feature does not include saves (text-saves), draws (keyed-draws), or the scenario runner (cross-build-check).

## Acceptance criteria

1. Each call to stepWorld runs one cycle in this order: a resolution if one is pending, the registered systems in registration order, then Tick advances by exactly one, then the registered swap functions in registration order, then the queued commands in submission order, each through its type's applyCommand, then a resolution if one is pending again. The call returns with the queue empty. stepWorld(world) runs the same cycle with no commands.
2. Resolution is pending from a world's construction until it is first resolved, and from each applied command until the next resolution. Each resolution calls every registered resolver once, in registration order, and clears it; that is what resolveWorld does and what a cycle does when resolution is pending. So a new or loaded world is resolved in full before its first tick, and a cycle that applies no command to a resolved world calls no resolver. isResolvePending reports whether resolution is pending.
3. The walk covers whether resolution is pending: a copy keeps it, and two worlds that differ only in it are unequal and hash differently.
4. A resolver may depend only on resolvers already registered, so dependencies never form a cycle and registration order satisfies them. Registration refuses, with std::invalid_argument and no change to the schema, a resolver whose name is malformed (the component name rule) or repeated, or with a dependency that is not an already registered resolver, and a command type registered twice.
5. When the queue holds a command whose type is not registered with the world's schema, stepWorld and makeCandidate throw std::invalid_argument naming the type, and change neither the world nor the queue.
6. makeCandidate copies a world, applies the commands in submission order, and resolves the copy if resolution is pending, with no stepping. Take two copies of a resolved world. Run the first through a cycle with no commands, then make a candidate from it with some commands. Queue the same commands on the second and run it through a cycle. The candidate equals the second copy and hashes the same, and making it leaves the first copy's hash unchanged.
7. In debug builds (WORLD_CHECKS), createEntity called during resolution throws WorldInvariantError, so resolvers never draw from the key counter. createDerivedEntity works during resolution as it does anywhere else.

## Medium

This feature introduces no fields or flows. It provides:

- System and resolver registration: used by every later capability to step its state and derive its derived data.
- Swap function registration: where shared-medium's double buffers join the cycle once first-field-and-flow lands.
- Command types and the CommandQueue: effortless-building's tools submit commands, and the owning module applies them.
- Candidates: legible-simulation's previews sample a candidate world's fields through the normal interface (decision 0025).
- Pending resolution: text-saves' load leaves a world pending, and resolveWorld resolves it.

## Principle checks

- Principle 10: two worlds built by the same calls, and cycled with the same commands at the same ticks, stay equal with the same hash after every cycle. Systems, swaps, and resolvers are plain function pointers that take only the world, so they hold no state outside it. Commands wait outside the world, so pending input never enters its value.
- Principle 8: criterion 6. A candidate made from a world that has just finished a cycle equals the world that queuing the same commands for that cycle would have given, and making it leaves the source unchanged.
- Principle 1: resolution runs after every applied command and before a new or loaded world's first tick, so derived data always matches intent when a tick samples it (criterion 2). Resolvers read only intent, so derived data never lags state. Resolvers take only derived keys (criterion 7), so resolving again recreates the same keys.
- Principle 6: the cycle applies commands only through each type's applyCommand, supplied by its owning module, and never names a command or component type.
- Principle 2: a resolved world with nothing registered cycles normally, advancing only its tick.

## Spec changes

src/sim/SPEC.md: in the Contract, replace "Each call to stepWorld increments World::Tick by one." with:

"Each call runs one cycle: a pending resolution runs first, the registered systems step in registration order, World::Tick increments by one, the registered swap functions run in registration order, the commands queued since the last cycle are applied in submission order, and then, if resolution is pending again, the registered resolvers run. stepWorld(world) is the same cycle with no commands."

At the end of the paragraph on WorldSchema, add:

"The schema also lists the cycle's systems, swap functions, and resolvers, each in registration order, and its command types."

After the paragraph on the walk, add:

"Commands are values of types their owning module registers with addCommand and applies with an applyCommand function found by argument-dependent lookup. They wait in a CommandQueue outside the world, never in its state, so copies, hashes, and saves never hold them. stepWorld empties the queue. A queue holding a command of an unregistered type is refused before anything changes.

Resolvers derive the world's derived data from its intent alone, never from state, which systems change every tick without a resolution following. Each declares the resolvers it depends on, and each dependency must already be registered, so registration order is a dependency order and a cycle cannot form. A resolution calls every resolver once, in registration order. It is pending from a world's construction until its first resolution, and again from each applied command until the next. A new or loaded world is therefore resolved in full before its first tick, and a cycle that applies no command runs no resolver. The walk covers whether resolution is pending. Resolvers never draw from the key counter: in debug builds, createEntity during resolution throws WorldInvariantError.

makeCandidate copies a world, applies commands to the copy, and resolves it, with no stepping. A candidate made from a world that has just finished a cycle therefore equals the world that queuing the same commands for that cycle would have given, so a preview is exact (principle 8, decision 0025). The original is unchanged."

## Files affected

- Create: src/sim/command_queue.h
- Create: src/sim/cycle.cpp
- Modify: src/sim/schema.h
- Modify: src/sim/schema.cpp
- Modify: src/sim/world.h
- Modify: src/sim/world.cpp
- Modify: src/sim/walk.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Test pass: files under tests/, written by the test-writer agent.

## Dependencies

- registered-walk: keys, the schema, copy, equality, and hash. Merged.
- EnTT v4.0.0's type_id, for command type ids and names.

## Out of scope

- Saving and loading: text-saves. It constructs a world, which leaves resolution pending, so its round trip is stated for resolved worlds.
- Draws: keyed-draws.
- Recording commands for replays, and command visitFields: the capability's replays candidate.
- Running only the resolvers a command affects: the milestone's incremental-resolution candidate.
- Names and profiling zones for individual systems.

## Open questions

None.
