# Feature: Paving Walk

## Summary

Guests stand on the paving and walk across it. Each guest holds a ground position of its own on the guest paving, and saves hold it. It keeps inside the outline of the paving it is on, with its box clear of that outline. The network still chooses its route. Each tick it walks straight toward its aim: the farthest point of its route ahead that a straight line reaches with its box on the paving. Its place on the network is where it has got to along that route, so choices, route distance, visits, meals, footfall, and the overlay work as before. A guest reaches the end of its route when it can come no closer, so it stops in front of a shop's door instead of inside it. After an edit, a guest keeps its ground position while its box fits there, and otherwise moves to the nearest point where it does. The parks that hold guests are made again. Guests still walk at one speed and keep no side of the path. Later features add early turns, their own pace and side, and stepping around one another.

## Acceptance criteria

1. Each guest holds a ground position on the guest paving, and saves hold it. Every tick, each guest stands on its own joined guest paving, at least GUEST_CLEARANCE, 0.45 m, from each visible edge line of that paving and from the door end of each of its walkways. The one exception is a guest that stands nearer than that, as one that has just arrived through the entrance's door: it never comes nearer. Where guest paths overlap without sharing a point, a guest stays on the path it is on. Confirmed by a check: the scratch program build/paving-walk/stays.exe steps every park in tests/parks and parks/ for 3600 ticks, checks every guest after every tick, and prints `0 guests off the paving` for each park. Also confirmed by play: on parks/busted-path.park, guests walk across its overlapping and stacked paving without leaving it.
2. A guest walks straight toward its aim: the farthest point of its route ahead, looking at most AIM_LOOKAHEAD, 30 m, along it, that a straight line reaches with its box on the paving. Its aim never moves back along its route until the route changes. On main-street.park, a guest heading for a shop across the 10 m street walks a straight line toward its door, and a guest heading round a bend cuts the inside of the bend without leaving the paving. Confirmed by play.
3. A guest's place on the network is the point of its route, between its place and its aim, nearest where it stands. Its place moves only forward along its route, and the guest chooses at every node its place reaches or passes. Choices, route distance, food offers, visits, meals, and hungry footfall read and feed that place as before. Confirmed by ctest, where food_loop_test passes, and by play with the food overlay on main-street.park, where footfall shows along the paths guests use.
4. A guest reaches the end of its route when it stands on it, or when it has walked as far along the last stretch of its route as its box allows. A guest heading to a shop stops in front of its door and sends its visit there. A guest heading home leaves at the entrance's door. A wandering guest turns back at a path's free end. Confirmed by play on main-street.park, and by food_loop_test.
5. After an edit, a guest keeps its ground position if its box still fits on guest paving there, and its place follows. If not, it moves to the nearest point where its box fits on the guest paving. If no guest paving is left, it leaves the park. No guest moves when the player widens, splits, or merges a segment under it. Confirmed by play on main-street.park once guests fill it: widen the street, add a path across it, delete a point between two segments, narrow the street, and delete a side path with guests on it. In a candidate, guests stand where committing its edit would put them, which the park world checks confirm.
6. tests/parks/warm.park, cut.park, and stress/full.park are made again under the new rules, and the park world checks, food_loop_test, and the rest of ctest pass. Confirmed by ctest.
7. Walking a guest between choices allocates nothing in the tick, and its work depends on the paving near the guest, its route ahead, and the node it stands at, not on the size of the park (decision 0031). A wandering guest's pick at a node reads only that node's edges. Choosing still allocates when it samples every source, which scalable-runtime's deepening candidate "No allocation in the tick path" holds. Confirmed by review. scripts/runtime-report.sh, run before and after the feature, reports the change in the guests' cost per tick on full.park against the 33 ms budget. The report is read, not gated.
8. src/sim/guests/SPEC.md describes guests walking on the paving. The milestone's criterion 3, the capability's brief, and the slice say a guest's place is where it has got to along its route. Confirmed by review.

## Tests

None. Walking is checked by playing and by the scratch check of criterion 1. The new guest state goes into tests/parks/warm.park, cut.park, and stress/full.park, where the park world checks cover it, and the derived ground guests walk on is in every resolved park with a guest path (docs/testing.md).

## Medium

The feature adds no field or flow.

- Park intent's derived shape (decision 0025): guests read sim/park's parkSurface with the guest walkways, walkwayPiece, and parkSegments, and routes' keyedWalkways. From them, a resolver in the guests module derives the ground guests walk on: the outline of each group of joined guest paving, with the pieces under it. It is derived data, never saved (principle 1).
- Networks: guests read the guest network, its places, ground points, and nearest places, and carryOver with the previous network, as before.
- Route distance: guests follow route distance's next steps from node to node to know their route ahead. Choices read it as before.
- Food offers, guest visits, meals, and hungry footfall: unchanged, read and fed at the guest's place.
- Inspection records: a guest's record gives the ground position it stands on. Drawing and picking read it as before.
- New state: each guest's ground position and aim. tests/parks/warm.park, cut.park, and stress/full.park hold it.

Guests read nothing of one another in this feature. Route distance stays the network's (principle 4, decision 0038).

## Spec changes

src/sim/guests/SPEC.md:

