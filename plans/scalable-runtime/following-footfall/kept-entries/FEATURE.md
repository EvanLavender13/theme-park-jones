# Feature: Kept Entries

## Summary

The shared medium gains kept fields, whose entries stay readable until something changes them, so a metric that accumulates where movers go costs a tick only what the movers touched (decision 0031). A scalar field becomes a kept field by defining readKept, its owner's rule for reading a value some ticks after it last changed, and registers with addKeptField instead of addField, so no other field's registrations or saves change. A system sets a source's entry at one place with keepEntry; the change is held until the swap that ends the tick, which applies the tick's changes in the order made and stamps each changed entry with the world's tick. Reads, through sampleField, fieldValue, and keptValue, give each entry's value as readKept gives it at the reading world's tick. A kept field may also define sampleNode, its owner's rule at a node, which is given the entries inside every edge meeting the node, found through a new network query, edgeEnds. The owner carries its entries across a resolution in its own finisher with replaceKeptEntries, which a new world query, isFinishing, limits to finishers. No field uses kept entries yet, so the simulation is unchanged; following-footfall's second feature moves hungry footfall onto them.

## Acceptance criteria

What each function refuses is a rule of src/sim/medium/SPEC.md, not a criterion; the criterion is that a refusal changes nothing.

1. Resolving never changes the kept entries a world holds. (test)
2. A change made with keepEntry is read by nothing before the swap that ends its tick, and from that swap on is held unchanged until the next change at its place. (test)
3. A source holds one entry at each place it has changed, with the last value given there and the tick of the swap that applied it. (test)
4. A kept entry reads as readKept of its held value and the ticks since its tick. (test)
5. At a node, a field's sampleNode is given every end of every edge meeting the node, each with the source's entries strictly inside that edge. (test)
6. edgeEnds gives each end of each edge at a node exactly once, in ascending edge index with an edge's From end before its To end. (test)
7. A kept field call that throws changes nothing. (test)
8. replaceKeptEntries, called by a finisher, makes the source's held entries exactly the ones given. (test)
9. A saved world, loaded and resolved, reads exactly as the world saved, and reading a world never changes its save. (test)
10. Every existing test passes unchanged, and tpj_scenarios's windows-debug output over the registered scenarios and tests/parks/*.park is identical, line for line, before and after the feature, by tpj_scenarios --compare (decision 0030). (check)

## Medium

The feature adds a kind of field to the shared medium and no field of that kind. Kept fields are sampled through the same sampleField and fieldValue as every field, so a sampler needs no new call to read one. keepEntry and replaceKeptEntries are the producer's side: a kept field's owning module's systems change its entries and its finisher carries them. edgeEnds is a network query any module may use. Route distance, the food offer, and hungry footfall are unchanged, and are still registered with addField.

## Principle checks

- Principle 1: the order by place kept beside a source's entries is derived and never saved (criterion 9).
- Principle 10: no system reads a change made in its own tick, so the order systems run in cannot change what they read (criterion 2).

## Spec changes

src/sim/medium/SPEC.md, appending to the paragraph beginning "A field is a set of entries published at places":

"A scalar field's type may instead define a static function readKept, its owner's rule for reading a kept entry, which makes it a kept field (Kept fields). A kept field may also define a static function sampleNode, its owner's rule at nodes."

src/sim/medium/SPEC.md, appending to the paragraph beginning "addField registers a field with a schema":

"A kept field is registered with addKeptField instead, and a compile-time check refuses one given to addField."

src/sim/medium/SPEC.md, after the sentence beginning "stopPlaces gives a node's stop places":

"edgeEnds gives the ends at a node of the edges meeting it: for each end at the node of each edge, the edge's index in edges() and whether the end is its From, in ascending edge index with an edge's From end before its To end, so an edge whose two ends are the node appears twice. Every node is a stop, so every node has an end. The network builds them once, in its constructor, and edgeEnds throws std::out_of_range for a node not below the node count."

src/sim/medium/SPEC.md, in the paragraph beginning "A sample resolves only its own place", replacing the sentence "Publishing, swapping, settling, copying, and loading make or remove whole slots, and nothing changes a slot's entries in place, so a kept order always equals orderByPlace of the slot's entries." with:

"Publishing, swapping, settling, copying, and loading make or remove whole field slots, and nothing changes a field slot's entries in place, so a field slot's order always equals orderByPlace of its entries. orderItemsByPlace(items) gives the same order for any list of items with places, field entries or kept entries, and orderByPlace(entries) is it for a field's entries. A kept field's swap changes its sources' entries in place, but never an entry's place, and adds an entry only with its position in the order, so a kept source's order, entriesByPlace of its slot, always equals orderItemsByPlace of its entries."

src/sim/medium/SPEC.md, a new subsection after the paragraph beginning "fieldValue gives a scalar field's value":

"### Kept fields

