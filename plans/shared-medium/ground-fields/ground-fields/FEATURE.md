# Feature: Ground Fields

## Summary

Adds the ground field to the shared medium: a field with one entry for each entity at a ground position, set and dropped by systems while stepping, kept from tick to tick, and sampled within a radius of a ground point in key order. When an entity is gone, sampling stops giving its entry, and the next swap removes it. Samples find entries through an order by cell, so a sample's work follows the entries near its point. tpj_scenarios's stalls scenario gives its visitors a ground field, and their choices depend on what they sample, so the cross-build check covers it. believable-guests' spread-guests registers the first park field of this kind, guest presence.

## Acceptance criteria

1. A module defines a ground field as a type with a name and an entry type, and registers it with addGroundField. If another field already has the name, registration throws std::invalid_argument. Confirmed by review of the header and by the stalls scenario building and running.
2. sampleGroundField gives exactly the entries within the radius of the point, the radius included, with key, position, and value, in ascending key order. It gives nothing for a negative or NaN radius or a point that isn't finite. It reuses the caller's list. Confirmed by review. spread-guests confirms it in play.
3. A change made while stepping tick N is readable from tick N+1, and the entry stays as it was until it changes again. A second change to one entity in one tick throws std::invalid_argument and changes nothing, including a drop for an entity with no entry. Confirmed by review.
4. Sampling never gives the entry of an entity that is gone, and the swap after its removal drops the entry. Confirmed by review. spread-guests confirms it in play.
5. Entries are state, which copies, saves, hashes, and comparisons cover, and resolution doesn't change them. The cell order is never visited. Confirmed by review now. The park world checks confirm it once spread-guests' warm-main-street.park holds guest presence.
6. In the stalls scenario, each visitor sets its entry every tick. It samples the entries near it when it chooses a stall, and what it finds changes the draw it chooses by. Its reach is small enough that samples read through the cell order. A visitor with an even key drops its entry in its last tick, and the rest are removed still holding theirs. Confirmed by a check: `build/windows-debug/tpj_scenarios.exe --ticks 600` before and after the feature gives different `scenario stalls` lines, and every other line is the same.
7. src/sim/medium/SPEC.md, src/scenarios/SPEC.md, and plans/shared-medium/CAPABILITY.md describe ground fields as built, and no longer say every field is sampled at network places. Confirmed by review.

## Tests

None. The feature adds medium machinery with no park field yet. spread-guests' park files bring guest presence under the park world checks (docs/testing.md).

## Medium

The feature adds the ground field kind. It registers no park field. The stalls scenario's visitor presence field is internal to tpj_scenarios. Each visitor sets its own entry and samples the others', so visitors meet only through the field.

## Spec changes

src/sim/medium/SPEC.md:

- The opening's "the fields sampled on them" becomes "the fields sampled on them or on the ground".
- Boundary list gains "sim/medium/ground_field.h for ground fields". The registration sentence becomes "It registers them with addField, addKeptField, addGroundField, or addFlow, which register their components and resolvers with the schema."
- In Places, the straight-line measures sentence becomes: "nearestPlace, nearestPlaceOn, and directionAt, carryOver's check that a snapped place lies on a place's old ground, and the radius a ground field is sampled within are the medium's only straight-line measures. The first two and that check are used only to snap a ground position onto a network, directionAt only to scale a direction to unit length, and a ground field's radius only to find the entities near a point. None of them measures how far anything travels (principle 4)."
- A new section after Kept fields:

  ## Ground fields

  A ground field holds one entry for each entity at a ground position, instead of entries at places on a network. Through it, an entity senses the entities near it on the ground, as guests do to step around one another (principle 3). Its owning module defines it as a type naming its name and its entry type. addGroundField registers it, and refuses a malformed name, or a name another field has, with std::invalid_argument. Its entries are state, which saves, hashes, and worldsEqual see (principle 1). Resolution leaves them as they are.

  A system calls setGroundEntry while stepping to give an entity's entry a ground position and a value, and dropGroundEntry to remove it. Changes become readable at the swap that ends the tick. An entry then stays readable, unchanged, until it changes again. Each entity's entry changes at most once a tick, and a drop counts as a change even when the entity has no entry. If a change comes outside stepping, for the null key, at a ground position that isn't finite, or for an entity already changed this tick, it is refused and changes nothing. The result is then the same whatever order systems run in (principle 10).

  When an entity is gone, sampling gives no entry for it, and the next swap removes its entry.

  sampleGroundField fills a list with the entries whose ground position is within a radius of a point, the radius included. Each comes with its entity's key, its position, and its value, in ascending key order. If the radius is negative or not a number, or the point isn't finite, it gives nothing. It reuses the list it is given, and allocates only while the list or the field grows, so systems can sample for every mover each tick. A sample's work grows with the entries near its point, not with every entry in the field. The swap's work grows with the field's entries, never with the size of the park (decision 0031).

src/scenarios/SPEC.md: in Scenarios, "the medium's fields and flows" becomes "the medium's fields, ground fields, and flows".

## Files affected

- Create: src/sim/medium/ground_field.h
- Modify: src/sim/medium/SPEC.md
- Modify: src/scenarios/stalls.h
- Modify: src/scenarios/stalls.cpp
- Modify: src/scenarios/SPEC.md
- Modify: plans/shared-medium/CAPABILITY.md

## Dependencies

first-field-and-flow's field registration, field entities, and swaps. No sibling feature.

## Out of scope

- Guest presence and any other park field of this kind. spread-guests adds guest presence.
- Ground fields that spread over open ground, and scalar ground fields. These are in the milestone's deepening candidates.
- Reading one entity's own entry by key. No consumer needs it yet.

## Open questions

None.
