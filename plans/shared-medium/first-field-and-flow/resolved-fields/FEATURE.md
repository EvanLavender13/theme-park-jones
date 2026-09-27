# Feature: Resolved Fields

## Summary

resolved-fields adds the medium's fields, with the layer that resolvers publish into. A field is a type its owning module defines, naming its entry type, its name, its kind (entry or scalar), and optionally its own sampling rule for places inside an edge. addField registers it: a derived component that holds its entries on an entity with a derived key, and a resolver that empties them at the start of each resolution. Resolvers publish each source's entries at places, and the entries are readable at once. sampleField gives the entries at a place on a network with their sources, in ascending source key order, by the default rule or the owner's. fieldValue gives a scalar field's value as the ordered sum of its sampled entries. World gains isResolving, so publication can refuse callers that are not resolvers. The stepped layer and the layer rule are stepped-fields'.

## Acceptance criteria

1. addField registers the field's derived component type, named its name followed by -resolved, and its resolver, named its name followed by -field, and throws std::invalid_argument when either name is malformed or already registered. After a resolution, the field's entries sit on the entity keyed fieldKey of its name.
2. World::isResolving is true while a resolver runs and false otherwise. Entries a resolver publishes are sampled at once, by resolvers that run after it and after the resolution. publishResolved throws std::logic_error when the world is not resolving, and when it holds no resolved entries for the field, as before the field's resolver has first run. It throws std::invalid_argument for the null source key, and for a source that has already published into the field in the same resolution, even with no entries. A publishResolved that throws leaves the field's entries unchanged.
3. Every resolution replaces every source's resolved entries in full: a source that does not publish in a resolution has no entries after it. Resolved entries never appear in a save, and loading the save of a resolved world holding fields and resolving gives a world equal to the one saved.
4. sampleField gives each source's entries with the source's key, sources in ascending key order, and each source's entries in the order it published them, whatever order the sources were published in. Publishing the same sources in shuffled orders gives the same world and the same hash.
5. At a place that resolves to a node, a source's sampled entries are those whose places resolve to the same node, whatever carrier they name, for fields with sampleEdge as for those without. At a place strictly inside an edge, a field without sampleEdge gives the entries whose places equal it. A place that does not resolve on the network gives no entries, and so does an entry whose place does not resolve.
6. At a place strictly inside an edge, a field with sampleEdge calls it once for each source with an entry at either of the edge's nodes or inside the edge, and nowhere else. The EdgeSample holds the edge, the place's offsets as resolve gives them, the source's entries at the edge's From node and at its To node, and its entries inside that edge with their offsets as resolve gives them, each list in the source's order. The entries it returns are the source's sampled entries.
7. fieldValue of a scalar field is 0.0 with each entry sampleField gives added in turn, in its order, bit for bit. A place with no entries has the value 0.0.
8. A candidate made with makeCandidate samples every field exactly as the world that commits the same commands and resolves does.

## Medium

This feature introduces the field mechanism and no park field. Its fields are published by resolvers and sampled by anyone holding a world, a network, and a place:

- navigable-networks will publish route distance, with a sampleEdge that applies the phantom-node rule.
- plausible-operations will publish the resolved layer of the food offer from intent and route distance.
- legible-simulation will sample fields in committed and candidate worlds.
- stepped-fields adds the stepped layer and the rule that prefers it.

## Principle checks

- Principle 1: criterion 3. Resolved entries are derived, never saved, and recreated by resolving a loaded world.
- Principle 2: criteria 3 and 5. A place with no sources, or one that does not resolve, gives no entries and a scalar value of 0.0, and neither is an error.
- Principle 3: a field is sampled without being consumed. sampleField and fieldValue take the world by const reference, which the header shows.
- Principle 4: criterion 6. An owner's rule reads a place between nodes through its offsets along the edge to each end.
- Principle 6: criteria 4 and 5. Consumers read a field only through sampleField and fieldValue, never through a source's components.
- Principle 8: criteria 4, 7, and 8. Each entry keeps its source, a scalar value is exactly the ordered sum of the entries that explain it, and a preview samples as the commit does.
- Principle 10: criterion 4. Source order is key order, whatever order resolvers publish in.

A scalar field's entries being doubles is a compile-time check in addField, which no runtime test covers, since tests/ has no compile-failure harness.

