# Slice: Paths and Plazas

Status: planned

## Summary

The player lays out the park's walkable ground as layered shapes, the way an art director sketches. A path is a line with a width. An area is a polygon, clicked out corner by corner or stamped down as a square. A clearing is an area that cuts a hole in the paving under it. Shapes stack with the newest on top, and the player edits every shape the same way: drag, insert, and delete vertices, round corners, and drag a path's side to change its width. There is no plaza object. A plaza is paving wide enough to cross. The walkable graph comes from the shape of the paved ground alone: a narrow path gets one line down its middle, a wide path gets lanes, and open paving gets a mesh that guests cross nearly straight. This is the right next goal because building is the reason this game exists (docs/vision.md), and the boxes-and-tubes tools were only the minimum needed to draw a park. Planet Coaster 2 shows what a finicky path system costs. Its paths and stamps fuse into one hidden surface that shows when something is deleted, some of its snaps act when snapping is off, and its guests stall on plazas. In this slice, each piece of paving stays its own shape. The game derives the merged ground every time and never saves it (principle 1). Every snap is visible and can be suppressed, and routing doesn't depend on how the ground was drawn.

## End-to-end scenario

The player builds a grand entrance modeled on Kings Dominion's. They pick the area tool and click out a long rectangle, 45 m wide, for a promenade leading from the entrance. Guides appear as they click. Each new edge snaps to run straight on from the last, or at 90 or 45 degrees to it. Its length shows in meters and snaps to whole meters. Each snap shows as a guide line in the ghost, and no snap acts without its guide. Holding a key suppresses every snap at once.

Down the middle of the promenade they draw a clearing for a reflecting pool. The paving under it disappears. The promenade now reads as two broad walks, one on each side of the pool, joined at both ends. At the far end they stamp a square, and size and turn it as it lands. They select its four corners and drag one radius handle until the corners round fully into a circle. They draw two guest paths that cross in its middle. Between the arms they stamp four small squares, rounded a little, as clearings for planters. The circle, the cross, and the promenade read as one paved surface, with no seams where they overlap. Narrow paths leave the hub to either side.

Then they adjust the layout. They click an edge of the promenade to insert a vertex, and drag it out to widen one corner. They drag a path's side to widen it from 3 m to 5 m. They continue a narrow path from its end, and delete a vertex that made a kink. They drag a planter clearing a little off center. They box-select the ends of three paths where they meet and drag the meeting point, and all three ends follow. To reach a shape that others cover, they click again at the same spot, and each click selects the next shape down the stack. A path was drawn over one planter clearing, so they bring the clearing to the front. When they dislike a step, one undo removes it, and redo brings it back.

They place shops along both sides of the promenade. Each ghost turns to face the paving, slides to a setback from its edge, and connects by its front door. They place a depot behind the shops on one side. They draw a backstage path from the depot to those shops' back doors, and on around the end of the promenade to the back doors of the shops on the other side. The backstage path crosses a guest path without joining it.

Guests arrive. On the narrow paths they walk in a line down the middle, spread across the path's width. On each side of the pool they walk the broad walk in lanes. In the hub they cross between the arms and around the planters. Wanderers on open paving keep their heading instead of jittering. Guests rethink where they are going no more often on open paving than on a narrow path, so nobody stops to dither in the middle of a plaza. The graph view shows the networks: one line down each narrow path, lanes where a path is wide, a mesh over open paving, and nothing over the pool or the planters.

The player deletes the pool. The promenade becomes one open area, and guests start crossing straight between shops that face each other across it. Only the graph near the pool changes. Guests who stood where the graph changed move to the nearest place they can walk, and nobody is lost or stuck. Undo brings the pool back.

## Acceptance criteria

Three park files are checked in under tests/parks:

- promenade.park holds the scenario's park with no guests: the entrance, the promenade with the pool clearing, shops along both sides, the round hub with its crossing paths and four planter clearings, narrow paths leaving the hub, a depot, and a backstage path to every shop's back door.
- pool-promenade.park is promenade.park warmed until guests fill the park.
- open-promenade.park is promenade.park with the pool clearing deleted, warmed the same way.

tpj_scenarios --slice-parks makes the two warmed parks from promenade.park, as it makes warm.park from fed.park. Every other park in tests/parks and tests/parks/stress is rewritten with path shapes.

The park world checks and the cross-build check read every park in tests/parks. Once the three parks are there, those checks cover the new intent, the derived networks, and guests on open paving without a new test. open-ground-guests adds deleting the pool clearing from pool-promenade.park to the world checks' chosen edits, so its preview is checked against its commit (principle 8). food_loop_test must keep passing on the rewritten warm.park.

