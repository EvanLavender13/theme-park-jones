# Feature: Route Updates

## Summary

route-updates makes route distance update only the entries an edit changes, instead of searching the whole network from every source on every resolution. Route distance is the largest whole-park cost left in a drag frame: 25% of a shop drag's and 21% of a path drag's on full.park.

After this feature:

- Edge lengths are added in whole millimetres, so cutting an edge in two leaves every distance beyond the cut exactly as it was (MILESTONE.md Decisions, 1; decision 0042).
- Each source's entries are kept between resolutions, ordered by place, so renumbering the network's nodes leaves them in place.
- On each resolution with a previous network, route distance finds the carriers whose stops changed and the nodes they touch. For each source, it updates only the nodes whose distance or next step changes. A source whose anchors changed, and a new source, are searched in full.
- The medium lets a field's resolver edit a source's resolved entries in place and keep the rest, instead of publishing every source again.

## Acceptance criteria

1. Both drags get faster. scripts/runtime-report.sh writes a report of the feature, and its --compare against build/unit-connections.report, written on the code main holds now, marks shop-drag and path-drag faster. Each median falls by at least 1 ms. Route distance took about 1.6 ms of each drag's frame. (check: scripts/runtime-report.sh build/route-updates.report, then --compare build/unit-connections.report build/route-updates.report)
2. A changed frame updates only the route entries its edit changes. For every source but one whose anchors changed, route distance visits only the nodes whose distance changes, their neighbours, and the nodes on the carriers whose stops changed. It doesn't search from each source again. Once per resolution, it matches the new network's nodes to the old network's and compares the two networks' carriers, which grows with the network, as milestone criterion 3 allows for renumbering nodes. (check: read the resolver; a Very Sleepy profile of the shop drag puts route distance under 5% of the frame)
3. The result is exact. The save checks pass on every chosen edit: a world that commits each edit updates route distance in place, and the world loaded from its save searches from nothing. The candidate check's drags on warm.park pass on every frame. (ctest)
4. Route distance is whole millimetres. Every published distance at a node is a whole number of millimetres, and equals the sum of its route's edge lengths, each its two stops' distances rounded to whole millimetres and subtracted. tpj_scenarios --compare of windows-debug output before and after the feature differs, since every world with a guest path hashes differently; if it doesn't differ, the rounding is missing. Later features compare against this feature's output. (check: read millimetresOf, the leaving steps' lengths, and searchEntries' sums; tpj_scenarios --compare exits non-zero)
5. Route distance stays off the allocation check's exception list. watched-resolution found that route-distance, guest-route-distance-field, and backstage-route-distance-field already allocate nothing in the drags and commits. The lists this feature keeps allocate nothing once they have grown, so the check still passes. (ctest: the allocation check)
6. Decision 0042 records whole-millimetre route lengths, CAPABILITY.md lists it among the exceptions, and src/sim/medium/SPEC.md and src/sim/routes/SPEC.md state the new behaviour. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean.

## Tests

None. The save checks and the candidate check's drags already compare route distance updated in place with route distance searched from nothing, on every chosen edit and every drag frame.

## Medium

Route distance's two fields keep their names, entries, and readers. Their entries are now whole millimetres, published in place order. The medium's resolved layer gains in-place edits for a field that asks for them. No field or flow is added or removed.

## Spec changes

src/sim/medium/SPEC.md, in the list of a field's two layers, replace the resolved-entries item with:

    - Resolved entries are derived data (principle 1). Resolvers publish them with publishResolved, which gives the source's entries for the resolution as an empty list for the resolver to fill. The list is readable at once and stays valid until the next publish into the field. Every resolution replaces every source's entries in full. The field keeps the memory of a resolution's entries for the next resolution's publishes.
    - A field can instead keep its resolved entries from one resolution to the next. Its resolver then changes only the sources it edits: editResolved gives a source's entries to change in place, adding the source if it has none, and dropResolved removes a source. Sources it doesn't edit keep their entries. Each is refused, changing nothing, outside a resolver or for the null source.

The paragraph after it, which clears a source's stepped entries when a command changes its resolved entries, holds for edited sources as it is.

src/sim/routes/SPEC.md, Route distance section:

- After "Distances and steps depend only on the network, not on the order a search visits nodes in (principle 10).", add: "An edge's length is the difference of its two stops' distances, each rounded to whole millimetres. A distance is the sum of those lengths, so every distance at a node is a whole number of millimetres. Cutting an edge in two leaves every distance the same (decision 0042)."
- After "Only resolved entries are published, so a resolved park's save holds the fields empty.", add: "A source's entries are ordered by place: carrier key, then distance."
- Replace "The lengths it walks add up exactly to the distance it first sampled." with "The lengths it walks add up exactly to the distance it first sampled, taking each edge's length rounded to whole millimetres, and a first stretch from a place inside an edge as it is."
- Best places: replace "added from the seed's end, as route distance's are (principle 4)" with "added from the seed's end (principle 4)".

## Files affected

- Create: docs/decisions/0042-whole-millimetre-routes.md
- Modify: plans/scalable-runtime/CAPABILITY.md
- Modify: src/sim/medium/field.h
- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/routes/route_distance.cpp
- Modify: src/sim/routes/route_distance.h
- Modify: src/sim/routes/SPEC.md

## Dependencies

watched-resolution, whose allocation check watches route distance in the drags and commits. PreviousNetwork, which path-networks already keeps through every resolution until the finisher drops it.

## Out of scope

- Writing a source's changed entries moves the rest of its list after the first change, once per source. A path drag erases entries on the split segment and inserts entries on new carriers, whose keys sort last, so about 90 guest sources each move most of a list of a few hundred 48-byte entries: about 1.7 MB of memory moved per frame on full.park. It grows with sources times nodes, a defect under docs/conventions.md. MILESTONE.md lists it among the whole-park work left, with this size, for a later feature to fix with storage that doesn't shift.
- Comparing the two networks' carriers and matching their nodes reads the whole network once per resolution. unit-networks would give the changed carriers directly.
- Best seats, which also searches the whole network: a later feature.

## Open questions

None.
