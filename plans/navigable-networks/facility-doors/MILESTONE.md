# Milestone: Facility Doors

Slice: day-at-the-park

Status: complete

## Summary

facility-doors gives benches and toilet blocks front doors that serve the guest network. A bench or toilet block placed beside a guest path gets a connector and a walkway, as a shop's front door does. Its door node is anchored to it, so it becomes a source of guest route distance. A bench's walkway is no wider than the bench. Nothing uses the new sources yet: served-needs makes benches and toilet blocks publish offers, and guests weigh those offers in a-day-out. This is member 3 of day-at-the-park. facilities-and-hours placed the boxes, and served-needs needs them on the guest network before it can serve guests at them.

## Acceptance criteria

1. A bench's and a toilet block's front door serves the guest network, and neither has a door serving the backstage network. Each door connects as a shop's front door does: within CONNECTION_REACH of guest paving, it gets a connector and a walkway joined to that segment, and its door node is anchored to the box. Each box is then a source of guest route distance. The path tool snaps to their doors as to a shop's. (capture: tests/parks/facilities.park with --graph shows a connector from the bench, the toilet block, and both shops to the guest path, each ending at an anchored node)
2. A connector and its walkway are as wide as their kind's path, or as the door's face if that is narrower. A bench's walkway is 2 m wide, and flush with the bench's ends when it runs straight out of the bench's front. Every other door's walkway keeps its width. (capture: facilities.park, and the routes spec read beside the code)
3. Every park without a bench or toilet block simulates exactly as before. tests/parks/facilities.park is the only park holding them, and it has no guests. (`tpj_scenarios --compare` on windows-debug output from before and after, over every park in tests/parks but facilities.park)
4. The park world checks pass over every park, so a bench's and a toilet block's connectors derive from intent alone, and facilities.park derives them equal after a load. (ctest)
5. src/sim/routes/SPEC.md lists the new doors and the walkway width. This capability's foundation criteria name benches and toilet blocks among the boxes that connect. (check: read each beside the code)
6. In the windows-release build, the user places benches and toilet blocks along guest paths with the graph view on. Each one the frontage snap places gets a connector. Drawing a guest path from a bench's or toilet block's door snaps to it and connects it. Moving one more than 3 m from the paving disconnects it, and undo and redo restore its connector. (play, by the user)

## Medium

The milestone adds no fields or flows. It changes which entities the networks anchor.

- Networks: guest-doors gives bench and toilet block front doors connectors and anchored nodes. routes produces them in shared-medium's network type. plausible-operations' served-needs finds a facility's anchored node to publish its offers there.
- Route distance: unchanged in kind. Benches and toilet blocks become sources on the guest network. believable-guests reads their entries once they publish offers, and legible-simulation reads them for need overlays.
- Park intent (decision 0025): routes reads the boxes through parkBoxes and footprintOf, as it does for shops. Connections are derived and never saved.

## Dependencies

- effortless-building/facilities-and-hours: bench and toilet block boxes, placed by the frontage snap. Complete.

## Core feature

`guest-doors` is the only feature. It puts the new facilities on the guest network, which is the whole of this member's share of the slice, and the graph view shows the result at once.

## Features

1. `guest-doors`: bench and toilet block front doors serve the guest network, and connectors and walkways are no wider than their door's face. The feature updates the routes spec's Connectors section and this capability's foundation criteria. Depends on: none.

The feature adds no test (docs/testing.md). facilities.park already holds a bench and a toilet block, so the park world checks cover their connectors.

## Deepening candidates

Unordered pool this milestone draws later features from.

## Open questions

None.

## Research notes

- A walkway as wide as the path would overhang a 2 m bench. Capping it at the door's face width changes no existing walkway.
- No park with guests holds a bench or toilet block, so adding doors changes no existing simulation.

Depth is in RESEARCH.md.
