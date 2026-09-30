# Research: candidate-previews

## How should the candidate be kept current?

makeCandidate copies the world, applies the queued commands, and resolves, with no stepping. A Linux perf profile of a 27 ms run on warm.park puts copying and resolving at about 11% each, which gives about 2 ms for one candidate on windows-debug. The app runs at a vsync cap of about 6 ms a frame, with or without the food overlay, so making the candidate every frame while a ghost is accepted fits inside the frame. A frame can hold several ticks, so once a frame is at least as often as after every cycle, and a changed edit shows in the frame it changes.

Made every frame, the candidate is a local value of the frame, like the food overlay's mesh: nothing holds it between frames, so nothing has to notice a new tick, a new edit, or a replaced world to reset it (decision 0027). The ghost mesh, the overlay, the tooltip, and a shop ghost's context all read that one value. The ghost mesh is then also built every frame, and the app's cache of the edit and highlight it was last built from goes away, since the candidate its walkways and marks come from changes with every frame's world. Building a ghost mesh is small beside the candidate.

Rejected: a component caching the candidate by tick and edit. It saves the 2 ms on frames with no tick and an unchanged edit, but it is derived state held apart from the world, with a key that must name everything the candidate depends on, the kind of hand-reset cache decision 0027 retires. It stays available if the cost shows in the app. Rejected: making the candidate only after cycles. A changed edit would wait for the next tick, and a paused park would never update.

Sources: src/sim/SPEC.md, makeCandidate; a Linux perf profile of a run on warm.park; docs/decisions/0027-code-architecture.md.

## Where does a shop ghost meet the guest path?

In the candidate, the shop's front door serves the guest network, and its connector, keyed connectorKey(shop, Face::Front), runs from the door, its first stop and the shop's anchored node, to the connection place on a guest path, its last stop (src/sim/routes/SPEC.md, Connectors). The node of that last stop holds, among its stopPlaces, the connection's place on the path carrier, the first of them whose carrier is a guest path. A connection within tolerance of a junction lands on the junction's node, whose stopPlaces hold every path meeting there, and taking the lowest key's is as good as any, since a node's place resolves to the same node on each.

For an AddBox or a MoveBox, the paths are the committed world's, so that place, a carrier key and a distance along it, lies on the same line in the committed world. Hungry footfall is sampled there in the committed world, since a candidate is never stepped and holds footfall only where the last step published it. Inside a stretch, footfall's value is the stretch's, and at a node the mean of those meeting it (src/sim/guests/SPEC.md, Hungry footfall).

The new shop of an AddBox has a key from the world's key counter, so the candidate holds exactly one box that the world does not. A MoveBox keeps its box's key.

The supply route is nearestDepot of the shop in the candidate: operations' own query for the depot with the least supply route length, the least backstage route distance from the shop's backstage anchor to the depot. A shop with none is starved, as its ghost mark shows.

Rejected: nearestGuestPathPlace of the front door. It finds the place by straight distance over every guest path, which agrees with the connection in the park's rules today, but would show a place the candidate's own connector does not reach when a door lies beyond CONNECTION_REACH. Rejected: sampling footfall in the candidate. It is empty at stretches the edit splits or creates.

Sources: src/sim/routes/SPEC.md, Connectors; src/sim/guests/SPEC.md, Hungry footfall; src/sim/operations/operations.h, nearestDepot; src/sim/park/SPEC.md, AddBox and MoveBox.

## Can the shop context share the food tooltip?

Dear ImGui 1.92 appends a second BeginTooltip in one frame to the same tooltip window unless the second asks to override the first. So the shop context can be its own component calling BeginTooltip, and while the food overlay's tooltip also shows, the two read as one box at the cursor, the context below the food.

Sources: .cpm-cache/imgui, imgui.cpp, BeginTooltipEx and ImGuiTooltipFlags_OverridePrevious.
