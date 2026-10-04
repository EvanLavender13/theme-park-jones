# Research: medium-reshaped

## Which statements of src/sim/medium/SPEC.md are trimmed?

Two statements copy a formula from the code. groundPoint's gives the interpolation a.X + t * (b.X - a.X) with t = (d - a.Distance) / (b.Distance - a.Distance). nearestPlace's gives the projection onto a segment written out the same way, and adds that a zero-length segment projects "to a, with t 0". Each becomes the property it computes: the point the same fraction of the way along the segment as the distance is between its two points. No other module's spec or code depends on the terms. legible, render, guests, and routes use groundPoint's and nearestPlace's results, never the interpolation. Three things stay, each with an existing test that is not in the inventory:

- At either end of a segment the projection is exactly that point, which makes a junction equally near on every carrier. "nearestPlace breaks ties by lower carrier key, then lower distance" shows it.
- A zero-length segment projects to its first point. "nearestPlace projects onto a segment whose points share a ground position at its first point" shows it.
- groundPoint interpolates within a segment. "groundPoint gives a carrier point's coordinates at its distance, and between points interpolates by distance" shows it with BEND, whose segments cover 10 and 2 of distance for 4 and 3 of ground. At distance 7.5 it expects (3, 0), where interpolating along the whole line would give (4, 0.375).

Every other detail the survey could flag stays, because something outside the medium depends on it:

- Saves hold the -stepped, -kept, and -ledger names as section headers; the park files in tests/parks hold eight such sections.
- Saves hold fieldKey and flowKey as entity keys.
- Other modules name -field resolvers as dependencies, as operations.cpp, route_distance.cpp, and scenarios' stalls.cpp do.
- -resolved names a registered type, which no other type may share.
- The ledger's packet and stock order is addressedTo's result and part of the saved ledger.
- orderByPlace's order is that public function's result, which field_index_test's orderByPlace case, not in the inventory, asserts.
- The kept entries' order is keptEntries' result and part of the saved kept entries.
- fieldValue's sum in sampleField's order decides other modules' results bit for bit.
- The cost statements, such as a few binary searches per source, state complexity, not layout.

## Which entries point at cases that no longer exist?

Commit fe946fc deleted 231 cases across the suite, including several whose inputs or checks the medium's entries name. The criteria therefore state those inputs themselves, so the test-writer needs neither the deleted code nor git history.

- field_test's node entry says "add field_index's -0.0, repeated-place and NaN inputs". The deleted field_index case sampled a source whose entries included -0.0 and 0.0 on A at node 0, two entries at A's 4, and a NaN distance.
- field_test's inside-edge entry says "add the nextafter-of-a-stop inputs from another test". The deleted case sampled at, and had entries at, std::nextafter(4.0, +infinity) and std::nextafter(10.0, 0.0) on A, one representable step inside each end of the edge from node 1 to node 2.
- field_test's sampleEdge entry says "add field_index's entries nearest each end": the same two entries, given to sampleEdge as inside that edge.
- field_test's addField entry says "absorb stepped_field_test's -stepped State check". fe946fc deleted that check, so the criterion names -stepped itself.
- stepped_field_test's sample-order entry ends "so field_test can go". The field_test case it meant, "sampleField gives sources in ascending key order…", was also deleted by fe946fc. The rewrite still gives resolved sources several entries.

field_index_test still holds declarations only the deleted cases used: the Sampled alias, MixedWorld, the sampled template, ExpectedAlong, and ExpectedCall. No case refers to them.

## Which entries need care?

field_test's "isResolving is true while a resolver runs and false otherwise" asks for one test of the three stage flags. sim-root-reshaped wrote it in cycle_test: "isStepping, isResolving, and isFinishing are each true exactly while their stage runs" records each flag in a system, a swap, a resolver, and a finisher. The medium's case is deleted, not rewritten.

stepped_field_test's "systems that publish into fields give the same world and the same hash…" merges into field_test's publication-order case, which then covers both layers, compares with worldsEqual, and drops the hash. synthetic_fields.h's publish order governs both the resolved publishers and the stepped ones, through sourcesInPublishOrder.

kept_field_test's read-rule case reaches a world whose tick is below an entry's by editing a save's text, which rule 1 forbids for setup. Stepping cannot reach that state. A world's tick only advances, the swap stamps a kept change with the advanced World::Tick (kept_field.h), and replaceKeptEntries refuses an entry whose tick is later than World::Tick. World::Tick is a public member (world.h), and keptValue reads it (kept_field.h's readKeptEntry clamps the elapsed ticks at 0). So the rewrite sets Tick back on a copy of a stepped world.

kept_field_test's two one-entry-per-place cases merge. The merged property keeps "in the order first kept" from the case it merges into, since no other test shows the order keptEntries gives for entries a swap added.

## Which test support can the rewrites leave unused?

synthetic_fields.h is included by field_index, field, field_sampling, kept_field, source_sample, and stepped_field tests. synthetic_flows.h is included by flow_test alone. synthetic_network.h is included by carry_over, network, field_sampling, and sim/routes' route_distance tests. A definition is unused when no includer names it and no used definition in its header refers to it. A check by name over the includers finds 14 names with no includer today, each referred to by a used definition in its own header: publishOrder, SourceEntries, sourcesInPublishOrder, publishSources, NETWORK_PURPOSE, produceNetworks, addSyntheticFields, Trader, traderKeys, issuedKey, tradeFor, anyKey, Churn, and makeFlowSchema. Restricting the search to includers matters, since names such as Layout and evenKey are also defined elsewhere.

GCC's -Wunused-function, an error in windows-debug, catches a file-local function a rewrite leaves unused, but not a type or a template, which is how field_index_test's dead declarations survived.
