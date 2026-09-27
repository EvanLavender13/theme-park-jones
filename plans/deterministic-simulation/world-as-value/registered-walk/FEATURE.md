# Feature: Registered Walk

## Summary

registered-walk makes the world a value. Every entity gets a stable key. Component types are registered with a schema by explicit calls, each with a name, a data kind (intent, state, or derived), and a visitFields function from its owning module. One walk over the registered types, in registration order and key order, gives copy, equality, and a 64-bit state hash. In debug builds the walk refuses worlds it cannot cover fully: components of unregistered types, entities without keys, and NaN in registered fields. This feature does not include saving and loading (text-saves), the cycle (tick-cycle), or draws (keyed-draws). It provides the keys and registration that all three build on.

## Acceptance criteria

Keys

1. A world made with `World()` has Tick 0, Seed 0, `nextKey()` 1, and no entities. A world made with `World(schema, 42)` has Seed 42.
2. `createEntity()` returns keys 1, 2, 3, and so on, in call order. After key 2 is destroyed, the next call returns 4, and `nextKey()` is 5. No sequence of creates and destroys ever returns a key twice.
3. `findEntity(key)` returns the entity for a live key and `entt::null` for a key never created or already destroyed. `keyOf(entity)` returns the key of a live entity, and `NULL_KEY` for `entt::null` or an entity the world did not create. `destroyEntity(key)` returns true and removes the entity with all its components, or returns false and changes nothing when the key is not live.
4. `keys()` lists live keys in ascending order, whatever order they were created and destroyed in. Derived keys, which have the top bit set, sort after every counter key.
5. `deriveKey(owner, purpose, index)` is constexpr and depends only on its arguments. `isDerivedKey` is true for every derived key and false for every counter key and for `NULL_KEY`. Over all combinations of owners 1 to 100, purposes `hashName("a")` and `hashName("b")`, and indexes 0 to 499 (100,000 keys), no two derived keys are equal.
6. `createDerivedEntity(owner, purpose, index)` returns `deriveKey(owner, purpose, index)`. It creates the entity on the first call, and on later calls with the same arguments it returns the same key without creating another entity, and keeps its components. It never changes `nextKey()`. After the entity is destroyed, the same call creates it again under the same key.
7. `hashName` is constexpr, gives the same value for equal strings, and gives distinct values for the 1,000 strings "name0" to "name999".

Schema

8. `addComponent<T>(name, kind)` accepts names made of lowercase ASCII letters, digits, and hyphens, such as "shop" and "box-2". It throws `std::invalid_argument` for "", "Shop", "a b", "a_b", and a name already registered, and for a type already registered under another name. After a throw, the schema is unchanged.
9. `components()` lists the registered types in registration order, with their names and kinds. `findComponent(entt::type_id<T>().hash())` returns the entry for a registered T, and nullptr otherwise.
10. Field types a visitFields may list: bool, every integer type, double, EntityKey, enums, std::vector of any of these except bool, and structs with their own visitFields, nested to any depth. A component type with no members (a tag) registers without a visitFields. A component with a field of any other type, such as float, fails to compile.

Copy, equality, and hash

11. `copyWorld(world)` gives a world that `worldsEqual` reports equal to the original and whose `hashWorld` is equal. Its Tick, Seed, `nextKey()`, `keys()`, and components on every key are the same. An EntityKey field copied into the copy finds, through the copy's `findEntity`, the entity with that key.
12. After a copy, each of these changes to the copy leaves the original's hash, `keys()`, and components unchanged: changing a field, adding or removing a component, creating or destroying an entity, and changing Tick or Seed. The same holds with the roles swapped.
13. `worldsEqual(left, right)` is false when the worlds differ in any one of: Tick, Seed, `nextKey()`, the set of live keys, which keys hold a given component, or any field value. It compares doubles by their bits, so 0.0 and -0.0 differ. Two worlds built by the same sequence of calls are equal, even when each uses its own separately built schema, provided the schemas register the same names, kinds, and types in the same order. Worlds whose schemas differ are never equal.
14. Changing any single value changes `hashWorld`: flipping a bool, adding 1 to an integer, changing a double to the next representable value, changing 0.0 to -0.0, changing an EntityKey, changing an enum, changing one element or the length of a vector, changing a nested struct's field, adding or removing a tag component, and changing Tick, Seed, or `nextKey()` (by creating and destroying an entity). A randomized test does this over many worlds, types, entities, and fields.
15. The hash and equality ignore EnTT's storage order. Two worlds with the same keys and components, where the components were added in different orders or removed and re-added, are equal and hash equal.
16. `stepWorld` still advances Tick by one per call. Two worlds built by the same calls and stepped 100 times have equal hashes after every step.

Debug checks

17. `WORLD_CHECKS` is true in builds without NDEBUG. When it is true, `copyWorld`, `worldsEqual`, and `hashWorld` call `validateWorld` on each world they are given. `validateWorld` can also be called directly in any build, and throws `WorldInvariantError` in these cases:
    - A component of a type not registered with the world's schema is attached to any entity. The message contains the type's name. An unregistered type whose storage exists but is empty, for example after a view of it, is not an error.
    - An entity exists in the registry that the world did not create, for example through `Registry.create()`.
    - A live key's entity was destroyed directly through `Registry.destroy()`.
    - A registered double field holds NaN, including inside a vector or a nested struct. The message contains the component's name, the field's name, and the entity's key in decimal.
18. A world with entities that have no components, and an empty world, are valid, and copy, compare, and hash like any other.

## Medium

This feature introduces no fields or flows. It provides:

- Stable keys, the key lookup, and derived keys: used by keyed-draws, text-saves, and tick-cycle's resolvers, and by every later capability for references between entities.
- The schema with data kinds: used by tick-cycle and text-saves, and by every capability that registers its component types. shared-medium's buffers register here once they exist.
- Copy, equality, and the state hash: used by tick-cycle's candidates, by text-saves' round-trip checks, and by cross-build-check.

## Principle checks

- Principle 10: two worlds built by the same calls and stepped the same number of ticks hash equal every tick (criterion 16), and the walk ignores EnTT's storage order (criterion 15). The hash is built only from integers, bit patterns, and names, never from addresses or entt::entity values.
- Principle 6: the tests declare their synthetic component types in the test file and register them through the public schema. The walk reaches them only through the registered functions, and no walk header names a component type.
- Principle 1: every registered type carries a data kind. Derived types are covered by copy, equality, and hash like the others, and text-saves later leaves them out of saves.
- Principle 2: an empty world, and entities without components, are legitimate and walk normally (criterion 18). Invalid worlds fail loudly in debug instead of hashing to a value that hides missing state (criterion 17).

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
