# Research: park-routes-reshaped

## Which statements of the park and routes specs are trimmed?

src/sim/park/SPEC.md fixes tuning numbers and copies formulas from the code. The numbers are PARK_SIZE's 256 m and the -128 to 128 it gives, pathWidth's 3 m and 2 m, boxSize's and ENTRANCE_SIZE's sizes, MIN_POINT_SPACING's 1 cm, MIN_SEGMENT_SAMPLES's 8, GROUND_LINE_SPACING's 1 m, CONTACT_TOLERANCE's 1 mm, and the new park's "z = 128". The formulas are:

- keptPoints's squared-distance comparison;
- the Catmull-Rom phantoms, knots, Barry and Goldman pyramid, and sample count;
- the distance sum's sqrt;
- footprintOf's normalization steps;
- the corner projection onto an axis;
- the frame in which a line's distance to a rectangle is measured.

Each number goes. The names stay, except MIN_SEGMENT_SAMPLES and GROUND_LINE_SPACING, which nothing outside geometry.h and geometry.cpp names and which leave the spec with the sample count. Each formula becomes the property it computes:

- a point is dropped when it is closer than MIN_POINT_SPACING to the last kept point;
- the curve is the centripetal Catmull-Rom spline, and its phantoms continue each end's chord, so a two-point path is straight;
- each segment contributes its first kept point exactly and then points on the curve. How many points is left to the code, since nothing outside park depends on the count, and the line's own points are what validity and routes measure;
- a distance grows by each step's straight length;
- Forward is the unit vector of any finite facing of nonzero length;
- a line meets a solid when some point of a segment comes nearer the rectangle than its half width less CONTACT_TOLERANCE.

src/sim/routes/SPEC.md copies the side formula, the crossing fractions, the projection formula, the squared tolerance test, the door's (first + second) / 2, and the reach's sqrt. It also fixes JUNCTION_TOLERANCE's 1 mm and CONNECTION_REACH's 4 m. These go the same way. Crossing becomes each segment's ends lying strictly on opposite sides of the other's line. A meeting lies the same fraction of the way between a segment's distances as along it. An end's nearest point on a segment is exactly the segment's end when it is nearest there.

What stays, and why:

- The save lines, kind names, and registration orders: saves hold them.
- networkKey and connectorKey: they are derived keys that saves and other modules use.
- The corner order and Right = (-Forward.Z, Forward.X): routes reads doors from the corner order, and render and tools index it.
- The new park's coordinates: tests/parks/new.park is its save, app's --hash test compares against it, and scenarios' full_park.cpp extends its path from (0, 123).
- Route distance's statements: they state properties already.

Other modules name these constants and never their values. scenarios uses PARK_SIZE. render and tools use pathWidth, boxSize, and ENTRANCE_SIZE. legible uses connectorKey. No other module's spec states a park or routes number.

## Which property did a trimmed number stand in for?

With CONNECTION_REACH's 4 m and pathWidth's values gone, nothing says a box placed flush against a line can connect to it. Validity keeps a box at least half a line's width, less CONTACT_TOLERANCE, from the line's ground line. A door connects when its reach is at most CONNECTION_REACH. So the routes spec now states that CONNECTION_REACH exceeds half of every pathWidth, which means a door flush against a line of the kind it serves has a connector. connections_test shows it with a door flush against a line among its connected parks.

A door within JUNCTION_TOLERANCE of a line lies in no physically valid park. This rests on half of every pathWidth, less CONTACT_TOLERANCE, exceeding JUNCTION_TOLERANCE. The routes spec already says this in words, and it stays.

## Which unlisted cases lean on the trimmed numbers?

Twelve cases outside the inventory place inputs for a reason stated against a number the trim drops. They pass today, but they rest on values the spec will no longer state. Evan chose to add them to the inventory with their properties unchanged, so the test-writer rebuilds their setups from the constants by name.

networks_test has three:

- "Where two segments meet…" uses throughLinePoint and collinearOverlap. These put lines through line points at whole meters, which holds only while GROUND_LINE_SPACING is 1 m and a two-point path's samples are evenly spaced. The rewrite takes those points from groundLine itself, as selfCrossing already does.
- "Consecutive stops of a carrier lie more than the tolerance apart" uses clusteredMeetings. Its lines are 0.5 mm and 0.8 mm apart, chosen against a 1 mm JUNCTION_TOLERANCE.
- "Nodes are numbered in order of first stop…" also uses clusteredMeetings.

connections_test has five, the cases over connectedParks():

- "Each kind's network has its paths as carriers…"
- "A connector has two points…"
- "A connector's first stop is at a node…"
- "A connector stops only at its door and its connection…"
- "A network's anchors are exactly its connectors' door nodes…"

Those parks lean on the trimmed numbers in several ways:

- servingPark writes door positions from literal box and entrance depths, with lines 2 m and 3 m beyond them, inside a 4 m reach.
- connectionPlaces puts shop 7's door 0.5 mm from a junction.
- turnedShop's expected connection point (3, 4) follows from a 6 m shop depth.
- The door check compares bit for bit with the test's own (first + second) / 2, and the reach check with its own sqrt. The spec no longer states either formula.

previous_networks_test has one: "A finisher registered … reads, in each resolution after a world's first, …". It REQUIREs that the shop pushed at (6.5, 80) anchors the guest network. That holds only for the current shop depth and connection reach.