1. Deleting the pool clearing from pool-promenade.park shortens the route between two shops that face each other across the pool. (integration test)
2. Deleting the pool clearing from pool-promenade.park leaves every carrier along the narrow paths beyond the hub with the same key, points, and widths. The edit carries no guest on those carriers to another place, and drops no hungry footfall on them. (integration test)
3. Placing a shop beside the promenade in pool-promenade.park leaves every carrier except the shop's own connectors with the same key, points, and widths. The edit carries no guest to another place and drops no hungry footfall, except on those connectors. (integration test)
4. Undoing the pool's deletion gives back pool-promenade.park's intent, with the pool under its old key, and the networks it had before the deletion. Redoing it gives back the intent and networks the deletion made. (integration test)
5. A capture of pool-promenade.park shows the promenade, pool, hub, planters, and paths as one paved surface, with no seams or overlapping ribbons. Corners are rounded where shapes were rounded, and there are holes where clearings lie. The shops face the promenade in rows, and guests are spread across the paving instead of walking single lines. (scripted capture: open-ground-guests)
6. A capture of promenade.park with --graph shows one line down each narrow path, lanes where paving is wide, a mesh over open paving, and no line over the pool, the planters, or unpaved ground. Where the backstage path crosses a guest path, the two networks share no node. (scripted capture: ground-becomes-routes)
7. A capture of promenade.park with --overlay food shows the overlay shading the paved ground. (scripted capture: ground-overlay)
8. In the running app, every tool, snap, and edit works as the scenario describes, and so do undo and redo. (manual)
9. In the running app, guests in pool-promenade.park and open-promenade.park are served at every shop, including shops they reach across open paving. Wanderers cross open paving in smooth lines, and guests on open paving don't stop or turn more often than on a narrow path. (manual)

## What the slice replaces

The slice replaces how paths are saved, edited, drawn, and turned into networks. When you plan a member, plan the changes to the specs, code, and parks in its own area as tasks of its milestone. Old saves don't load. Every checked-in park is rewritten in the new format. If your member changes what guests hold, regenerate the parks that hold guests. Simulation output and runtime timings change on purpose, and the runtime report taken after the last member becomes the new baseline.

- restored-keys: today a key is never reused (src/sim/SPEC.md), and entity_key_test checks that. Operations stops counting orders sent to a deleted depot. If undo restores that depot, those orders would count again and push its shops past ORDER_UP_TO, so restored-keys changes that rule too.
- carrier-widths: every caller that builds a network directly must supply a width.
- layered-paving: how paths are saved, edited, validated, and drawn, the build tools and their ghosts, the new park template, the scenarios' park edits, the full-park generator, every checked-in park including degenerate-intent.park, food_loop_test's setup, and the world checks' chosen edits. Until ground-becomes-routes lands, networks keep coming from path shapes' center lines.
- ground-overlay: the overlay's band along guest path lines (src/render/band_overlay) and the nearest guest path place (src/legible/path_place) both pick carriers by path key. They must stop doing that before ground-becomes-routes re-keys carriers. src/views/availability_overlay builds the band today, so it and views/SPEC.md change too. The overlay's values (src/legible/availability) stay as they are.
- ground-becomes-routes: how networks are derived, and everything else that picks carriers by path key or assumes one carrier per drawn path. That covers the shop preview's connection (src/legible/preview), the graph view's rule that a carrier whose key isn't a path is a connector, and door reach, which today is measured to a path's center line.
- open-ground-guests: when guests choose and how they wander, what a guest saves, and how guests are drawn and picked. The mesh and picking must use the same spread.

Also reword your member capability's brief where it describes the old path behavior.

## Medium

The slice adds no fields or flows. Three things cross capabilities: park intent, the paved ground derived from it, and the network type that every field and flow lives on. Abbreviations are those of the boxes-and-tubes slice, with LS for legible-simulation.

