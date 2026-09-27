# Research: first-field-and-flow

## How should a place be held, and how is a field sampled between nodes?

Linear referencing in GIS stores a position as a route and a measure along it, never as a piece of geometry or an edge. A place that is a carrier and a distance along it is that model. Because nothing refers to derived edges, splitting or merging them under a place costs nothing. Esri's linear referencing products make relocation after a route edit a declared policy for each kind of event. Stay Put keeps the ground position and recomputes the measure. Move keeps the measure. Retire marks the event historical. Snap moves it to another route that runs alongside. Events on a deleted route are retired, never silently moved elsewhere. The capability's carry-over rule is Stay Put for a changed carrier and Retire for a deleted one, applied to every place alike.

Routing engines separate a place on an edge from a graph node. OSRM's phantom node carries the snapped edge and the partial cost from the snapped point to each end, and Valhalla's candidate carries the edge and a fraction along it. The distance from a mid-edge point to a source is the smaller of the two ends' distances, each plus the point's offset to that end. That is exact for shortest paths, where linear interpolation of the two ends' values is wrong wherever the minimum switches ends. A source on the same edge as the point needs a direct along-edge term. OSRM once measured the reverse offset from the wrong end, which is worth a test of its own.

What this suggests: a field's values between nodes are not a storage question the medium can answer generically. Route distance needs the phantom-node rule, which belongs to route distance, while a scalar field such as footfall is naturally a value per edge. So the medium should give a sample the place's position on its edge, and each field kind should supply how that position reads its stored entries.

Rejected: holding positions as an edge and a fraction, since derived edges do not survive re-derivation. Interpolating node distances linearly, since it is wrong wherever the nearer end changes. Reprojecting a place onto another carrier, since it needs a notion of carriers running alongside that nothing in the slice asks for.

Sources: https://enterprise.arcgis.com/en/pipeline-referencing/10.5/get-started/event-behaviors.htm — Stay Put, Move, and Retire; https://desktop.arcgis.com/en/arcmap/latest/extensions/roads-and-highways/event-behaviors.htm — Snap, and retiring events on edited sections; https://github.com/Project-OSRM/osrm-backend/blob/master/include/engine/phantom_node.hpp — per-end offsets of a snapped point; https://github.com/Project-OSRM/osrm-backend/pull/5315 — the reverse-offset bug; https://valhalla.github.io/valhalla/loki/ — edge candidates with a fraction along.

## What happens to units whose destination or sender is removed?

Logistics games differ, and the ones players trust keep the units. OpenTTD reroutes cargo whose next stop is removed, and when a source station is deleted it keeps the packets and clears their origin. Factorio's robots drop cargo they cannot deliver into a storage chest, and its trains wait with cargo aboard. Satisfactory moves a dismantled belt's items into the player's inventory and spills overflow into a ground crate. Transport Fever 2 removes or teleports cargo inconsistently, and players report it as a bug.

Accounting keeps the same identity the ledger needs. Goods in transit are a real balance owned by one party. A suspense account holds entries whose destination is not yet known. A loss is an explicit write-off that moves value into a named expense, never a silent removal.

What this suggests: return to sender covers the case the slice needs, and when the sender is gone too, the units need a named destination. That is either a held stock with no owner, which something must later collect, or consumption with a recorded cause, which is a write-off. Either keeps created units equal to units in transit, held, and consumed.

Rejected: silent deletion, which breaks the identity. Teleporting units home, which hides the delay and the time in transit. A packet that keeps pointing at a removed endpoint, which leaves a reference to nothing.

Sources: https://docs.openttd.org/source/db/d18/classVehicleCargoList — rerouting cargo whose next hop is gone; https://github.com/OpenTTD/OpenTTD/blob/master/src/cargopacket.cpp — packets outlive their source station; https://wiki.factorio.com/Logistic_network — undeliverable robot cargo goes to storage; https://satisfactory.wiki.gg/wiki/Conveyor_Belts — dismantled items kept, overflow to a crate; https://steamcommunity.com/app/1066780/discussions/0/3726197825948021941/ — Transport Fever 2 cargo loss reports; https://en.wikipedia.org/wiki/Suspense_account — holding entries of unknown destination; https://www.netsuite.com/portal/resource/articles/inventory-management/inventory-write-off.shtml — write-offs as explicit entries.

## Where should a field's entries and the ledger's packets live in the world?

Overwatch's ECS holds world-scoped state in singleton components of the world, not in globals, because a replay needed a second world and exposed every global. EnTT's context variables are its equivalent, but world-as-value's walk covers only registered components on keyed entities, so a context variable would sit outside copy, hash, and save. A component on an entity with a derived key serves the same purpose and is covered by the walk as it stands.

EnTT's storage order depends on the history of additions and removals, since removal swaps the last element into the gap. Spreading one field over a component on each source entity would leave its entries in that order, and every sample would need a gather and sort. One table per field, with each source's entries in a slot ordered by the source's stable key, gives the fixed source order the capability asks for whatever order the producers publish in. Ordered containers, never hash maps, hold anything iterated.

A double buffer swapped by exchanging two containers costs nothing, and clearing the new write side afterwards avoids reading two-tick-old data. Swapping keeps value semantics, so a world copy is still a deep copy. Readers hold keys and places, never references into a buffer, so nothing goes stale at the swap.

Rejected: a component per source, whose order is storage history. Globals or statics, which break copies. Hash maps keyed by source, whose iteration order is unspecified. A new kind of world-level registered storage, which would change the walk when a keyed entity already carries the data.

Sources: https://www.gdcvault.com/play/1024001/-Overwatch-Gameplay-Architecture-and — singleton components scoped to the world; https://skypjack.github.io/entt/md_docs_2md_2entity.html — context variables; https://skypjack.github.io/entt/classentt_1_1basic__sparse__set.html — pool order and sorting; https://github.com/Uriopass/Egregoria/issues/90 — ordered over hashed containers for determinism; https://gameprogrammingpatterns.com/double-buffer.html — swapping versus copying buffers.
