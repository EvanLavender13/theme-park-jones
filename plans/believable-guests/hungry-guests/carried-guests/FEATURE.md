# Feature: Carried Guests

## Summary

Guests keep their place when the paths under them are edited. path-networks keeps each network the previous resolution derived, in a derived component beside the new one, and routes publishes it through previousNetwork for finishers to read. A guests finisher carries every guest's place from the previous guest network to the new one with carryOver, and moves a guest whose place is retired, because its path or connector was deleted, to the new network's nearest place to where it stood. A routes finisher that addPark registers after the guests module drops the previous networks, so no resolution ends holding them and saves are unaffected. Because carrying happens in the resolution, every world a cycle leaves and every candidate holds carried places, so a preview's guests stand where committing its edit puts them. With no guest network left, a guest's place resolves nowhere and it leaves the park when it next steps. This replaces eating-guests' interim rule that a guest whose path is edited away leaves the park.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema that has stepped at least one cycle, so it is resolved, N(X) is parkNetwork(X, PathKind::Guest) for a world X, and a guest's place is its record's At.

1. previousNetwork(W, kind) gives none for both kinds, and so does it for every candidate made from W: no resolution ends holding a previous network.
2. In a schema that registers addNetworkComponent, addParkIntent, addParkEdits, and addRoutes, then a finisher of its own, then addDropPreviousNetworks, that finisher reads through previousNetwork, in each resolution after a world's first, a network equal in its carriers, node count, and anchors to the kind's parkNetwork before that resolution, and none in a world's first resolution.
3. For any commands, the candidate C made from W holds exactly W's guests. Each guest's place in C is carryOver of its place in W from N(W) to N(C) when that gives a place; otherwise N(C).nearestPlace of groundPoint of its place in W on N(W), when both give one; and otherwise its place in W. Every other field of its record except Position is as in W, so a waiting guest keeps waiting and a guest heading to a shop keeps its Target.
4. In every world a cycle leaves whose N has a carrier, every guest's place resolves on N.
5. A world stepped a cycle from a world whose N has no carrier holds no guest.
6. In every cycle of randomized park edit sequences (tests/sim/support/route_edits.h) from a park with an entrance, shops, a depot, and guests walking, waiting, and eating, nothing throws, a copy of the world equals it, its save loads back and resolves equal to it, a candidate made with an edit equals the world that queues the same edit, and guest-visits and meals each conserve their units.
7. The cross-build check passes, with the park-edits scenario's guests now carried across its edits.

## Medium

- Networks and carry-over (shared-medium, navigable-networks): path-networks, in the routes module, keeps each kind's previous network in the derived component PreviousNetwork on the network's entity, and publishes it through previousNetwork. The guests module's finisher reads the previous and current guest networks through previousNetwork, parkNetwork, and the Network type's groundPoint and nearestPlace, and carries places with the medium's carryOver. The routes module's finisher drops the previous networks after it. Guests never clear routes' data.
- Guest places: state private to the guests module. The finisher changes only each guest's At.

## Principle checks

- Principle 1: the previous networks are derived and dropped before every resolution ends, so a save never holds them and every world randomized edits with guests reach loads back and resolves equal to itself (criteria 1 and 6).
- Principle 2: a guest whose path is deleted stands on the nearest point of the guest network, and one with no guest network left leaves the park, each a legitimate state (criteria 3 to 5).
- Principle 4: a carried place stays on its own carrier by carryOver. The nearest point for a retired place is the capability's one sanctioned straight-line measure (criterion 3).
- Principle 6: guests read the networks only through previousNetwork, parkNetwork, and the Network type's queries, and routes never reads a guest. Checked by review.
- Principle 8: a candidate's guests stand where committing its edit puts them (criteria 3 and 6).
- Principle 10: carrying is keyed on nothing but the networks and the guest's place, in ascending key order; candidates equal committed worlds, and the cross-build check passes (criteria 6 and 7).

## Spec changes

- src/sim/SPEC.md: a finisher may change state its module's spec says a resolution invalidates, which includes places held on a re-derived network. addPark registers the guests module's finisher with its state and system, and registers the routes module's dropping finisher last.
- src/sim/routes/SPEC.md: addRoutes registers PreviousNetwork, named previous-network, as derived before path-networks. path-networks moves a network already on the entity into it before replacing the network. previousNetwork gives it, or none. addDropPreviousNetworks registers the finisher that removes it, and addPark calls it after every module whose finisher reads it.
- src/sim/guests/SPEC.md: addGuests also registers the finisher carryGuests. A new Carrying section gives the carrying rule. Stepping's leave rule names the case it now covers, a guest network with no carrier, and the module's reads include previousNetwork.
- src/scenarios/SPEC.md: park-edits compares how guests are carried across its edits.

The exact text is in PLAN.md, Tasks 1 to 4.

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/guests/SPEC.md
- Modify: src/sim/guests/guests.h
- Modify: src/sim/guests/guests.cpp
- Modify: src/sim/park_schema.cpp
- Modify: src/scenarios/SPEC.md
- Tests, from the test pass: tests/sim/guests/, tests/sim/routes/, and tests/render/guest_mesh_test.cpp

## Dependencies

- eating-guests, merged: guests that walk, choose, wait at shops, and eat.
- shared-medium's carryOver and nearestPlace, and the sim contract's finishers.
- navigable-networks' path-networks and parkNetwork.

## Out of scope

- A guest walking off a deleted path along the ground, instead of moving to the nearest point of the network: the capability's deepening candidate.
- The ghost drawing its candidate's guests: the milestone's deepening candidate.
- Carrying LastChoice's place. It records where the guest chose, on the network of that tick.
- Other holders of places. Guests are the first; later holders register their finishers before addDropPreviousNetworks the same way.

## Superseded tests

Two existing tests assert the interim rule this feature replaces, and the test pass rewrites them to the carrying rule:

- tests/sim/guests/guest_edits_test.cpp, "A guest whose place stops resolving leaves the park in the next cycle, and guests whose places still resolve stay": with a guest path left, a guest on a deleted path is now carried to the nearest point (criterion 3), and a guest leaves only when no guest path is left (criterion 5).
- tests/render/guest_mesh_test.cpp, partlyStrandedWorld and the tests using it: a guest record with no Position now arises only when every guest path is deleted, until the next cycle.

## Test pass decisions

- Criterion 7 is checked by scripts/cross-build-check.sh, which runs the park-edits scenario, not by a Catch2 test.
- Principle 6, that guests read the networks only through previousNetwork, parkNetwork, and the Network type's queries and routes never reads a guest, is checked by review.
- The finishers' registration order, carryGuests in addGuests and addDropPreviousNetworks last in addPark, is checked by behavior: criteria 1 and 3 through makeParkSchema fail if the previous networks were dropped before guests were carried.

## Open questions

None.
