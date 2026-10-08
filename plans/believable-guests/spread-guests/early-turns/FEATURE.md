# Feature: Early Turns

## Summary

Guests decide each turn a little before they reach it. A guest makes its choice for a node when its place comes within TURN_LOOKAHEAD, 10 m, of the node along its route. A wandering guest also picks its way out of the node then. Its route runs on past the node along that way, so it no longer walks toward the middle of every junction and shop connector it passes. It turns only where it actually turns. Guests walk through shaping points without choosing. A shaping point is a node with exactly two edges and no anchor, which only bends a path, so the number of points the player draws no longer changes how guests choose. Guests save the choice they have made for the node ahead. An edit keeps it unless the edit moves the node or the edge the guest picked.

## Acceptance criteria

1. A guest makes its choice for each node of its route when its place comes within TURN_LOOKAHEAD of the node along the route, or when it stands on the node, and it chooses once for each node it comes to. It makes no choice for a shaping point, or for the node it is heading to. A wandering guest picks its way out of the node when it makes that choice, and its route runs on along that way. On main-street.park, wanderers walk past shop connectors and junctions without bending toward their middles, and they bend only where they turn. (play)
2. A guest walks through a shaping point without choosing there. If a point is inserted into a straight guest segment under a wandering guest whose stay is over, the guest is still wandering after it walks through the new point. Before this feature, it chose at the new node and headed home. (listed test)
3. If a wanderer's only way out of a node is back the way it came, as at a path's free end or a door, it walks as far toward the end as its box allows before it turns, as it does today. (play)
4. When the player edits the park, a guest keeps the choice it made for a node ahead if the edit leaves that node where it was. A wandering guest also needs the edge it picked to be whole and where it was. Otherwise the guest makes that choice again. If the player edits a path far from a group of guests, none of them changes course. (play)
5. Guests save the choice they have made for the node ahead. warm.park, cut.park, and stress/full.park are made again. The park world checks, food_loop_test, and the rest of ctest pass. In every park in tests/parks and parks/, no guest leaves the paving: paving-walk's stays check reports 0 guests off the paving. Guests still reach a shop's door and eat: paving-walk's door check reports meals and no guest stuck heading to the shop. (ctest, and the two scratch checks)
6. The runtime report on full.park shows the tick still inside the 33 ms budget. The work of choosing ahead depends on the guest's route ahead, not on the size of the park (decision 0031). (scripts/runtime-report.sh)

## Tests

- tests/integration/early_turns_test.cpp, a guest walking through a new shaping point. The test compares a park with the same park after a change, so it doesn't depend on TURN_LOOKAHEAD's value.
    - Setup: open tests/parks/new.park. Its guest path runs 20 m from the entrance. Add a straight guest path of about 60 m that continues the existing path from its far point, and resolve. Add a guest with addGuest at the middle of the new segment, with its stay already over (stayUntil 0). If the guest chose, it would head home. Step the world a few ticks to see which way along the segment it walks. Copy the world.
    - Change: in one of the two worlds, apply InsertPoint on the segment about 3 m ahead of the guest in the way it walks, and resolve.
    - Expected: step both worlds together until the guest's ground position in the changed world is 2 m past the new point. After every step, the guest's activity is the same in both worlds.

## Medium

- The guest network (navigable-networks, through shared-medium's network type): guests now read the edges and the anchor of each node on their route ahead (edgeEnds and nodeAnchor), to find shaping points and the ways out of a node.
- Route distance: read as before. A guest heading somewhere now also chooses from a place inside an edge, where route distance's entries are the better of the edge's two ends.
- Food offers, guest visits, meals, and hungry footfall: unchanged.
- Guest state gains the choice for the node ahead. warm.park holds guests with choices made, so the park world checks cover it.

There are no new fields or flows. Guests read only the network and route distance, as before (principles 3 and 6).

## Spec changes

src/sim/guests/SPEC.md:

- In Choosing, replace "A guest chooses at every node its place reaches or passes. It also chooses as soon as its target becomes unreachable or stops offering meals, and when its visit comes back." with:

    A guest makes its choice for each node of its route a little before it gets there: when its place comes within TURN_LOOKAHEAD of the node along its route, or when it stands on the node. It chooses once for each node it comes to. It makes no choice for a shaping point, a node with exactly two edges and no anchor, which only bends a path, so the number of points the player draws doesn't change how guests choose. A guest heading to a shop or home makes no choice for the node it is heading to. A guest also chooses as soon as its target becomes unreachable or stops offering meals, and when its visit comes back.

- In Walking, replace "When it wanders, its route runs to the next node, and at each node it picks at random among the ways leading out of it. It never turns back unless there is no other way, as at a path's end." with:

    When it wanders, its route runs through shaping points to the next node it chooses for. When it makes that choice and carries on wandering, it picks at random among the ways leading out of the node, and its route runs on along the way it picked, through shaping points, to the node after. It never picks the way it arrives by unless there is no other way, as at a path's end. If it picks that way, its route stops at the node, so it walks to the end before it turns.

- In Carrying across an edit, replace "Carrying changes only where guests stand, their places, and which way they walk." with:

    A guest keeps the choice it has made for a node ahead if the edit leaves that node where it was. A wandering guest also needs the edge it picked out of the node to be whole and where it was. Otherwise the guest makes that choice again. Carrying changes only where guests stand, their places, which way they walk, and which choices for nodes ahead they keep.

## Files affected

- Modify: src/sim/guests/SPEC.md
- Modify: src/sim/guests/guests.h (TURN_LOOKAHEAD)
- Modify: src/sim/guests/internal/guest.h (the choice for the node ahead, saved)
- Modify: src/sim/guests/internal/route_ahead.h and src/sim/guests/route_ahead.cpp
- Modify: src/sim/guests/guests.cpp
- Modify: tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park (made again)
- Create (test pass): tests/integration/early_turns_test.cpp, and modify tests/integration/CMakeLists.txt to build it
- Modify, untracked: parks/busted-path.park, parks/jank.park, parks/radii.park (their guests carried into the new save)

## Dependencies

- paving-walk: the route ahead, the aim, the place along the route, and carrying by ground position. Merged.
- The network's edgeEnds and nodeAnchor. Met.

## Out of scope

- Choosing for more than one node ahead at once. If two nodes are less than 10 m apart, a wanderer's route ends at the second until it passes the first. Its aim can pull toward the second node's middle for that short stretch.
- Showing a guest's next turn in the inspector. This is already a deepening candidate ("A guest's aim and its sidestep shown in the inspector").

## Open questions

- Whether 10 m reads as natural on main-street.park. Resolved by the user playing it and adjusting TURN_LOOKAHEAD.