- Networks: the guest and backstage graphs, in SM's network type, produced by NN. Each kind's network now comes from that kind's paved ground alone, not from path lines. Every place on every carrier has a walkable width. SM's network type adds the width and queries for the width and direction at a place. The networks must meet these requirements:
  - A path of its kind's default width, 3 m for guests and 2 m for backstage, with nothing overlapping it, gives one line down its middle. The line branches only where other ground meets it.
  - Wider ground gives lanes, and open paving gives a mesh that guests cross nearly straight.
  - Every link except a door's connector lies on paved ground of its kind. Guest and backstage networks never share a node, even where their shapes overlap.
  - The networks depend only on the paved ground and the boxes' doors. If two intents stack to the same paved ground and hold the same boxes, they give equal networks, whether one shape or several cover the ground.
  - An edit changes the networks only near the ground it changed. Placing, moving, or deleting a box changes nothing except that box's connectors.
  - A door connects within its reach of the edge of its kind's paved ground. Today reach is measured to a path's center line.
  How NN builds the networks is ground-becomes-routes' to design. RESEARCH.md has the starting point.
- Carrier keys: carriers are no longer keyed by the paths they follow. This replaces navigable-networks' and shared-medium's statements that carriers are keyed from intent. If an edit changes a carrier, its places retire. Guests there move to the nearest place of the network, as they do today when their path is deleted. Hungry footfall there is dropped and builds again as guests walk. carrier-widths and ground-becomes-routes reword those capabilities' briefs.
- Consumers of the networks: BG (movement, wandering, and the spread guests are drawn with), PO (anchors, unchanged), every field sampled at network places, LS (the overlay), EB (walkways), and NN's own graph view.
- Route distance: unchanged in kind, produced by NN and measured along the new carriers. Crossing open paving costs the length of the links walked, so it is never less than the straight walk (principle 4). It must also stay close to the shortest walk inside the paved ground. ground-becomes-routes measures how close (see Open questions).

Dependencies that are not fields or flows, ruled on by decision 0025:

- Park intent: paving shapes and boxes, authored by EB. Each paving shape has a kind, guest or backstage, and a place in the stacking order. It is a path (a line of points with one width), an area (a polygon with a corner radius at each vertex), or a clearing (an area that clears paving of both kinds). A stamp becomes an area once placed. EB adds edit commands to add, move, and delete shapes; to move, insert, and delete vertices; to set the radius of one or more corners and a path's width; to move several vertices of one or more shapes at once; to bring a shape to the front; and to continue a path from its end. Only the player's own edit changes intent. The game never merges or rewrites shapes.
- Paved ground: each kind's ground as the stacked shapes give it. EB derives it from intent every time, and sim/park publishes it through a public query, as it publishes ground lines today. NN derives the networks from it, EB renders it and checks boxes against it, and LS shades it.
- Candidate resolution: every new edit previews on a candidate world, as placements do today.
- Inspection records: unchanged. BG draws a guest from the place in its record and the width and direction there, read through the network's public queries.

Undo history is the tools' own session state: a list of edits and their inverses. It is not intent and is never saved (principle 1). Undo and redo commit at once, without a preview (decision 0029). Undoing a deletion recreates the deleted shape under its original key. That key came from the counter, and no entity holds it. A key still never passes to a different entity.

## Members

These are in build order. Milestone slugs are provisional until each capability plans them. The prerequisites have landed: sound-architecture's composed-app holds tool state outside src/app/main.cpp, and scalable-runtime's following-footfall makes hungry footfall's tick work follow its guests (decision 0031). src/scenarios/main.cpp still predates decision 0027. Member 6 may extend --slice-parks there, but must not add a new concern to it.

1. `deterministic-simulation/restored-keys`: a command may recreate an entity under a key the counter issued and no entity holds, so undoing a deletion restores the same key, and saves, copies, and hashes keep their meaning. Depends on: none.
2. `shared-medium/carrier-widths`: a walkable width at every place of every carrier, which may vary along a carrier, kept through construction and copies and derived again on load like the rest of the network, with queries for the width and direction at a place. Depends on: none.
3. `effortless-building/layered-paving`: paving shapes as intent in a stacking order, the paved ground derived from them, every edit command the Medium section lists, visible snaps the player can suppress, click cycling, undo and redo, physical validity against the paved ground, the paved ground rendered as one surface, the frontage snap, each kind's default path width, the existing parks rewritten with path shapes, and promenade.park. Depends on: member 1.
4. `legible-simulation/ground-overlay`: the food overlay shades the paved ground by the value at the nearest place of the guest network off its connectors, instead of bands along path lines, and the tooltip finds its place the same way, so neither picks carriers by path key. Depends on: member 3.
5. `navigable-networks/ground-becomes-routes`: each kind's network derived from its paved ground, meeting the requirements in Medium, with widths, doors connected within reach of the paving's edge, the graph view showing the new networks and telling a connector by its anchored door, and a stress park with plazas and wide promenades, generated as the full park is and measured by the runtime report (decision 0031). Depends on: members 2, 3, and 4.
6. `believable-guests/open-ground-guests`: guests drawn spread across the walkable width by an offset from their key that varies smoothly along their walk, wanderers on open paving keeping their heading, and guests choosing after each re-choose distance walked instead of at every node, with the existing triggers still choosing at once. tpj_scenarios --slice-parks makes both warmed parks from promenade.park, and the world checks' chosen edits include deleting the pool clearing from pool-promenade.park. Depends on: members 2 and 5.