route_distance_test has three. Its separated park places each line a literal 2 m from its door, and REQUIREs that the doors anchor. Two cases use that park: "sampleField of a kind's route distance field at a node gives…" and "Following Next from any place…". The random edit sequence's opening shop faces its line across a 1 m gap, which puts its door 2.5 m from the line, and the case checks that the sequence routes through it. Each holds only while CONNECTION_REACH exceeds those gaps, and the spec now promises only that it exceeds half of every pathWidth.

Tests in later modules also place shops beside paths at literal gaps. They belong to those modules' features, which rewrite only their own inventory entries, so this feature leaves them alone.

## What does each listed case check, and where is each check after the rewrite?

A rewrite that keeps only an entry's property can lose a check the old case made beside it, as medium-reshaped lost keptValue's absent case. So every check of the 22 listed cases is accounted for here.

- **edit_sequences_test's random case:**
  - Validity after every cycle moves to the merged acceptance test. With edits_test's "Commands queued for a cycle are each applied exactly when isAccepted is true…", which stays, it shows that every cycle leaves the park valid.
  - Copy, save, load, and re-save are walk_test's and save_test's, so they go (rule 2).
  - The tally of each command kind accepted and refused checked only the random draws, so it goes.
  - Its draws reached commands of no enum kind; refusing those is now in the merged acceptance test.
  - Its draws also reached non-finite points, zero facings, and stale keys without throwing. Those stay in, respectively, the groundLine rewrite's never-throws, footprintOf's "gives none exactly when … never throws", and edits_test's "A command naming no intent of the kind it acts on is refused, without throwing", all unlisted except the first.
  - Once its case is merged, the file holds nothing and is deleted with its CMake line.
- **edits_test's four accepted-command cases:**
  - An AddPath through its kept points, dropping a repeated point and one closer than MIN_POINT_SPACING to the last kept.
  - An AddBox's and a MoveBox's pose bit for bit, -0.0 and an unnormalized facing included.
  - MoveBox keeps the key and kind.
  - Deletes destroy only the named entity.
  - Nothing else changes: other intent, the key set, the seed, and the tick stay as they were.
  - The key counter advances by one for an add and not otherwise.
  - All of these go to the merged criterion, which names its inputs.
- **edits_test's validity-equivalence case:** it checks isAccepted == isPhysicallyValid(the world described) over many hand-placed commands, with expected outcomes from literal sizes. The rewrite keeps the equivalence and takes the described world's validity as its oracle. It uses a few inputs chosen for a reason: a command that fits, one that overlaps a box, a MoveBox onto part of its own old footprint (judged without itself), and an AddBox whose kind is no enum value. Both outcomes must occur. The flush and edge inputs move to validity_test's touching property, and zero facings and too-short paths to the no-footprint property.
- **geometry_test:**
  - groundLine's empty case checks the edge as inclusive and one step past it as outside, a NaN, an infinity, and too few kept points. All of these stay, with bounds from PARK_SIZE and MIN_POINT_SPACING by name.
  - keptPoints' case checks the kept list and that groundLine(kept) equals groundLine(points). The list check becomes: each kept point lies at least MIN_POINT_SPACING from the last kept one, and a point closer than that to the last kept one is dropped even when it lies more than that from the point before it.
  - The distances case checks a bit-exact sum and strict increase. Only strict increase stays. That distances measure the line stays shown by networks_test's meeting cases, whose expected distances are ground distances along straight lines within SLACK.
  - The corners case checks the default pose's Right and corners at three literal poses. It becomes corners in FL, FR, BR, BL order at plus or minus half the size along Forward and Right. It is shown on the default pose, whose Forward is -z, and on one pose off the axes, with sizes by name.
- **validity_test's four boundary cases:** they check touching against 2 mm deep for solid against solid (on an axis, diagonal, corner to side, box to entrance), line against solid (beside a side; a segment's middle near a corner whose points are not), solid against edge (on an axis, diagonal), and line against edge (straight; a curve that swings past its points). They merge into one test. The diagonal, segment-not-points, and curve-past-points inputs stay as reasoned inputs, with offsets from boxSize, pathWidth, ENTRANCE_SIZE, and CONTACT_TOLERANCE.
- **validity_test's no-footprint case:** it checks an unfacing entrance, an unfacing box, and four paths whose lines are empty. One of each kind of intent stays. The paths' variety is groundLine's emptiness rule, which geometry owns.
- **connections_test's unconnected case:** its five inputs stay (beyond reach, within tolerance, no line of the kind, no footprint, a path keyed like the connector). The first two are placed by CONNECTION_REACH and JUNCTION_TOLERANCE, and doors by boxSize.
- **The two interior-stop cases:** they check interior stops of path carriers against known meetings (networks_test's meeting cases) and against meetings and connections (connections_test's parks). The merged test covers both kinds of park.
- **route_distance_test's three tie-break cases:**
  - lowest carrier key, even toward higher distances;
  - lower direction first, even from a higher stop;
  - lowest From.
  - Each tie level stays, in one network with a tie at every level.
- **sampleRouteEdge's case:** its sections are the To end nearer, the From end nearer, the least of several at one end, and one end only. Each end winning, several at one end, and one end only stay. The tie and the ignored inside entries are unlisted cases, which stay.

## Which test support can the rewrites leave unused?

The rewritten files include park_worlds.h, same_networks.h, route_edits.h, and synthetic_network.h. A check by name over each header's includers finds seven names that no includer uses, each referred to by a used definition in its own header: writePose and writeKey, through worldOf, and ROUTE_EDIT_REGION, snappedPoint, routeEditPath, routeEditPose, and besideLine, through routeEdit. synthetic_network.h was checked by medium-reshaped, and route_distance_test's unlisted cases still use it.
