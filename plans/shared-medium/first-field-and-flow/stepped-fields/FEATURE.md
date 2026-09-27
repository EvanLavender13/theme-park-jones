# Feature: Stepped Fields

## Summary

stepped-fields adds the second layer of every field. Systems publish a source's entries while stepping with publishStepped. The entries become readable at the swap that ends the tick and stay readable for the one tick after it, and they are state, so they are saved. A source with readable stepped entries is sampled by them, and otherwise by its resolved entries. A resolution after a command clears the stepped entries of each source whose resolved entries it changed, so the next tick samples every change resolution made. The clearing needs to run after every resolver has published, so the schema gains finishers, which every resolution runs after its resolvers, and World gains isStepping, so stepped publication can refuse callers that are not systems.

## Acceptance criteria

1. World::isStepping is true while a system runs and false otherwise. Every resolution, a candidate's and a loaded world's included, runs each of the schema's finishers once, after all of its resolvers, in registration order. addField also registers the field's state component type, named its name followed by -stepped, and throws std::invalid_argument when that name is malformed or already registered.
2. Entries a system publishes into a field while stepping tick t are not sampled during that tick. They are sampled from the swap that ends it until the swap that ends the next tick, which replaces them with the entries published during that tick, if any. publishStepped throws std::logic_error when the world is not stepping, or holds no stepped entries for the field. It throws std::invalid_argument for the null source key, and for a source that has already published into the field in the same tick, even with no entries. A publishStepped that throws leaves the field's entries unchanged.
3. A source with readable stepped entries, even an empty list, is sampled by them, and otherwise by its resolved entries. Sources are sampled in ascending key order across both layers, and a stepped entry whose place does not resolve on the network is not sampled.
4. A resolution after a command clears the readable stepped entries of exactly the sources whose resolved entries it changed, including sources whose resolved entries it removed and sources it gave their first. Every other source keeps its stepped entries. Entries count as changed when the walk emits different words for them, so doubles compare by their bits.
5. Stepped entries are state. A save holds them, and loading the save of a resolved world with both layers in flight and resolving gives a world equal to the one saved. Changing any stepped entry changes the world's hash, and a copy stepped forward equals the original stepped forward.
6. Systems that publish into fields give the same world and the same hash after every tick whatever order they are registered in.
7. A candidate made with makeCandidate samples every field, both layers, exactly as the world that commits the same commands does.

## Medium

This feature adds the stepped layer and the layer rule, and no park field. Systems publish into it, and anyone holding a world, a network, and a place samples it:

- plausible-operations will republish the food offer from its state each tick, over the resolved offer it publishes from intent.
- believable-guests will publish hungry footfall.
- legible-simulation samples both layers in committed and candidate worlds.
- flow-ledger will use isStepping to confine ledger operations to stepping, and finishers are available to any later module that needs every resolver's output.

## Principle checks

- Principle 1: criteria 4 and 5. Stepped entries are state and saved, resolved entries are derived and never saved, and a load resolves back to the world saved because the first resolution clears nothing.
- Principle 2: criteria 3 and 4. A source whose path is removed has no stale stepped entries in the next tick, and an empty stepped list is a legitimate sample.
- Principle 3: a field is sampled without being consumed. sampleField takes the world by const reference.
- Principle 8: criteria 4 and 7. A preview samples as the commit does, and the tick after a command samples every change resolution made.
- Principle 10: criteria 2 and 6. Stepped entries published during a tick are read only after it, and sources are held in key order whatever order systems publish in.

## Spec changes

src/sim/SPEC.md:

In the paragraph beginning "The world advances only through stepWorld", after "stepWorld(world) is the same cycle with no commands.", add: "isStepping is true only while the systems step, so functions meant only for systems can refuse other callers."

In the paragraph beginning "Component types are registered with a WorldSchema", replace "The schema also lists the cycle's systems, swap functions, and resolvers, each in registration order, and its command types." with "The schema also lists the cycle's systems, swap functions, resolvers, and finishers, each in registration order, and its command types."

