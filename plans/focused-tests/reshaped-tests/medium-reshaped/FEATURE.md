# Feature: Medium Reshaped

## Summary

The medium's spec states groundPoint and nearestPlace as the properties they compute instead of the formulas that compute them. plans/focused-tests/RESEARCH.md lists 19 of the medium's test cases as rewrite. Each now asserts the property its entry gives, merged where the entries say. The test-writer rewrites them in rewrite mode, the medium's entries leave the inventory, and test support no remaining test uses is deleted.

## Acceptance criteria

1. The medium's tests assert each of these properties once, in place of the cases the inventory lists for carry_over_test, field_index_test, field_test, flow_test, kept_field_test, network_test, and stepped_field_test:
    - A place whose carrier's geometry changed carries over to nearestPlaceOn, in the network after, of its carrier and its ground point in the network before. Shown on two inputs. One is a carrier moved aside with another carrier now lying nearer its old line, which shows the place stays on its own carrier. The other is a carrier whose distances before are unlike its ground lengths, which shows the ground point is the network before's.
    - Every field slot's order by place, entriesByPlace, equals orderByPlace of its entries, whatever made the slot. Shown on slots made by resolving, stepping, and a resolution after a command, and on a copy and a load of that world, neither of which carries the order.
    - addField registers the field's -resolved derived type, its -stepped state type, and its -field resolver.
    - A refused publication into a field changes nothing, shown with one refused call.
    - The order sources publish in does not change a field, in either layer: worlds whose sources publish in different orders, while resolving and while stepping, are equal by worldsEqual.
    - At a place that resolves to a node, a source's sampled entries are those whose places resolve to that node, whatever carrier they name, in the source's order. Shown on a source whose entries include one at -0.0 and one at 0.0 where a stop is at 0, two at the same place, and one whose distance is NaN, which is never sampled.
    - At a place strictly inside an edge, a field without sampleEdge gives each source's entries whose places equal it. Shown with entries, and sampled places, one representable step inside each end of an edge, which are never taken for the edge's nodes.
    - sampleEdge is given the edge, the place's offsets as resolve gives them, and the source's entries at the edge's From node, at its To node, and strictly inside it, each in the source's order. Shown with entries one representable step inside each end of the edge, which it is given as inside it.
    - A field samples sources in ascending key order across both layers, and each source's entries in the order it gave them. Shown with sources in both layers each holding several entries, published in an order other than ascending key.
    - A refused flow operation changes nothing, shown with one refused call.
    - A kept entry read in a world whose tick is t has the value of its owner's rule given t minus the entry's tick, or given 0 ticks when the entry's tick is later than t. Stepping only advances a world's tick, so the later-tick case reads a copy of a stepped world whose public World::Tick is set below the entry's tick, never a save whose text was edited.
    - sampleField of a kept field without sampleNode or sampleEdge gives, for each source in ascending key order, the read values of its entries at the place.
    - A kept field's sampleEdge is given the read values of the source's entries.
    - A source holds one kept entry at each place it has changed, in the order its places were first kept, with the last value given there and the tick of the swap that applied it. Shown on a new world and on a world loaded from a save, since a load rebuilds the order by place.
    - The network constructor refuses malformed input with std::invalid_argument, shown with one refused input.
    - A place whose carrier is absent, or whose distance lies outside its carrier, resolves to nothing. Shown one representable step past each end of a carrier and on one absent carrier.
    - nodePlace and nodeAnchor throw std::out_of_range for a node not below the node count, shown with one call.
2. field_test's "isResolving is true while a resolver runs and false otherwise" is deleted, since cycle_test's "isStepping, isResolving, and isFinishing are each true exactly while their stage runs" shows its property.
3. src/sim/medium/SPEC.md states groundPoint and nearestPlace as properties, as Spec changes gives. Existing tests outside the inventory already assert what the changed statements say, and stay:
    - network_test's "groundPoint gives a carrier point's coordinates at its distance, and between points interpolates by distance", whose values interpolating along the whole line would miss;
    - "nearestPlace breaks ties by lower carrier key, then lower distance";
    - "nearestPlace projects onto a segment whose points share a ground position at its first point".
4. The medium's entries are removed from plans/focused-tests/RESEARCH.md.
5. No definition in tests/sim/support/synthetic_fields.h, synthetic_flows.h, or synthetic_network.h is left that no test uses, directly or through another definition in its header. field_index_test.cpp keeps no declaration that no case uses.
6. Nothing under src/ changes except src/sim/medium/SPEC.md, and tpj_scenarios's windows-debug output is identical before and after.
7. keptValue gives none at a place where the source holds no entry, shown at one such place beside places it holds. The read-rule case asserted this beside its own property before the rewrite, and no other test does.

## Medium

None. The feature changes tests and one spec. It samples, emits, draws, and supplies no fields or flows. Each rewritten test reaches the medium through its public headers. The world-as-value standards stay proven once, by the shared world checks. The publication-order property compares worlds without their hashes, and is the medium's own ordering rule. Flow conservation stays with flow_test's conservation case, which the inventory does not list.

## Spec changes

src/sim/medium/SPEC.md:

- "Otherwise, with a and b the points before and after the distance d, it is a.X + t * (b.X - a.X) and a.Z + t * (b.Z - a.Z), where t = (d - a.Distance) / (b.Distance - a.Distance)." becomes "Otherwise it is the point on the segment between the carrier points before and after the distance, the same fraction of the way along it as the distance is between theirs."
- "It projects the position onto each segment of each carrier's polyline, clamped to the segment, and a projection at the fraction t of a segment from a to b is the ground point a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z) at distance a.Distance + t * (b.Distance - a.Distance). When t is 0 or 1 it is exactly a or b, with its coordinates and distance," becomes "It projects the position onto each segment of each carrier's polyline, clamped to the segment, and a projection some fraction of the way along a segment is the ground point and the distance that same fraction of the way between the segment's two points. At either end of a segment it is exactly that point, with its coordinates and distance,"
- "A segment whose two points share a ground position projects to a, with t 0." becomes "A segment whose two points share a ground position projects to its first point."

## Files affected

- Modify: src/sim/medium/SPEC.md
- Modify: tests/sim/medium/carry_over_test.cpp, field_index_test.cpp, field_test.cpp, flow_test.cpp, kept_field_test.cpp, network_test.cpp, stepped_field_test.cpp (the test-writer)
- Modify: tests/sim/medium/field_index_test.cpp, for its unused declarations
- Modify: tests/sim/support/synthetic_fields.h, synthetic_flows.h, synthetic_network.h, for definitions no remaining test uses
- Modify: plans/focused-tests/RESEARCH.md

## Dependencies

sim-root-reshaped, merged: the rewrite dispatch, and cycle_test's stage-flag test.

## Out of scope

- The names, keys, and orders RESEARCH.md lists as kept.
- Medium tests the inventory does not list, such as the flow conservation case and nearestPlace's bound, which both stay.

## Open questions

None.
