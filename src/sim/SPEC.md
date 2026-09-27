# sim

The simulation: the park's state and the rules that change it. It holds a world of keyed entities whose registered state can be copied, compared, and hashed.

## Contract

The target tpj_sim links nothing but tpj_core and EnTT. It must never depend on rendering, windowing, or input (principle 10), and the test suite links it alone, so a violation fails the build. Entities will live in an EnTT registry once the first entity kind exists; each kind's internal components are private to its module, and only shared-medium components are public (decision 0016).

The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS (1/30 s) at a time. Frame rate never changes what a tick does. Each call to stepWorld increments World::Tick by one.

Every entity has a stable EntityKey. Keys from createEntity come from a counter held by the world, starting at 1, and are never reused. Resolvers create entities with createDerivedEntity, whose key is a pure function of an owner key, a purpose, and an index, with the top bit set so that it never meets the counter's range. A component refers to another entity by its key, never by entt::entity, because EnTT recycles its identifiers.

Component types are registered with a WorldSchema by explicit calls in a written order, each with a name, a DataKind (intent, state, or derived), and a visitFields function its owning module supplies. visitFields lists the type's fields once, and every walk over the world runs through it. The schema is built once and shared as a const value.

The walk visits registered types in registration order and entities in key order, never in EnTT storage order. copyWorld, worldsEqual, and hashWorld are defined by it: two worlds are equal exactly when the walk emits the same words for both, and the hash folds those words with SplitMix64's finalizer. Doubles are compared and hashed by their bits. In debug builds, the walk first checks that every stored component's type is registered, that every entity has a key, and that no registered double is NaN, and it throws WorldInvariantError if not.

Stepping is deterministic: the same world state stepped the same number of times gives the same result, bit for bit, on every supported build (decision 0022). Simulation code therefore never calls the C runtime's transcendental math functions, and tpj_sim builds with -ffp-contract=off.
