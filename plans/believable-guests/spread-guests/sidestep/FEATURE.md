# Feature: Sidestep

## Summary

Guests step aside from the guests they would walk into. Each walking guest publishes where it stands and how it moved in guest presence, a ground field, at the end of each tick. Each tick, it samples the entries within 3 m of it, which are where the others stood at the end of the last tick. For each guest ahead of it, it predicts their nearest approach if both kept walking as they were. If they would come too close soon, it steps aside, away from where the other would be, just fast enough to pass at a comfortable distance. A head-on meeting goes to the right, the side the guest's lane keeps. The step aside is added to its step toward its aim only where its box stays on the paving and it can still walk straight to its aim. So two guests walking toward each other on a path step apart before they meet, and a faster guest steps out around a slower one ahead of it. A guest waiting at a shop and a guest leaving the park drop their entries. warm.park and cut.park are made again, so the park world checks cover guest presence.

## Acceptance criteria

1. A walking guest steps aside from each guest ahead of it that it would come within SIDESTEP_DISTANCE of within SIDESTEP_HORIZON, if both kept walking as they were at the end of the last tick. It steps away from where that guest would be, and right when it would be dead ahead. Guests walking side by side at one pace, and guests behind it, don't make it step aside. On main-street.park, guests walking toward each other step apart before they meet and pass without their boxes overlapping, and a faster guest steps out to pass a slower one. Guests step aside only from guests near them, which confirms ground-fields' deferred criterion 2. The closeness check reports that the ticks two walking guests spend closer than 0.6 m, a box's width, fall to less than half the unchanged tree's on main-street.park and parks/busted-path.park. (play, and the closeness check)
2. A guest senses other guests only through guest presence, and reads only the entries readable at the start of the tick, so the order guests step in doesn't change where any guest walks (principle 10). (review)
3. Each guest sets its entry at the end of each tick it walks, with where it stands and its velocity over that tick. A guest drops its entry when it starts waiting at a shop, and when it leaves the park. No guest steps aside from a guest that has left, which confirms ground-fields' deferred criterion 4. warm.park and cut.park are made again and hold guest presence entries, and the park world checks cover them, which confirms ground-fields' deferred criterion 5. (play at an entrance, the presence check, and ctest)
4. A sidestep never takes a guest's box off the paving and never leaves it without a straight line to its aim. The stays check reports 0 guests off the paving in every park in tests/parks and parks/. The lanes check reports 0 stalled guests in every park, and on main-street.park more than two thirds of walking guests clear of the middle of the 10 m street are right of it, with a mean offset between 1 and 3 m. paving-walk's door check reports meals and no guest stuck heading to the shop. (the stays, lanes, and door checks)
5. The runtime report on full.park shows the tick still inside the 33 ms budget. A guest's sample and sidestep depend on the guests near it, not on the number of guests in the park or the size of the park (decision 0031). (scripts/runtime-report.sh)
6. src/sim/guests/SPEC.md describes guest presence and stepping aside, and believable-guests' brief lists the nearest approach among the straight-line measures that measure no travel. (review)

## Tests

None. Stepping aside is gameplay, checked by playing and by the scratch checks (docs/testing.md). Guest presence entries are state, which the park world checks cover once warm.park holds them.

## Medium