A kept field holds kept entries, values that accumulate where movers go and persist between ticks with no source republishing them (decision 0031). It has no resolved or stepped entries. addKeptField registers it with a schema: the state component type named its name followed by -kept, which holds its kept entries; the resolver named its name followed by -field, which creates the field's entity, fieldKey(name), holding empty kept entries when it holds none, and changes none it holds; and a swap function. It throws std::invalid_argument when any of its names is malformed or already registered, so a field cannot be registered both ways. Kept entries are state, and saves, hashes, and worldsEqual see them (principle 1).

Each source holds its kept entries in the order their places were first kept, each a place, a value, and a tick, the value of World::Tick when the value became readable. keepEntry and replaceKeptEntries keep at most one entry at a place. A system calls keepEntry(world, source, place, value) while stepping to set the source's entry at the place. The change is held, unread, until the swap that ends the tick, so no system reads a change made in the same tick. The field's swap runs among the other swaps in the order addKeptField registered it, and applies the tick's changes in the order they were made. A change at a place where the source has an entry sets the value of the first such entry in the source's order, and otherwise adds an entry after the source's others, adding the source, in ascending key order, when it had none. Either way the entry's tick becomes World::Tick, which has already advanced, so a later change in the same tick at the same place is the one that stays. The entry is then readable, unchanged, until a change or a replacement at its place: nothing republishes it, and a tick's swap visits only that tick's changes. A change at a place the source holds costs a binary search, and one at a new place, or from a new source, also shifts the positions after it in the source's order or the field's sources. keepEntry throws std::logic_error when the world is not stepping, or holds no kept entries for the field, as before its first resolution, and std::invalid_argument for the null source key and for a place whose distance is NaN. A keepEntry that throws changes nothing.

A kept entry is read through its owner's rule: read in a world whose tick is t, an entry has the value readKept(value, t - tick), or readKept(value, 0) when its tick is later than t. sampleField of a kept field gives, for each source holding kept entries, in ascending key order, the read values of its entries at the place, and none at a place that does not resolve on the network. At a place strictly inside an edge, a field without sampleEdge gives the source's entries at the same place, and a field with it is given an EdgeSample of the source's read values, as for any field. At a node, a field without sampleNode gives the source's entries at the node's stop places, in its order. A field with sampleNode calls it once for each source with an entry at the node's stop places or strictly inside an edge meeting the node, in ascending source order, and what it returns are that source's entries at the node. It receives a NodeSample: the node; the source's read values at the node's stop places, in its order; and one end for each of edgeEnds(node), in that order, each with its edge, whether the node is the edge's From end, and the source's entries strictly inside the edge, each with its FromOffset and ToOffset, as resolve gives them, and its read value, in the source's order. fieldValue sums what sampleField gives, as for any scalar field. keptValue(world, source, place) gives the read value of the source's first entry at exactly the place, or none, and none for a place whose distance is NaN, which is at no place; it does not resolve the place on a network. keptEntries(world, source) gives the source's kept entries as held, unread, in its order, or an empty list, so its owner can carry them.

A kept field's owner carries its entries across a resolution in its own finisher. replaceKeptEntries(world, source, entries) makes the source's kept entries the given ones, in the given order, readable at once, and an empty list removes the source. It throws std::logic_error when the finishers are not running, or the world holds no kept entries for the field, and std::invalid_argument for the null source key, for an entry whose distance is NaN or whose tick is later than World::Tick, and for two entries at the same place. A replaceKeptEntries that throws changes nothing.

A kept source's entries are found through an order by place kept with them, as other slots' are: made the first time it is needed after a load or a replacement, and kept as the swap adds entries, so a change at a place the source holds costs a binary search, and a read a few. The order is derived and never visited."

src/sim/SPEC.md, appending to the paragraph beginning "Resolvers derive the world's derived data":

"isFinishing is true only while the finishers run, so functions meant only for finishers can refuse other callers."

## Files affected

- Create: src/sim/medium/kept_field.h
- Modify: src/sim/medium/field.h, src/sim/medium/network.h, src/sim/medium/network.cpp, src/sim/world.h, src/sim/medium/SPEC.md, src/sim/SPEC.md
- Test pass: tests/sim/medium/kept_field_test.cpp (create), tests/sim/medium/network_test.cpp (modify), tests/sim/CMakeLists.txt (modify)

## Dependencies

- indexed-sampling's place-indexed-entries: the order by place and the position helpers kept slots reuse. Met.
- Decision 0031. Met.

## Out of scope

- Moving hungry footfall onto kept entries, its decay rule, its node mean, and remaking the parks: following-footfall's footfall-on-kept-entries.
- Kept fields whose entries are not doubles: kept fields are scalar, as every accumulating metric decision 0031 names is.
- sampleNode for fields registered with addField: no such field needs it.
- Removing a single kept entry, or a kept entry that decays to nothing being dropped: a replacement in the owner's finisher removes entries, and no metric needs more.

## Spec decisions

- Places compare as numbers, as orderByPlace already orders them, so distances 0.0 and -0.0 are one place.
- addField's refusal of a kept field is a compile-time check; it holds by building, and no test observes it.
- The refusal conditions, the names addKeptField registers, where its swap runs among the swaps, and the swap's cost are spec rules with no test of their own; one refused call shows a refusal changes nothing.

## Open questions

None.
