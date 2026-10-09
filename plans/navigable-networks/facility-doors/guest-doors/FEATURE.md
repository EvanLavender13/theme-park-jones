# Feature: Guest Doors

## Summary

A bench's and a toilet block's front door serves the guest network, as a shop's front door does. Within reach of guest paving, the door gets a connector and a walkway, its door node is anchored to the box, and the box becomes a source of guest route distance. Each door knows the width of its face, and a connector and its walkway are no wider than that. A bench's walkway is 2 m wide, and every other walkway keeps its width. Nothing uses the new sources yet. served-needs makes them publish offers.

## Acceptance criteria

1. A bench's and a toilet block's front door serves the guest network, and neither has a door serving the backstage network. Within CONNECTION_REACH of guest paving, each gets a connector and a walkway joined to that segment, and its door node is anchored to the box. (capture: `build/windows-debug/ThemeParkJones.exe --park tests/parks/facilities.park --graph --capture build/guest-doors/graph.bmp` shows connectors from the bench, the toilet block, and both shops to the guest path, each ending at an anchored node)
2. Each box is then a source of guest route distance. (check: a scratch program loads tests/parks/facilities.park, resolves it, and prints the source of each guest route distance entry sampled at the middle of segment 4. Expected: `1 5 6 7 8`, the entrance, the bench, the toilet block, and the two shops)
3. A connector and its walkway are as wide as their kind's path, or as the door's face if that is narrower. A bench's walkway is 2 m wide, in the park and in the ghost of placing or moving a bench. Every other walkway keeps its width. (capture of facilities.park without --graph, and play: drag a bench ghost beside a path and see its walkway strip as wide as the bench)
4. Every park without a bench or toilet block simulates exactly as before. (`tpj_scenarios --compare` on windows-debug output from before and after, over every park in tests/parks but facilities.park)
5. The park world checks pass over facilities.park, which now derives the new connectors. (ctest)
6. In the windows-release build, the user places benches and toilet blocks along guest paths with the graph view on. Each one the frontage snap places gets a connector. Drawing a guest path from a bench's or toilet block's door snaps to it and connects it. Moving one more than 3 m from the paving disconnects it, and undo and redo restore its connector. (play, by the user)

## Tests

None. tests/parks/facilities.park already holds a bench and a toilet block, so the park world checks cover their connectors.

## Medium

- Networks: routes adds bench and toilet block front doors to the guest network's connectors and anchors. plausible-operations' served-needs will find a facility's anchored node there.
- Route distance: benches and toilet blocks become sources on the guest network. Nobody reads their entries yet: guests take only sources with a food offer, and operations finds shops and depots by box kind.
- Park intent (decision 0025): routes reads the boxes through parkBoxes and boxSize, as before.

The feature registers no state.

## Spec changes

src/sim/routes/SPEC.md, Networks, the connector sentence becomes:

> It also has a carrier for each connector, as wide as its kind's path (pathWidth, sim/park/SPEC.md), or as its door's face if that is narrower.

src/sim/routes/SPEC.md, Connectors, the door list becomes:

> - An entrance's front door serves the guest network.
> - A shop's front door serves the guest network, and its back door serves the backstage network.
> - A depot's front door serves the backstage network.
> - A bench's and a toilet block's front doors serve the guest network.

and the ParkDoor sentence becomes:

> A ParkDoor has its entity, its face, its position, the unit direction out of its face, and the width of its face.

plans/navigable-networks/CAPABILITY.md, Foundation criteria, the list of connecting faces gains:

> - a bench's front and a toilet block's front connect to a guest path;

Public interface, in src/sim/routes/networks.h:

```cpp
struct ParkDoor {
  EntityKey Entity = NULL_KEY;
  Face Side = Face::Front;
  ParkPoint At;
  ParkPoint Out;
  double Width = 0.0;
  ...
};
```

## Files affected

- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/park/intent.h (pathWidth's comment)
- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: plans/navigable-networks/CAPABILITY.md

## Dependencies

- effortless-building/facilities-and-hours: bench and toilet block boxes, and tests/parks/facilities.park. Complete.

## Out of scope

- Offers at benches and toilet blocks, and guests using them: plausible-operations' served-needs and believable-guests' a-day-out.
- A walkway that leaves a bench at an angle stays as wide along the face as any angled walkway, so it can overhang the bench. Only freehand placement beside a path's end makes that happen.

## Open questions

None.