In the paragraph beginning "Resolvers derive the world's derived data", replace "A resolution calls every resolver once, in registration order." with "A resolution calls every resolver once, in registration order, and then every finisher once, in registration order. A finisher completes work that needs every resolver's output, such as comparing it with the previous resolution's. Like a resolver, it never draws from the key counter and derives nothing from state, but it may clear state that its owning module's spec says a resolution invalidates. It never changes state in a world's first resolution, which has no previous resolution to compare with, so loading the save of a resolved world and resolving still gives the world saved."

src/sim/medium/SPEC.md, in the Fields section:

Replace "addField registers a field with a schema: the derived component type named its name followed by -resolved, which holds its resolved entries, and the resolver named its name followed by -field, which empties them." with "addField registers a field with a schema: the derived component type named its name followed by -resolved, which holds its resolved entries; the state component type named its name followed by -stepped, which holds its stepped entries; the resolver named its name followed by -field, which empties the resolved entries; a swap function; and a finisher."

Replace "It throws std::invalid_argument when either name is malformed or already registered." with "It throws std::invalid_argument when any of its names is malformed or already registered."

Replace "The entries sit on an entity keyed fieldKey(name)" with "Both layers sit on an entity keyed fieldKey(name)".

After the paragraph beginning "Resolved entries are derived data (principle 1).", add:

"Stepped entries are state, and saves hold them (principle 1). A system calls publishStepped with a source's entries while stepping. They are not readable until the swap that ends the tick, and are then readable until the next swap, which replaces the field's readable stepped entries with those published during the tick it ends. So a source that stops publishing loses its stepped entries one swap later. Sources are held in ascending key order, whatever order systems publish in, and each source's entries in the order it gave them. publishStepped throws std::logic_error when the world is not stepping, or holds no stepped entries for the field, as before the field's resolver first runs. It throws std::invalid_argument for the null source key, and for a source that has already published into the field in this tick, which an empty list of entries counts as. A publishStepped that throws changes nothing.

A source with readable stepped entries, even an empty list, is sampled by them, and otherwise by its resolved entries. This is the layer rule. A resolution after a command in a resolved world has the previous resolution's entries to compare with, and the field's finisher clears the readable stepped entries of each source whose resolved entries it changed, including by removing them or giving the source its first. So the tick after a command samples every change the resolution made. A source's resolved entries are its list, or an empty list when it did not publish, and two lists differ when the walk emits different words for them, so doubles compare by their bits. The first resolution of a new or loaded world has nothing to compare with and clears nothing, so loading the save of a resolved world and resolving gives the world saved."

In the paragraph beginning "sampleField gives a field's entries at a place on a network", after its first sentence, add: "Each source's entries are chosen by the layer rule, and the rules below apply to those."

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/schema.h
- Modify: src/sim/schema.cpp
- Modify: src/sim/world.h
- Modify: src/sim/cycle.cpp
- Modify: src/sim/medium/field.h
- Test pass: files under tests/, written by the test-writer agent: tests/sim/medium/stepped_field_test.cpp, additions to tests/sim/support/synthetic_fields.h for publishing systems, a finisher and isStepping test in tests/sim/cycle_test.cpp, and the addition to tests/sim/CMakeLists.txt.

## Dependencies

- resolved-fields: field definitions, addField, resolved entries, and sampling. Merged.
- deterministic-simulation's world-as-value: the cycle, swaps, saves, and candidates. Merged.

## Out of scope

- The flow ledger and its use of isStepping: flow-ledger.
- The synthetic scenario that puts fields in the cross-build check: flow-ledger, which adds it with the ledger.
- Resolved entries computed from state: a milestone deepening candidate.
- Clearing only the stepped entries a change affects, rather than a changed source's all: nothing in the slice needs it.

## Open questions

None.
