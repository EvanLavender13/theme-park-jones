# Research: candidate-previews

## How should the candidate be kept current?

Timed on warm.park on windows-debug, 40 runs each: making the candidate, a copy and a resolution, takes 1.73 ms; the ghost mesh from it 0.46 ms; the food overlay mesh 2.62 ms; and one tick 0.66 ms. The app runs at a vsync cap of about 6 ms a frame. Making the candidate and the ghost every frame, as first built, cost Evan about 60 fps while a shop ghost was shown, and the overlay, rebuilt every frame since food-overlay, was already the largest cost.

The candidate, the ghost, and the overlay change only when the world ticks, the edit changes, the highlight changes, or the overlay is shown or hidden, and a world changes only as it ticks. So a preview kept with the tick and edit it was made from, and meshes rebuilt only when it is made again or the highlight or checkbox changes, give the same pictures as rebuilding every frame, while a frame between ticks with a still cursor rebuilds nothing. The kept preview is emptied when the app replaces the world, as the park and guest meshes' records are. This is a cache decision 0027 would rather avoid, and it is taken because the frame rate needs it: keepPreview's rule, headless and tested, is the one place that decides when it is stale.

A moving ghost changes the edit every frame, so each new pose still costs a candidate, as the ghost cache before this feature did. Making that cheaper needs incremental resolution, re-deriving only what an edit touches, or making candidates off the frame's thread, which world-as-value's copyable worlds allow.

Rejected: making the candidate and meshes every frame. It costs about 3 ms a frame in debug while a ghost is shown. Rejected: making the candidate only after cycles. A changed edit would wait for the next tick, and a paused park would never update.

Sources: src/sim/SPEC.md, makeCandidate; timings from a temporary Catch2 case in tpj_render_tests on windows-debug; docs/decisions/0027-code-architecture.md.

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