## Out of scope

- Splitting and joining paths.
- Smooth curves through area vertices, and Bezier bend handles on edges.
- Width that varies along one path. Use another shape for a flare.
- Vertices shared between shapes.
- Stamps other than a square, and saved or user-made stamps.
- Path materials, textures, and brush-painted ground.
- Frontage saved as an attachment, so boxes follow a reshaped edge.
- A tool that lays rows of boxes.
- Terrain, slopes, stairs, bridges, and heights.
- Queue paths and one-way paths.
- Crowding, capacity from width, and crowd avoidance in the simulation.
- Guests' local knowledge of the park, and wayfinding with maps (a believable-guests deepening).
- Boxes inside paving, such as kiosks, and supplying boxes from inside guest areas. Both wait on how supplies and staff reach guest areas (docs/open-questions.md).
- Moving the entrance.
- Player-facing UI. ImGui stands in, and the question stays open (docs/open-questions.md).

## Open questions

- How to build the networks from the paved ground. RESEARCH.md gives a starting point. Start from a triangular lattice clipped to the ground, relax it with Lloyd's algorithm tile by tile, and link points by the Gabriel graph, keeping only links on paving. A small test gave one chain down a 3 m path and an even mesh on a plaza. Three things remain unproven: Gabriel links near 90° at bends, seams between tiles, and keeping widths on wide ground local to an edit. ground-becomes-routes settles them while planning.
- How close route distance must stay to the shortest walk on the paved ground. The earlier draft assumed 10% plus a fixed allowance. ground-becomes-routes measures both slice parks and a large synthetic ground against a reference shortest walk. The user decides whether the measured error is acceptable.
- How near an edit the networks may change, and whether deriving them again is fast enough to resolve a candidate on every pointer move. ground-becomes-routes measures both. A preview must still equal its commit (principle 8).
- Whether a park with plazas holds its tick and its previews within budget. Route distance keeps an entry for every source at every node it reaches, so it grows as nodes times sources. A plaza mesh adds many nodes, so this product grows with paved area as well as shop count. The plaza stress park's runtime report in ground-becomes-routes measures it. Wider spacing on open ground and hierarchical routing are the fallbacks, and the user chooses between them with the numbers.
- Snap reach, guide spacing, setback, the stamp's default size, and the re-choose distance are tuning values. layered-paving, ground-becomes-routes, and open-ground-guests set them while planning and adjust them by drawing parks.

## Research notes

- Platter gives each building its own frame, snapped to the road edge with a setback, and that is what makes curved streets work. The frontage snap follows it.
- Planet Coaster 2's fusing, seams, and lost strokes come from saving merged results. Its hidden snaps come from snaps the toggle misses, and its stalled guests from plazas with no structure. Here intent stays the shapes, the ground is derived, and every snap is visible.
- Width belongs on the path, not on parallel paths, which inflate distance in Parkitect. Plazas are paving, not tiled path pieces.
- Cities: Skylines 2's surfaces are node polygons that players layer by turning off the overlap check, and Figma gives each vertex its own corner radius. Here paving is layered shapes with straight edges and corner radii, and holes are clearing shapes that stay objects.
- Cities: Skylines gives every piece of road surface one owning node or segment. Its middle nodes show why shaping vertices must not be routing nodes.
- The graph comes from the paved ground alone. Points spread evenly by Lloyd's algorithm from a triangular lattice settle into one row on a narrow path and an even fill on open ground. The Gabriel graph links them into a chain on the path and a mesh on the plaza. Plain Delaunay links turn a narrow path into a strip of triangles, and the relative neighborhood graph gives fragile, long routes on open ground. A medial axis, portals, a width threshold, a uniform grid, and DistMesh were rejected.
- Recast's tiled navmesh shows how to keep a derived graph local: build each tile from its own ground plus a margin, and rebuild only the tiles an edit touches.
- A fixed sideways offset per guest, derived from its key, spreads guests across wide ground without crowd simulation.

Depth is in RESEARCH.md.
