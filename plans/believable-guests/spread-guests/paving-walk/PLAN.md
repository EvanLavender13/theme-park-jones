# Implementation Plan: Paving Walk

## Goal

Guests hold a ground position on the guest paving and walk straight across it toward the farthest point of their route they can reach, with their boxes kept on the paving, their places following along the route, and their ground positions kept across edits.

## Approach

A resolver in the guests module derives the ground guests walk on from parkSurface with the guest walkways: its walls (the visible guest edge lines and each walkway's door end), each with the group of joined paving it bounds, its pieces with their groups and carriers, and a lookup by 4 m cell. Each tick, a guest traces its route ahead from its place, up to AIM_LOOKAHEAD, moves its aim forward along that route as far as a straight line from where it stands stays clear, walks toward the aim, and takes as its place the point of the route nearest where it now stands. The carry finisher keeps each guest's ground position where its box fits, and otherwise moves it to the nearest point where it fits, as this feature's RESEARCH.md describes.

## Placement

Decision 0027 places each behavior this feature adds:

- The ground guests walk on, its resolver, and its queries (clear lines, fitting, the piece under a point, the nearest fitting point): sim/guests, a new private component in src/sim/guests/internal/ground.h with src/sim/guests/ground.cpp. Only guests walk on it, so it is private to the module (decision 0016). It is derived from park intent's shape, which sim/park and routes publish.
- A guest's route ahead, its aim, and its place along the route: sim/guests, a new private component in src/sim/guests/internal/route_ahead.h with src/sim/guests/route_ahead.cpp. It is guest movement, which the guests module owns, and it reads only the network and route distance.
- Walking, choosing as the place passes nodes, reaching a route's end, and carrying across an edit: sim/guests, src/sim/guests/guests.cpp, which already steps and carries guests.
- The guest's ground position and aim: sim/guests, the Guest state in src/sim/guests/internal/guest.h.
- The tuning constants: sim/guests, src/sim/guests/guests.h, beside WALK_SPEED.
- Remaking the parks: tpj_scenarios --slice-parks and tpj_bench --full-park, run as they are. Neither changes.
- The stays-on-paving check: a scratch program in build/paving-walk/, never committed. docs/testing.md allows no permanent test of one module's internals.

## Tasks

### Task 1: Record the runtime before the change

Files: none.

Step 1: On the unchanged tree, make a runtime report.

Run: `scripts/runtime-report.sh --launches 3 build/paving-walk-before.report`
Expected: the script builds both Windows presets, runs, and prints its report against the budget, with full.park's stages listed for windows-release. Keep the report for Task 14.

### Task 1b: Carry the guests in the user's parks into the new save

Files:
- Create, never committed: `build/paving-walk/build.sh`
- Create, never committed: `build/paving-walk/convert.cpp`
- Modify, untracked: `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

The user's parks in parks/ are untracked, and three of them hold guests in today's save. After Task 5, a guest's line needs its ground position and aim, and loading a line without them fails. The user approves this conversion by approving this plan. Do it now, while build/windows-debug's library still loads today's saves.

Step 1: Copy build/paving-holes/build.sh to build/paving-walk/build.sh, and copy parks/*.park to build/paving-walk/parks-before/ as a backup.

Step 2: Write convert.cpp against the public headers sim/park_schema.h, sim/save.h, sim/world.h, and sim/guests/guests.h. For each path on its command line: load and resolve the park as build/paving-holes/closure.cpp does, and read the file's text. In its `[guest]` section, each line starts with a guest's key and then its `at={...}` field. After that field's closing brace, insert ` x=<X> z=<Z> aim=<the at field's braces, copied>`, where X and Z are guestRecord(world, key)'s Position, or 0 when it has none, written with std::to_chars as sim/field_text.h writes doubles. Write the text back to the path in binary mode, and print `<path>: <count> guests`.

Step 3: Build it against today's library and run it on the parks that hold guests.

Run: `build/paving-walk/build.sh convert && build/paving-walk/convert.exe $(grep -l '^\[guest\]' parks/*.park)`
Expected: one line for each of parks/busted-path.park, jank.park, and radii.park, each with its guest count. Task 12 loads them under the new rules, which shows the conversion worked.

### Task 2: Describe walking on the paving in the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: Make each change listed under "src/sim/guests/SPEC.md:" in FEATURE.md's Spec changes, in order: Boundary's first paragraph and its new bullet, the two changes to Arriving and leaving, the sentence in Choosing, the four paragraphs replacing Walking's paragraph, the paragraph replacing Carrying across an edit's paragraph, and the change in Inspection. Write the indented paragraphs without their indent.

### Task 3: Say where a guest's place comes from in the plans above

Files:
- Modify: `plans/believable-guests/spread-guests/MILESTONE.md`
- Modify: `plans/believable-guests/CAPABILITY.md`
- Modify: `plans/slices/good-paths/SLICE.md`
- Modify: `plans/slices/good-paths/RESEARCH.md`

Step 1: In MILESTONE.md's criterion 3, replace "A guest's place on the network is the nearest place on the carrier of the segment, point, or walkway it stands on." with "A guest's place on the network is where it has got to along its route: the point of its route, between its place and its aim, nearest where it stands. After an edit, it is the nearest place on the carrier of the segment, point, or walkway it stands on." In its Medium, replace "and finds its place on the carrier of the piece it stands on" with "and finds its place along that route".

Step 2: In CAPABILITY.md's Medium, replace "Each guest's place on the network is the nearest place on the carrier of the paving it stands on." with "Each guest's place on the network is where it has got to along its route, the point of its route nearest where it stands."

Step 3: In SLICE.md's Walking on the paving, replace "Its place on the network is the nearest place on the carrier of the segment, point, or walkway it stands on, so every field, flow, and route distance it reads or feeds works as before." with "Its place on the network is where it has got to along its route, the point of its route nearest where it stands, so every field, flow, and route distance it reads or feeds works as before. After an edit, its place is the nearest place on the carrier of the segment, point, or walkway it stands on."

Step 4: In the slice's RESEARCH.md, replace "A guest's network place follows from where it stands. Each piece names its owner, a segment or a point, and a segment's carrier has the segment's key, so the place is the nearest place on the owner's carrier, or on the carriers at the point. On a walkway, it is the nearest place on that door's connector." with "A guest's network place follows from where it stands: it is the point of its route nearest where it stands, so it moves along the route as the guest does (plans/believable-guests/spread-guests/paving-walk/RESEARCH.md)."

### Task 4: Add the tuning constants

Files:
- Modify: `src/sim/guests/guests.h`

Step 1: After WALK_SPEED, add GUEST_CLEARANCE and AIM_LOOKAHEAD exactly as FEATURE.md's Public interface gives them, with their comments.

Step 2: In GuestRecord's comment, replace "where that is on the ground (none while its place does not resolve)" with "the ground position it stands on".

### Task 5: Give guests a ground position and an aim

Files:
- Modify: `src/sim/guests/internal/guest.h`
- Modify: `src/sim/guests/guests.cpp` (addGuest, guestRecord)

Step 1: In Guest, after At, add `GroundPoint Ground;` and `Place Aim;`. The struct comment says, after "where it stands on the guest network": "where it stands on the ground, the place on its route it walks toward". In visitFields, after "at", visit `"x"` as `guest.Ground.X`, `"z"` as `guest.Ground.Z`, and `"aim"` as `guest.Aim`, as GroundEntry does in src/sim/medium/ground_field.h. GroundPoint has no visitFields of its own; don't add one.

Step 2: In addGuest, after the network check, set `guest.Ground` to the place's groundPoint on the guest network and `guest.Aim` to the place.

Step 3: In guestRecord, set `record.Position = state->Ground` instead of the place's groundPoint.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 6: The ground guests walk on

Files:
- Create: `src/sim/guests/internal/ground.h`
- Create: `src/sim/guests/ground.cpp`
- Modify: `src/sim/CMakeLists.txt` (add `guests/ground.cpp` after `guests/footfall.cpp`)
- Modify: `src/sim/guests/guests.cpp` (addGuests)

Step 1: Write internal/ground.h, guard TPJ_SIM_GUESTS_INTERNAL_GROUND_H. It declares:

    // A line a guest may not cross, with the paving on its left seen from above: a run of a visible
    // guest edge line, or a walkway's end at its door. Group is the group of joined paving it bounds.
    struct GroundWall { ParkPoint From; ParkPoint To; uint32_t Group = 0; bool DoorEnd = false; };

    // A piece of guest paving, the group of joined paving it belongs to, and the carrier a guest
    // standing on it takes its place from after an edit.
    struct GroundPiece { SurfacePiece Piece; uint32_t Group = 0; EntityKey Carrier = NULL_KEY; };

    // A carrier of the guest network and the group of joined paving it runs on.
    struct CarrierGroup { EntityKey Carrier = NULL_KEY; uint32_t Group = 0; };

    // A point and the group of joined paving it is on.
    struct GroundSpot { GroundPoint At; uint32_t Group = 0; };

    // The guest paving as guests walk on it. Derived: built in resolution, never saved.
    class GuestGround {
    public:
      GuestGround() = default;
      // Builds the lookup by cell from the walls and pieces. Carriers are sorted by key.
      GuestGround(std::vector<GroundWall> walls, std::vector<GroundPiece> pieces,
                  std::vector<CarrierGroup> carriers);
      // The group of the paving the carrier runs on, or none for a carrier the ground doesn't know.
      std::optional<uint32_t> groupOf(EntityKey carrier) const;
      // Whether a straight line from one point to another stays on the group's paving with a guest's
      // box on it: it crosses none of the group's walls, and keeps GUEST_CLEARANCE from each, or,
      // from a wall nearer than that to where it starts, comes no nearer. Allocates nothing.
      bool clearLine(uint32_t group, GroundPoint from, GroundPoint to) const;
      // Whether a guest's box fits at the point on the group's paving: the point is on a piece of
      // the group and at least GUEST_CLEARANCE from each of its walls, or from each other than door
      // ends when doorEnds is false.
      bool fits(uint32_t group, GroundPoint point, bool doorEnds) const;
      // The first piece of the group the point stands on, else the first piece of any group it
      // stands on, else none. Pieces are in the order the ground was built with.
      const GroundPiece *pieceAt(GroundPoint point, std::optional<uint32_t> group) const;
      // The nearest point to the point where a guest's box fits, with its group, or none when no
      // point fits.
      std::optional<GroundSpot> nearestFit(GroundPoint point) const;

      // Lists the inputs; the lookup is rebuilt from them only by the constructor.
      template <typename Visitor> friend void visitFields(Visitor &visitor, GuestGround &ground);
    private: ...
    };

    // The ground the last resolution derived, or an empty ground before the first.
    const GuestGround &guestGround(const World &world);
    // Registers the derived type guest-ground and its resolver, guest-ground, after path-networks.
    void addGuestGround(WorldSchema &schema);

Give GroundWall, GroundPiece, and CarrierGroup visitFields. GroundPiece's lists the piece's kind, owner, its four corners as "c0" to "c3", its count, the group, and the carrier, since the schema's fields don't include std::array. GuestGround's visitFields lists walls, pieces, and carriers, as Network's does.

Step 2: In ground.cpp, the private lookup is a grid of square cells WALL_CELL_SIZE, 4 m, on a side, covering the park square of side PARK_SIZE centered on the origin. A point outside the square falls in the nearest edge cell. For each cell, the constructor lists the walls and the pieces whose bounding boxes overlap it, in their order, as two flat index lists with a start for each cell, as Network keeps its edge ends. Nothing else is built.

Step 3: Write the resolver, resolveGuestGround, in ground.cpp:

- Take `segments = parkSegments(world)`, `keyed = keyedWalkways(world, PathKind::Guest)`, the walkways of `keyed` in order, and `surface = parkSurface(world, those walkways)`.
- Groups: over the guest segments with ground lines, in key order, join each segment's From and To points with a union-find on point keys. Number the groups in the order their first segment comes. Each such segment's key, and each of its points' keys, maps to its group. Each connector's key maps to the group of its walkway's Joins segment.
- For each keyed walkway that walkwayPiece(segments, its Way) gives a piece for: add a GroundPiece with that piece, the Joins segment's group, and the connector's key as carrier. Add a GroundWall from the piece's Corners[3] to its Corners[0], with that group and DoorEnd true.
- For each piece of the surface with Kind PathKind::Guest that equals none of the walkway pieces: add a GroundPiece with its owner's group. Its carrier is its owner when the owner is a segment, and otherwise the lowest-keyed guest segment with a ground line that ends at that point.
- For each edge line of Kind PathKind::Guest, for each pair of consecutive positions of edgePoints(surface, line) that differ: add a GroundWall between them with the owner's group and DoorEnd false.
- CarrierGroup for each guest segment with a ground line and each connector.
- Create the derived entity with `world.createDerivedEntity(NULL_KEY, hashName("guest-ground"), 0)` and emplace or replace its GuestGround, so every resolved park holds one.

guestGround finds that entity as parkNetwork finds the network's, and returns a static empty ground when there is none. addGuestGround registers `GuestGround` as `"guest-ground"` with DataKind::Derived and the resolver `"guest-ground"` with dependency `"path-networks"`. In addGuests, call addGuestGround after registering the guest component.

Step 4: Define groupOf (binary search over the sorted carriers) and pieceAt (the pieces listed in the point's cell, using pieceDistance(piece, point) == 0.0 for standing on it). Define clearLine, fits, and nearestFit as stubs that return true, true, and none, for Task 7.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 7: The ground's queries

Files:
- Modify: `src/sim/guests/ground.cpp`

Use only +, -, *, /, and std::sqrt in these, never std::hypot or other library math, so every build gives the same results (decision 0022). Compare squared distances where that avoids a square root.

Step 1: clearLine. Visit the cells within GUEST_CLEARANCE of the line, row by row: for each row the line's band, widened by GUEST_CLEARANCE, crosses, take the columns within GUEST_CLEARANCE of the part of the line inside that band. For each wall listed there whose group is the group:

- If `to` lies strictly to the wall's right, and the line touches the wall, the line crosses it: return false.
- Otherwise, the line must keep from the wall at least the lesser of GUEST_CLEARANCE and the distance from `from` to the wall, less 1e-9 for rounding. The distance between the line and the wall is 0 if they meet, and otherwise the least of the four distances from one's ends to the other. If it is less, return false.

Return true. A wall listed in several cells is tested again in each; that only repeats work.

Step 2: fits. False when pieceAt(point, group) gives no piece of the group. Otherwise, true when every wall of the group listed in the cells within GUEST_CLEARANCE of the point, leaving out door ends when doorEnds is false, lies at least GUEST_CLEARANCE less 1e-6 from it.

Step 3: nearestFit. Search rings of cells around the point's cell, ring 0 being that cell, then the cells around it, and so on, until the ring leaves the grid. In each ring, for each wall listed in its cells, door ends included, take these candidates, each with the wall's group:

- the point of the wall's copy shifted GUEST_CLEARANCE to its left, nearest the point;
- for each end of the wall, the point GUEST_CLEARANCE from that end toward the point, unless the point is that end;
- for each other wall of the same group listed in the same cell, where the two shifted copies' lines cross, if they do.

Keep the nearest candidate that fits its group with door ends counted, ties to the one found first. After each ring r, stop if a candidate was kept and it lies no farther than r times WALL_CELL_SIZE from the point. Return the kept candidate, or none.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 8: A guest's route ahead

Files:
- Create: `src/sim/guests/internal/route_ahead.h`
- Create: `src/sim/guests/route_ahead.cpp`
- Modify: `src/sim/CMakeLists.txt` (add `guests/route_ahead.cpp` after `guests/guests.cpp`)
- Modify: `src/sim/guests/guests.cpp` (homeEntryAt moves out)

Step 1: Write internal/route_ahead.h, guard TPJ_SIM_GUESTS_INTERNAL_ROUTE_AHEAD_H. It declares:

    // A point of a guest's route ahead: where it is on the ground, the carrier of the stretch of route
    // that ends at it, the distances along that carrier where the stretch starts and ends, and
    // whether the point is a node.
    struct RoutePoint { GroundPoint At; EntityKey Carrier = NULL_KEY; double From = 0.0; double To = 0.0; bool Node = false; };

    // A guest's route ahead of its place: its points, the first at the place, and whether the last
    // is where the route ends rather than where looking ahead stops.
    struct RouteAhead { std::vector<RoutePoint> Points; bool Ends = false; };

    // A point along a route ahead: the index of the point its stretch ends at, and how far along
    // that stretch it lies, from 0 to 1. Index 0 is the route's first point.
    struct RouteSpot { size_t Point = 0; double Along = 1.0; };

    // The least entry at the place among the sources' route distance entries on the guest network,
    // ties to the lower key, or none. Sources are in ascending key order.
    std::optional<RouteEntry> leastEntryAt(const World &world, const Network &network,
                                           const Place &place, std::span<const EntityKey> sources);
    // Fills the route from the place along the first step, and on from each node it reaches by
    // the least next step among the sources' entries there, until AIM_LOOKAHEAD along it. With no
    // sources, the route is the first step alone, and it ends at that step's To. Allocates nothing
    // once the route's list has grown to its largest.
    void traceRoute(const World &world, const Network &network, const Place &place,
                    const RouteStep &first, std::span<const EntityKey> sources, RouteAhead &route);
    // The first spot of the route at the place, or none when the route doesn't pass it.
    std::optional<RouteSpot> spotOf(const RouteAhead &route, const Place &place);
    // The spot's place on the network, and its ground position.
    Place placeAt(const RouteAhead &route, RouteSpot spot);
    GroundPoint pointAt(const RouteAhead &route, RouteSpot spot);
    // Moves the aim forward along the route to the farthest spot a straight line from the point
    // reaches on the group's paving: each later route point in turn while the line to it is clear,
    // and then, within the stretch to the first that isn't, as far as halving finds clear.
    RouteSpot advanceAim(const GuestGround &ground, uint32_t group, GroundPoint from,
                         const RouteAhead &route, RouteSpot aim);
    // The spot of the route from its start up to the aim nearest the point, ties to the earlier.
    RouteSpot nearestSpot(const RouteAhead &route, GroundPoint point, RouteSpot aim);

Step 2: Move homeEntryAt out of guests.cpp as leastEntryAt, unchanged in what it does, with sources in place of entrances. In guests.cpp, headingOf calls `leastEntryAt(world, network, guest.At, entrances)` for home.

Step 3: traceRoute. Clear the points. The first point is the place's ground point, with the place's carrier and distance as both From and To, and Node true when the place resolves to a node. Then walk the first step: its stretch runs along the step's carrier from the place's distance, when the step is on the place's carrier, and otherwise from the step's From, to the step's To. Add a point for each carrier point strictly between the two, in walking order, and one at To with Node true, each with its From the distance the stretch to it starts at. Count the distance walked as the change in carrier distance. If a point would lie beyond AIM_LOOKAHEAD, add instead the point exactly AIM_LOOKAHEAD along, from groundPoint at that distance, with Node false, and stop with Ends false. At the step's To, with no sources, stop with Ends true. Otherwise take leastEntryAt the place at To on the step's carrier. If it has no entry, stop with Ends false. If its Next has carrier NULL_KEY, stop with Ends true. Otherwise walk its Next the same way.

Step 4: spotOf finds the first stretch whose carrier is the place's and whose distances run from at or before the place's distance to at or after it, in the stretch's direction; the spot's Along is the place's share of the way. Spot index 0 with Along 1 is the first point itself; spotOf gives it when the place is the route's first point. placeAt interpolates the distance between the stretch's From and To; pointAt interpolates between the previous point's position and this one's.

Step 5: advanceAim. From the aim's stretch on: if the aim's Along is below 1, its own stretch's end is the next candidate, else the next point's. While the line from `from` to the candidate is clear, move the aim to it. At the first candidate that isn't clear, halve the stretch between the aim and the candidate eight times, moving the aim to each clear midpoint, and stop. Stop also at the route's last point.

Step 6: nearestSpot. Over each stretch from the route's start up to the aim's, take the point of the stretch nearest `point`, limited to the aim's Along on the aim's own stretch. Keep the nearest, ties to the earlier.

Step 7: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 9: Walk on the paving

Files:
- Modify: `src/sim/guests/guests.cpp` (walk, walkStep, stepGuests, and their comments)

Step 1: stepGuests makes one RouteAhead, one list of steps, and one list of weights before its loop, and passes them to walk with guestGround(world), so walking allocates nothing per guest once the lists have grown.

Step 1a: Rewrite wanderStep so a pick at a node reads only that node's edges and allocates nothing. It takes the list of steps and clears it. For each of network.edgeEnds(node), in order, it adds the step along that edge away from the node: from the end's distance to the other end's. It keeps the way back apart as now. The keyed pick draws over a weights list of ones, kept beside the steps and resized to the onward steps' count. edgeEnds gives ends in ascending edge index with an edge's From end first, the order stepsFrom lists steps in today, so the same draw picks the same step. Remove stepsFrom, which nothing calls now.

Step 2: Rewrite walk. It keeps its loop and its order up to choosing: resolve the place, leave if it doesn't resolve, take the heading, send the visit or leave when the heading's Next has carrier NULL_KEY, and choose when it must or at a node with distance left, at most once until the place moves. After choosing, set the aim to the place. Then, while distance is left:

- The first step is the target's Next, the home entry's Next, or wanderStep's, as now. The sources are the target alone, the entrances, or none when wandering.
- Trace the route ahead into the RouteAhead. Find the aim's spot with spotOf; if the route doesn't pass the aim, use spot 0, the place. The group is ground.groupOf of the place's carrier; if it has none, return true.
- Advance the aim from the guest's ground position. Move the ground position toward pointAt the aim by the lesser of the distance left and the distance to it, and take that from the distance left. Moving straight toward a point it has found clear keeps the guest on the paving.
- Take the place as nearestSpot of the new ground position, up to the aim. Set Forward to whether the stretch it is on runs toward higher distances. Store the aim's place in the guest.
- If the route ends, the guest has reached the end when its ground position stands on the route's last point, or when it stands at its aim, the aim lies on the route's last stretch, and advancing it this pass moved it less than 1 cm, so halving can't creep it on for ever. A door's end can meet its connector at a slant, so the nearest clear point of the route can lie well over GUEST_CLEARANCE from the door. Then its place becomes the last point's place, at the step's To, and so does its aim.
- If the place passed or reached a node of the route other than the first point, it must choose. Continue the outer loop, which chooses, visits, or leaves at the new place.
- If the ground position and the place didn't change, return true.

Step 3: Remove walkStep, which nothing calls now. Rewrite walk's comment and stepGuests's to say what they do now.

Step 4: Build and run the app on main-street.park long enough for guests to arrive, and capture it.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 3600 --capture build/windows-debug/paving-walk-street.bmp`
Expected: the app prints `GPU driver: vulkan` and exits with status 0. Look at the capture: guests stand on the paving, none on the grass, and some stand off the center lines.

### Task 10: Carry guests by their ground position

Files:
- Modify: `src/sim/guests/guests.cpp` (carryGuests and its comment)

Step 1: Rewrite carryGuests. It still does nothing without a previous network. For each guest:

- Carry its place with carryOver, and take that carrier's group, if any, as the group it prefers.
- Take pieceAt the ground position with that group. If there is a piece and the box fits on its group there with door ends left out, the guest keeps its ground position. Leaving door ends out keeps a guest that has just come through the entrance's door where it stands. If the carried place's carrier is in the piece's group, the place is the carried place, and the guest keeps its way over the ground as now. Otherwise the place is nearestPlaceOn the piece's carrier at the ground position.
- Otherwise take nearestFit of the ground position. If there is one, the ground position moves there, and the place is nearestPlaceOn the carrier of pieceAt that point with its group. If there is none, leave the place as it is: no guest paving is left, so it doesn't resolve, and the guest leaves when it next steps.
- Set the aim to the place.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 11: Make the parks that hold guests again

Files:
- Modify: `tests/parks/warm.park`
- Modify: `tests/parks/cut.park`
- Modify: `tests/parks/stress/full.park`

Step 1: Remake the slice parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park; echo "status $?"`
Expected: `status 0`, and fed.park unchanged in `git status --short tests/parks`.

Step 2: Remake the full park.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo "status $?"`
Expected: `status 0`.

Step 3: Run the park world checks and the food loop.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe 2>&1 | tail -3 && build/windows-debug/tpj_integration_tests.exe 2>&1 | tail -3`
Expected: both report `All tests passed`. If a check that needs a guest waiting at the shop or heading to it fails on warm.park, that is a Major deviation: stop and report it.

### Task 12: Check that guests stay on the paving

Files:
- Create, never committed: `build/paving-walk/stays.cpp`

Step 1: Write stays.cpp, beside Task 1b's build.sh. It reads only public headers: sim/park_schema.h, sim/save.h, sim/world.h, sim/park/intent.h, sim/park/surface.h, sim/routes/networks.h, and sim/guests/guests.h. For each path on its command line, load and resolve it as build/paving-holes/closure.cpp does, then step it 3600 times. Before the steps, work out:

- the walkways from keyedWalkways(world, PathKind::Guest), and parkSurface with them;
- groups of joined guest paving, as Task 6's resolver works them out, and the group of each segment, point, and connector;
- the visible guest edge lines' runs and the walkways' door ends, each with its group;
- each guest piece with its group, taking walkway pieces' groups from their Joins segments.

After each step, for each guest's record: its group is that of record.At.Carrier, and its distance is the least distance from its Position to a run or door end of its group. Keep, for each guest, the greatest such distance seen so far. A guest is off the paving if no piece of its group has pieceDistance 0 from its Position, or if its distance is less than the lesser of GUEST_CLEARANCE and its greatest distance so far, less 1e-6. That allows a guest that starts nearer an edge than GUEST_CLEARANCE, at the entrance's door or where a park's save put it, but never lets it come nearer, and holds every other guest to the full clearance. Print the first few guests off the paving with the tick, key, and position, and at the end `<path>: <guests seen> guests, <count> guests off the paving`.

Step 2: Build the check and run it.

Run: `build/paving-walk/build.sh stays && build/paving-walk/stays.exe tests/parks/*.park parks/*.park`
Expected: one line for each park, each ending in `0 guests off the paving`. If any guest is off the paving, that is a deviation: find the cause before going on.

### Task 13: Play the feature

Files: none.

Step 1: Build windows-release, which the user plays.

Run: `cmake.exe --build --preset windows-release --target tpj_app`
Expected: the build succeeds with no warnings.

Step 2: Capture main-street.park after guests arrive, with the food overlay.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/main-street.park --ticks 3600 --overlay food --capture build/windows-debug/paving-walk-overlay.bmp`
Expected: `GPU driver: vulkan`. In the capture, footfall shows along the paths guests use.

Step 3: Ask the user to play main-street.park and parks/busted-path.park with build/windows-release/ThemeParkJones.exe and check criteria 1, 2, 4, and 5: guests walk straight to doors across the street, cut the inside of bends, stop in front of doors, turn back at free ends, never leave the paving, and don't move when the street is widened, split by a crossing path, or merged by deleting a point. Fix what they find in code, as play findings are fixed.

### Task 14: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

Step 2: Make the runtime report after the change and compare it with Task 1's.

Run: `scripts/runtime-report.sh --launches 3 build/paving-walk-after.report && scripts/runtime-report.sh --compare build/paving-walk-before.report build/paving-walk-after.report`
Expected: the comparison prints each stage's change. Report the guests stage's time on full.park, and the tick's total, against the 33 ms budget to the user. Nothing is gated on it.

### Task 15: Commit

Files: none.

Step 1: Stage only this feature's paths: src/sim/guests/, src/sim/CMakeLists.txt, tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park, plans/believable-guests/spread-guests/MILESTONE.md, plans/believable-guests/spread-guests/paving-walk/, plans/believable-guests/CAPABILITY.md, plans/slices/good-paths/SLICE.md, and plans/slices/good-paths/RESEARCH.md. Commit them by pathspec with the commit-hygiene skill, subject `Guests: Walk on the paving inside its outline`.
