# Milestone: Following Footfall

Slice: none

Status: complete

## Summary

following-footfall makes hungry footfall's tick work grow with the guests rather than with the guest network, as decision 0031 requires. Today stepFootfall visits every stretch of the guest network every tick, recomputes its moving average, and republishes one entry per stretch and one per node, because the medium's stepped layer drops any entry a source does not republish each tick. On today's stress parks that cost is likely a small share of a tick, since their guest networks have a few hundred stretches. It is the only per-tick work that grows with the network, though, and paths-and-plazas derives networks several times larger from paved ground, so it is rebuilt before that slice grows them. The milestone gives the shared medium a third kind of field entry, a kept entry, which a field opts into: a system changes it at one place, and it stays readable until changed, carrying the tick it last changed so its owner can decay it when it is read. Hungry footfall then moves onto kept entries: each tick, only the stretches guests stand on change, and an empty stretch's value decays by a stated rule when it is read or next changed. Nothing in the simulation reads hungry footfall; its one reader is the shop ghost's tooltip, so no guest or shop behaves differently. Its values differ from today's only in their last bits after a stretch sits empty, and its saved form changes, so world hashes and the parks that hold footfall change with it.

## Acceptance criteria

- The shared medium holds kept entries beside resolved and stepped ones, for a field that opts into them by its own registration call, so every other field's schema, registered types, and saves are unchanged. A system changes a source's kept entry at one place during a tick, and the change becomes readable at the swap that ends the tick, so systems in the same tick never see each other's changes and the order they run in cannot change a result. A kept entry stays readable through later ticks until it is changed again, with no source republishing it. Each holds the tick it last changed, and a read of it goes through its field's owner's rule, given that tick and the tick of the read. A field's owner may also give a rule for reading at a node from the kept entries of the stretches meeting it. Kept entries are state, saved and hashed, carried across a resolution by their owner's finisher, and a tick's swap costs what the tick changed, never what the field holds. src/sim/medium/SPEC.md states all of this, with its layer rule saying which layer a source is sampled by.
- Hungry footfall is held as kept entries, one per stretch any guest has stood on. Each tick its system visits each guest once, sums the hunger of the guests on each stretch, and changes only those stretches. It visits no stretch no guest is on and publishes nothing per node, so its tick work grows with guests, not the network. src/sim/guests/SPEC.md states the field's rule: a stretch with guests on it takes today's step, V plus (S minus V) over FOOTFALL_TIME, from its value decayed to the tick before; an empty stretch's value decays by a stated rule computed from the ticks since it last changed, using only basic operations or simExp and simLog, so it is identical on every build (decision 0022); and a node's value is the mean of the values of the stretches meeting it, worked out when read. The field still means what it means today: each stretch's moving average, with time constant FOOTFALL_TIME, of the summed hunger of the guests on it.
- A stretch never stood on reads 0.0. A resolution carries each kept value to its new place by carryOver from the network before, as carryFootfall does today, so untouched stretches keep their values, a split stretch's value goes to the half holding its old place, and a deleted path's values are dropped. After a resolution each stretch holds at most one kept entry, at its midpoint: values carried into the same stretch, as when two stretches join, are each decayed to the resolution's tick and added, so joined stretches' values add as they do today. This work is over the whole network, which decision 0031 allows in resolution and never in a tick. A candidate's footfall is carried as committing its edit carries it (principle 8).
- shopContext's footfall, through fieldValue on the committed world, is unchanged in what it reads: the value at the shop's connection at the world's tick.
- The parks that hold footfall are remade by the tools that made them: tests/parks/fed.park edited to the new form by hand, since it is the source, and warm.park and cut.park remade from it by tpj_scenarios --slice-parks; tests/parks/stress/full.park by tpj_bench --full-park; and tests/parks/stress/winding-path.park, a player's save no tool makes, converted by a one-off edit of its text that keeps its footfall values, held as changed at the save's tick. The remade warm.park, cut.park, and full.park each differ from the ones before only in footfall's sections, which shows that thousands of ticks of guests, shops, and shipments run exactly as before.
- Existing tests pass unchanged except hungry footfall's own and the guests module's registration order, which the test-writer rewrites from the new specs, and the medium's tests gain kept entries. tpj_scenarios's windows-debug output changes, since footfall is in every hash, and that change is this milestone's stated exception to the capability's identical-output rule; the remade parks above are its evidence instead.
- The milestone's report compares the runtime report before and after on every stress park, and its ticks median is not slower on any of them beyond the spread both reports measured.
- No test, hook, or check fails on a timing. windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean. Linux is checked at release, not by the milestone (decision 0030).

## Medium

The shared medium gains kept entries, a third layer a field opts into by its own registration call, so route distance's and the food offer's schemas and saves are unchanged, with a system's change to one place, the swap that makes changes readable, owner rules for reading a kept entry at a tick and for reading at a node, and saving. kept-entries supplies them, and following-footfall is their first user. Hungry footfall, the field named hungry-footfall on the guest network, keeps its name, its meaning, and its one reader, legible's shopContext, and moves from the stepped layer to the kept layer. Route distance, the food offer, and every other field and flow are untouched. The milestone samples and emits nothing new, and the runtime report reads parks only through tpj_bench's public headers, as before.

## Dependencies

- measured-runtime and indexed-sampling: the stress parks, tpj_bench, and the report with its comparison. Met.
- Decision 0031, which requires this work and allows a metric's spec to state its own decay rule: met.
- simExp and simLog in sim/sim_math.h: met.
- Must land before paths-and-plazas' ground-becomes-routes grows the guest network (plans/slices/paths-and-plazas/SLICE.md).

## Core feature

`kept-entries`, because footfall cannot stop republishing until the medium can hold a value nobody republishes. It is small, its contract is the medium's own, and its tests prove it on their own; it is also the layer decision 0031 says every later accumulating metric, such as litter or crowding, is built on.

## Features

1. `kept-entries`: the shared medium's kept layer, which a field opts into by its own registration call: a system's change to one source's entry at one place, held aside during the tick and made readable by its swap, entries that stay readable until changed with the tick they last changed, owner rules for reading at a tick and at a node, the layer rule naming which layer a source is sampled by, saving, and an index by place kept as entries are added rather than rebuilt. Depends on: none.
2. `footfall-on-kept-entries`: hungry footfall moved onto kept entries, changing only the stretches guests stand on each tick, with its decay and node rules stated in src/sim/guests/SPEC.md, its carrying across resolutions, the parks that hold it remade or converted, and the runtime report compared before and after. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

None.

## Open questions

None.

## Research notes

- Forward decay holds each value with the time it last changed and applies the decay owed when it is read; exponential decay over any idle gap is one factor raised to the gap's length, computable identically on every build with simExp and simLog or repeated squaring.
- Replaying each idle tick on read was rejected: a stretch idle for an hour of play owes about 100,000 steps. Making footfall a guests-module query outside the medium was rejected: it leaves the shared medium, and decision 0031 expects later metrics to be kept the same way.

Depth is in RESEARCH.md.
