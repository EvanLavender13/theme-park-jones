# Milestone: Ground Fields

Slice: good-paths

Status: complete

## Summary

ground-fields adds a second kind of field to the medium. Its entries sit at ground positions instead of network places, one entry for each entity, and it is sampled within a radius of a ground point. The good-paths slice needs it so guests walking on the paving can step around one another through the medium (principles 3 and 6). Each guest publishes where it stands and how it walks, and each guest senses the guests near it, without reading their state. Fields on networks can't do this, because nearness across a wide path is a straight line, not a route. This milestone builds only the mechanism, as first-field-and-flow did for fields on networks. believable-guests' spread-guests registers the first real one, guest presence. The milestone comes before spread-guests because spread-guests can't sidestep without it, and it depends on nothing else in the slice.

## Acceptance criteria

1. A module defines a ground field as a type giving its name and its entry type, and registers it with the schema. A ground field's name can't repeat any other field's name. (build: the synthetic scenario in criterion 6 registers one)
2. Sampling a ground field at a ground point within a radius gives every entry whose position is within that radius, the radius included, each with its entity's key, its position, and its value. The entries come in ascending key order, and no other entry comes. (play: spread-guests' guests step around only the guests near them)
3. While stepping, a system sets or drops the entry of the entity it acts for. A change made in tick N is readable from tick N+1 on, and the entry then stays as it was, without being published again, until it is changed or dropped. If a system changes one entity's entry twice in a tick, the second change is refused and changes nothing, so the order systems run in never changes the result (principle 10). (review)
4. If an entity is gone, sampling never gives its entry. Its entry is dropped at the next swap, so the field holds entries only for entities in the park. (play: guests stop stepping around a guest the moment it leaves)
5. Ground field entries are state. Copies, saves, hashes, and comparisons cover them, and resolution leaves them as they are, since a ground position doesn't depend on any network. Anything a field keeps to find entries faster is derived from the entries and never saved. (the park world checks, once spread-guests' warm-main-street.park holds guest presence; they fail if no checked world holds a registered type, so spread-guests can't skip this)
6. tpj_scenarios's synthetic stalls scenario gives its visitors a ground field. Each visitor sets its entry where it stands, samples the entries near it, and is removed when it retires. The keys it finds change its choice of stall, so a sample that differs between builds changes the hash, and the cross-build check covers setting, sampling, and dropping. (run `tpj_scenarios` and confirm the stalls lines change from before; scripts/cross-build-check.sh at release)
7. A sample's work grows with the entries near its point, not with every entry in the field, and a tick's work on a ground field grows with its entries, never with the size of the park (decision 0031). (the runtime report on the stress parks after spread-guests)
8. src/sim/medium/SPEC.md describes ground fields, and lists the sampling radius among the medium's straight-line measures. It measures how near entities are, never how far anything travels (principle 4). This capability's brief says the same in its Medium and Principles sections. (review)

## Medium

The milestone adds the ground field kind to the medium. It adds no park field or flow of its own.

- Ground fields: produced by this milestone as a type, as fields on networks are. Consumer in the slice: believable-guests' guest presence, registered by spread-guests. The stalls scenario's ground field is internal to tpj_scenarios.
- Fields on networks, kept fields, and the flow ledger are unchanged.

## Dependencies

- first-field-and-flow: field registration, the field entity, swaps, and finishers. Met.
- deterministic-simulation's world-as-value: the walk, saves, copies, hashes, and the cross-build check. Met.
- No other slice member. spread-guests depends on this milestone.

## Core feature

`ground-fields` is the core and the only feature. Storage, the swap, dropping gone entities, and sampling make one usable piece. Without sampling, a field that holds entries does nothing, and without a fast lookup, sampling costs guests times guests each tick, so neither half is worth landing alone.

## Features

1. `ground-fields`: the ground field type, its registration, setting and dropping entries while stepping, the swap that makes changes readable and drops gone entities' entries, sampling within a radius in key order through an order by cell rebuilt at the swap, the stalls scenario's ground field, and the medium spec and this capability's brief. Depends on: none.

The feature adds no test (docs/testing.md). spread-guests' park files bring guest presence under the park world checks.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Ground fields that spread over open ground, such as noise and smell, read by a rule that weakens with distance, and scalar ground fields whose value is the ordered sum of their entries. Gated on: a mechanic that needs one.

## Open questions

None.

## Research notes

- Crowds find their neighbours through a uniform grid refilled once per update and queried by the cells a circle overlaps (Detour's proximity grid, OpenSteer's bin lattice). A ground field keeps its entries sorted by cell as derived data, so a rebuild grows with the entries and not the park, and sorts each sample's entries by key so the order is fixed.

Depth is in RESEARCH.md.