- Guest presence (shared-medium's ground fields): a new ground field, guest-presence, which the guests module registers with addGroundField. Each walking guest sets its entry with setGroundEntry and samples others' with sampleGroundField. A guest that starts waiting or leaves drops its entry with dropGroundEntry. Its entry is the guest's velocity over its last tick walked. Nothing outside the guests module reads it.
- The guest paving (GuestGround, derived in the guests module): a sidestep is kept only where clearLine says the line to the stepped point, and from there to the aim, stays on the paving.
- Route distance, food offers, guest visits, meals, and hungry footfall: unchanged. A guest's place still follows from where it stands (decision 0038).

There are no new flows. warm.park and cut.park hold guest presence, so the park world checks cover it.

## Spec changes

src/sim/guests/SPEC.md:

- In Boundary, replace "addGuests registers the guest state, the ground guests walk on and the resolver that derives it, the systems that step guests and their footfall, and the finishers that carry both across a resolution." with:

    addGuests registers the guest state, guest presence, the ground guests walk on and the resolver that derives it, the systems that step guests and their footfall, and the finishers that carry both across a resolution.

- In Boundary, add to the list after "the guest paving, from parkSurface …":

    - guest presence, which only guests publish and sample.

- In Arriving and leaving, replace "It also leaves when no guest paving is left for it to stand on." with:

    It also leaves when no guest paving is left for it to stand on. A guest that leaves drops its entry in guest presence.

- In Walking, add a paragraph after the one that begins "Each guest walks at its own speed":

    Guests step aside from one another. At the end of each tick it walks, a guest sets its entry in guest presence, the ground field guest-presence: where it stands, and its velocity over that tick. A guest that starts waiting at a shop drops its entry, and sets it again when it next walks. A guest senses other guests only through guest presence, from the entries within SIDESTEP_RADIUS of where it stands. Those say where each stood and how it was moving at the end of the last tick, so the order guests step in never changes where they walk (principle 10). For each guest ahead of it, a guest predicts where they would come nearest if both kept walking as they were. If that is within SIDESTEP_HORIZON and nearer than SIDESTEP_DISTANCE, it steps aside, away from where the other would be, fast enough to pass at SIDESTEP_DISTANCE, and right if the other would be dead ahead. Its steps aside from each such guest add up, to at most SIDESTEP_SPEED either way. Guests walking side by side at one pace don't step aside, and a guest doesn't step aside from those behind it. A guest steps aside only where its box stays on the paving and it can still walk straight to its aim from where it steps to. Otherwise it walks on toward its aim.

plans/believable-guests/CAPABILITY.md, in the principle 4 note, replace "guest presence's sampling radius," with "guest presence's sampling radius, the nearest approach between two guests,".

## Files affected

- Create: src/sim/guests/internal/presence.h (the guest presence field, its entry, and the step aside)
- Create: src/sim/guests/presence.cpp
- Modify: src/sim/CMakeLists.txt (adds guests/presence.cpp)
- Modify: src/sim/guests/guests.h (SIDESTEP_RADIUS, SIDESTEP_HORIZON, SIDESTEP_DISTANCE, SIDESTEP_SPEED)
- Modify: src/sim/guests/guests.cpp (registering guest presence, sampling it, stepping aside in the walk, setting and dropping entries)
- Modify: src/sim/guests/SPEC.md
- Modify: plans/believable-guests/CAPABILITY.md (the principle 4 note)
- Modify: tests/parks/fed.park, tests/parks/warm.park, tests/parks/cut.park (made again; fed.park only gains the field's empty entries)
- Modify, if the park world checks need it: src/scenarios/slice_parks.h (WARM_TICKS)

## Dependencies

- shared-medium's ground-fields: addGroundField, setGroundEntry, dropGroundEntry, and sampleGroundField. Met.
- paving-walk: walking toward the aim and GuestGround's clearLine. Merged.
- own-pace-and-side: each guest's speed and side. Merged.

## Out of scope

- Slowing or stopping for a guest ahead instead of stepping aside. It is the milestone's deepening candidate "Guests pausing or slowing where the paving is crowded".
- Stepping aside from waiting guests. They all stand at the door's point, which the guests walking to that door must reach.
- Showing a guest's sidestep in the inspector. It is the milestone's deepening candidate "A guest's aim and its sidestep shown in the inspector".
- Making the stress parks and warm-main-street.park again, and the new runtime baseline. main-street-guests does them.

## Open questions

- Whether SIDESTEP_DISTANCE 0.9 m, SIDESTEP_HORIZON 3 s, and SIDESTEP_SPEED 0.6 m/s read as natural on main-street.park. Resolved by the user playing it and adjusting them.