## Spec changes

src/sim/medium/SPEC.md: replace the introduction's "the networks every field is sampled on" with "the networks, and the fields sampled on them". At the end of the file, add a section:

"## Fields

A field is a set of entries published at places, each attributed to the source that published it (principles 3 and 8). Its owning module defines it as a type with three public static members: Entry, the entries' type, which must be default constructible, copyable, and a registrable field type; Name, a std::string_view of lowercase letters, digits, and hyphens; and Kind, FieldKind::Entry or FieldKind::Scalar. A scalar field's Entry is double, which a compile-time check enforces. The type may also define a static function sampleEdge, its owner's rule for places inside an edge.

addField registers a field with a schema: the derived component type named its name followed by -resolved, which holds its resolved entries, and the resolver named its name followed by -field, which empties them. It throws std::invalid_argument when either name is malformed or already registered. Resolvers run in registration order, so a module registers a field before the resolvers that publish into it, and those may name the field's resolver as a dependency. The entries sit on an entity keyed fieldKey(name), which is deriveKey(NULL_KEY, hashName(\"field\"), hashName(name)), and which the field's resolver creates.

Resolved entries are derived data (principle 1). A resolver calls publishResolved with a source's entries, each a place and a value. They are readable at once, by later resolvers and after the resolution, and every resolution replaces every source's entries in full, so a source that does not publish in a resolution has none afterwards. Sources are held in ascending key order, whatever order they publish in, and each source's entries in the order it gave them. publishResolved throws std::logic_error when the world is not resolving, or when it holds no resolved entries for the field, as before the field's resolver first runs. It throws std::invalid_argument for the null source key, and for a source that has already published into the field in this resolution, which an empty list of entries counts as. A publishResolved that throws changes nothing. Entries are not otherwise checked, and the walk's checks apply to them as to any registered data.

sampleField gives a field's entries at a place on a network, each with its source, sources in ascending key order. A place that does not resolve on the network, or a world holding no entries for the field, gives none, and that is a legitimate sample (principle 2). An entry whose place does not resolve on the network is never sampled. At a place that resolves to a node, a source's entries are those whose places resolve to the same node, whatever carrier they name, in the source's order, whether or not the field has sampleEdge. At a place strictly inside an edge, a field without sampleEdge gives each source's entries whose places equal it, the same carrier at the same distance. A field with sampleEdge calls it once for each source with an entry at either of the edge's nodes or strictly inside the edge, in ascending source order, and the entries it returns are that source's at the place. It receives an EdgeSample: the edge; the place's FromOffset and ToOffset as resolve gives them; the source's entries whose places resolve to the edge's From node, and those that resolve to its To node, which are the same list when both ends are one node; and its entries whose places resolve inside the edge, each with its FromOffset and ToOffset as resolve gives them. Each list is in the source's order. This is how route distance reads a place between nodes (principle 4).

fieldValue gives a scalar field's value at a place: 0.0, with each entry sampleField gives added in turn, in its order. A place with no entries has the value 0.0."

src/sim/SPEC.md: after the sentence "Resolvers never draw from the key counter: in debug builds, createEntity during resolution throws WorldInvariantError.", add:

"isResolving is true only while the resolvers run, so functions meant only for resolvers can refuse other callers."

## Files affected

- Create: src/sim/medium/field.h
- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/SPEC.md
- Modify: src/sim/world.h
- Test pass: files under tests/, written by the test-writer agent: tests/sim/medium/field_test.cpp, synthetic fields and a synthetic network producer in tests/sim/support/, and the addition to tests/sim/CMakeLists.txt, whose tests join tpj_sim_tests.

## Dependencies

- networks-and-places: Network, Place, and resolve. Merged.
- deterministic-simulation's world-as-value: resolvers in registration order, derived keys, derived components, saves, and makeCandidate. Merged.

## Out of scope

- Publication while stepping, the swap, stepped entries as state, and the layer rule that prefers them: stepped-fields.
- Any park field: route distance, food offer, and hungry footfall belong to their capabilities.
- An index from positions to entries: sampling scans a field's entries, which suits the slice's few sources. Profiling can add one later.
- Listing a world's fields for tooling: the capability's generic field enumeration candidate.

## Open questions

None.