- In Boundary, in the list of what a guest reads, after "parkEntrances (decision 0025)", add a bullet: "the guest paving, from parkSurface with the guest walkways, walkwayPiece, parkSegments, and keyedWalkways (decision 0025)." Replace the first paragraph of Boundary with: "addGuests registers the guest state, the ground guests walk on and the resolver that derives it, the systems that step guests and their footfall, and the finishers that carry both across a resolution. addPark calls it after addOperations, so guests step after shops and depots."
- In Arriving and leaving, replace "It also leaves when the guest network has nowhere left for it to stand." with "It also leaves when no guest paving is left for it to stand on." After "If the place is not on the guest network, addGuest refuses it.", add: "A new guest stands at the place's ground position."
- In Choosing, replace "A guest chooses at every node it leaves." with "A guest chooses at every node its place reaches or passes."
- Replace the Walking section's paragraph with these four:

      A guest stands at a ground position of its own on the guest paving, and saves hold it. It walks inside the outline of the paving it is on: the visible edge lines of its own joined paving (sim/park/SPEC.md), and the end of each walkway at its door. Where paths that aren't joined overlap, the other path's edge lines don't bound it, so it stays on its own path.

      The network chooses a guest's route. When a guest heads to a shop or home, its route follows route distance there, node after node. When it wanders, its route runs to the next node, and at each node it picks at random among the ways leading out of it. It never turns back unless there is no other way, as at a path's end.

      A guest walks straight toward its aim at a fixed speed. Its aim is the farthest point of its route, looking at most AIM_LOOKAHEAD along it, that a straight line reaches with the guest's box on the paving. The aim never moves back along the route until the route changes. The box is on the paving while the guest keeps at least GUEST_CLEARANCE, half the diagonal of its box, from every line that bounds it. If the guest stands nearer a line than that, as at the entrance's door it arrives through, it never comes nearer that line and never crosses it. So a guest heading for a shop across a wide street walks straight to its door, and at a bend it cuts the inside without leaving the paving. Walking anywhere on its route's paving travels that route, so route distance still measures how far it goes (principle 4, decision 0038). The work of walking a guest depends on the paving near it and its route ahead, not on the size of the park.

      A guest's place on the network is the point of its route, between its place and its aim, nearest where it stands. Its place moves only forward along its route, so the guest passes each node of its route in order. A guest reaches the end of its route when it stands on it, or when it has walked as far along the last stretch of its route as its box allows, as in front of a door or at a path's free end. Its speed doesn't depend on how the network is cut into edges.

- Replace the Carrying across an edit section's paragraph with:

      When the player edits the park, a guest keeps its ground position if its box still fits on guest paving there. It stays on the paving its place is carried to (sim/medium/SPEC.md) if that paving lies under it, so where paths that aren't joined overlap, it stays on its own path. Its place is then the carried place. If the carried place isn't on the paving under it, its place is the nearest place on the carrier of the segment, point, or walkway it stands on. If its box no longer fits where it stands, the guest moves to the nearest point where it fits on the guest paving, and takes its place from the paving there. If no guest paving is left, it leaves the park when it next steps. If the guest is carried onto a carrier that runs the other way there, it turns, so it keeps walking the same way over the ground. Carrying changes only where guests stand, their places, and which way they walk. If a guest is waiting, it keeps waiting. If it is heading to a shop, it keeps its target until its next walk. In a candidate, guests stand where committing the candidate's edit would put them (principle 8).

- In Inspection, replace "The record holds its activity, place and ground position," with "The record holds its activity, its place, the ground position it stands on,".

Public interface, src/sim/guests/guests.h, after WALK_SPEED:

    // Meters a guest keeps from every line bounding the paving it walks on: half the diagonal of its
    // 0.6 m box, so the box stays on the paving at any angle.
    inline constexpr double GUEST_CLEARANCE = 0.45;
    // Meters of route ahead of its place that a guest looks along for its aim.
    inline constexpr double AIM_LOOKAHEAD = 30.0;

GuestRecord's comment changes "where that is on the ground (none while its place does not resolve)" to "the ground position it stands on". Its Position always holds a value.

## Files affected

- Modify: src/sim/guests/SPEC.md
- Modify: src/sim/guests/guests.h
- Modify: src/sim/guests/guests.cpp
- Modify: src/sim/guests/internal/guest.h
- Create: src/sim/guests/internal/ground.h
- Create: src/sim/guests/ground.cpp
- Create: src/sim/guests/internal/route_ahead.h
- Create: src/sim/guests/route_ahead.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park (made again)
- Modify: plans/believable-guests/spread-guests/MILESTONE.md, plans/believable-guests/CAPABILITY.md, plans/slices/good-paths/SLICE.md, plans/slices/good-paths/RESEARCH.md
- Modify, untracked: parks/busted-path.park, parks/jank.park, parks/radii.park, whose guests are carried into the new save once, with a backup kept in build/
- Create, never committed: build/paving-walk/build.sh, convert.cpp, and stays.cpp (build/ is ignored by git)

## Dependencies

walkway-pieces: walkwayPiece and keyedWalkways, and the check that the outline closes in every park. shared-medium's carryOver. No other sibling feature.

## Out of scope

- Choosing about 10 m before a node, wanderers seeing past the next node, and walking through shaping points. early-turns does these, so until then a wandering guest aims at each node it reaches.
- Each guest's own speed and side of the path. own-pace-and-side does these.
- Guest presence and stepping around other guests. sidestep does these.
- warm-main-street.park, the stress parks' new baseline, and the slice's capture. main-street-guests does these.
- Walking a stranded guest off deleted paving instead of moving it. The capability's deepening candidates hold it.

## Open questions

- Whether looking 30 m ahead, and the tests it takes each tick, suit full.park's 2,000 guests within the 33 ms budget. Resolved by criterion 7's report, and settled in main-street-guests' baseline.
