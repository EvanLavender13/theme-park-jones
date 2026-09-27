# Feature: Registered Walk

## Summary

registered-walk makes the world a value. Every entity gets a stable key. Component types are registered with a schema by explicit calls, each with a name, a data kind (intent, state, or derived), and a visitFields function from its owning module. One walk over the registered types, in registration order and key order, gives copy, equality, and a 64-bit state hash. In debug builds the walk refuses worlds it cannot cover fully: components of unregistered types, entities without keys, and NaN in registered fields. This feature does not include saving and loading (text-saves), the cycle (tick-cycle), or draws (keyed-draws). It provides the keys and registration that all three build on.

## Acceptance criteria

1. Counter keys are unique and never reused. createEntity returns keys ascending from 1, nextKey() is the key the next call will return, and destroying an entity never lets its key come back.
2. Derived keys are a pure function of (owner, purpose, index), lie outside the counter's range (isDerivedKey tells them apart, and NULL_KEY is neither), and createDerivedEntity is idempotent: calling it again returns the same entity with its components, and never moves the counter.
3. findEntity and keyOf are inverses for live entities, and give entt::null and NULL_KEY for anything not live. destroyEntity removes the entity and its components, and is a no-op returning false for a key that is not live. keys() is ascending.
4. A schema accepts component names of lowercase ASCII letters, digits, and hyphens, and refuses a malformed name, a repeated name, or a type registered twice, with std::invalid_argument and no change to the schema. components() keeps registration order.
5. A copy equals its original and hashes the same, and from then on the two are independent: a change to either leaves the other unchanged.
6. Equality and the hash agree, and see every registered value: worlds built by the same calls are equal and hash equal, including on separately built identical schemas, and a change to any one thing the walk covers (Tick, Seed, the key counter, the live keys, a component's presence, or any field, with doubles compared by their bits) makes them unequal and changes the hash. Worlds on different schemas are never equal.
7. The walk ignores EnTT's storage order: the same keys and components added in a different order give an equal world with the same hash.
8. validateWorld throws WorldInvariantError for each world the walk cannot cover: a component of an unregistered type (the message names the type; an empty storage is not an error), an entity not created by the world, a live key whose entity was destroyed on the registry, and a NaN in a registered double (the message names the component, field, and key). It accepts every other world.
9. In debug builds (WORLD_CHECKS), copyWorld, worldsEqual (on either side), and hashWorld each run validateWorld first, so each refuses an invalid world.
10. Each call to stepWorld advances Tick by exactly one, and two worlds built by the same calls stay equal, with the same hash, after every step.

Which field types visitFields may list is enforced at compile time by a static_assert in emitField, not by a test.

## Medium

This feature introduces no fields or flows. It provides:

- Stable keys, the key lookup, and derived keys: used by keyed-draws, text-saves, and tick-cycle's resolvers, and by every later capability for references between entities.
- The schema with data kinds: used by tick-cycle and text-saves, and by every capability that registers its component types. shared-medium's buffers register here once they exist.
- Copy, equality, and the state hash: used by tick-cycle's candidates, by text-saves' round-trip checks, and by cross-build-check.

## Principle checks

- Principle 10: two worlds built by the same calls and stepped the same number of ticks hash equal every tick, and the walk ignores EnTT's storage order. The hash is built only from integers, bit patterns, and names, never from addresses or entt::entity values.
- Principle 6: the tests declare their synthetic component types in the test file and register them through the public schema. The walk reaches them only through the registered functions, and no walk header names a component type.
- Principle 1: every registered type carries a data kind. Derived types are covered by copy, equality, and hash like the others, and text-saves later leaves them out of saves.
- Principle 2: an empty world, and entities without components, are legitimate and walk normally. Invalid worlds fail loudly in debug instead of hashing to a value that hides missing state.

## Spec changes

src/sim/SPEC.md: replace "Currently a skeleton that holds only a tick counter." with "It holds a world of keyed entities whose registered state can be copied, compared, and hashed.", and add after the Contract's second paragraph:

"Every entity has a stable EntityKey. Keys from createEntity come from a counter held by the world, starting at 1, and are never reused. Resolvers create entities with createDerivedEntity, whose key is a pure function of an owner key, a purpose, and an index, with the top bit set so that it never meets the counter's range. A component refers to another entity by its key, never by entt::entity, because EnTT recycles its identifiers.

Component types are registered with a WorldSchema by explicit calls in a written order, each with a name, a DataKind (intent, state, or derived), and a visitFields function its owning module supplies. visitFields lists the type's fields once, and every walk over the world runs through it. The schema is built once and shared as a const value.

The walk visits registered types in registration order and entities in key order, never in EnTT storage order. copyWorld, worldsEqual, and hashWorld are defined by it: two worlds are equal exactly when the walk emits the same words for both, and the hash folds those words with SplitMix64's finalizer. Doubles are compared and hashed by their bits. In debug builds, the walk first checks that every stored component's type is registered, that every entity has a key, and that no registered double is NaN, and it throws WorldInvariantError if not."

## Files affected

- Create: src/sim/mix.h
- Create: src/sim/entity_key.h
- Create: src/sim/schema.h
- Create: src/sim/schema.cpp
- Create: src/sim/walk.cpp
- Modify: src/sim/world.h
- Modify: src/sim/world.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Test pass: files under tests/, written by the test-writer agent.

## Dependencies

- EnTT v4.0.0, already linked to tpj_sim.
- No sibling feature. keyed-draws, tick-cycle, and text-saves build on this one.

## Out of scope

- Saving and loading, the encode and decode visitors, and creating an entity under a given key: text-saves.
- Systems, resolvers, commands, and candidates: tick-cycle.
- Draws: keyed-draws. It reuses mix64 and Hasher from this feature.
- A report of where two worlds first differ: the capability's divergence-diagnosis candidate.
- Faster equality than comparing two word streams: measured later, under the milestone's copy-and-hash-cost question.

## Open questions

- The check that two different derived origins never share a key runs in debug builds, but no test can reach it, because finding a real collision of the 63-bit derived hash is infeasible. It stays a defensive check. Resolved if text-saves' creation under a given key makes a mismatched origin reachable in a test.
