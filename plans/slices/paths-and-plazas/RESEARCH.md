# Research: paths-and-plazas

## What do Cities: Skylines 2's Platter and its neighbors do, and what carries over?

Platter is a zoning overhaul for Cities: Skylines 2, not a surface or area tool. The player places parcels, which are rectangles of zone cells, anywhere and at any angle, with a chosen width and depth. A parcel snaps to the road edge and to other parcels, and it can sit back from the road by a setback. The building that grows on it faces the parcel's own direction. Players like it because vanilla zoning builds one rectangular grid per road segment, which leaves wedges and gaps along curves. Platter gives each building its own frame, attached to the road edge with a setback, and that makes curved streets work. This is the prior art for boxes snapping along paths: each box takes its frame from where it meets the path, not from a grid the path owns. Platter is still in beta. Its 2026 update reworked snapping, with clearer feedback when several snaps apply.

Platter's neighbors show the rest of the toolset players reach for:

- Cities: Skylines 2's road tool has straight, curve, continuous, and grid modes. It snaps to existing geometry, round lengths, right angles, building sides, and guidelines extended from other roads. Each snap can be turned off. Its wiki warns that guideline snapping left on breaks layouts.
- Its surfaces and districts are drawn polygons.
- AreaBucket fills an area bounded by roads in one click.
- Line Tool places objects along lines, curves, and circles, with control points the player can edit after placing.
- Move It exposes a segment's ends and control points for reshaping.
- Anarchy turns off placement checks wholesale. It is one of the most used mods, which shows that players resent validation that blocks them.

The lessons are these. Offer snaps visibly, let the player suppress each one, and never force them. Give boxes their frame from the path edge they meet, with a setback. Block only what is physically impossible (principle 5).

Rejected: one grid per path for the boxes along it, because it leaves gaps on curves, the problem Platter exists to fix. Guideline snaps on by default, because they break layouts.

Sources: https://github.com/lucarager/CS2-Platter — Platter's purpose and authors; https://mods.paradoxplaza.com/mods/125278/Windows — parcels at any angle, snapping, setbacks, the 2026 snapping rework; https://forum.paradoxplaza.com/forum/threads/platter-a-custom-zoning-mod.1870699/page-9 — snap feedback in the 2026 update; https://steamcommunity.com/app/949230/discussions/0/3877095833482232497/ — zone gaps on curved roads; https://cs2.paradoxwikis.com/Roads — road tool modes and snaps; https://cs2.paradoxwikis.com/Editor:_Snapping_and_Tool_Modes — guideline snapping breaking grids; https://github.com/Cmyna/AreaBucket — flood-fill areas; https://github.com/phillycheeze/LineTool-CS2 — placement along lines with editable control points; https://www.nexusmods.com/citiesskylines2/mods/109 — Move It reshaping; https://github.com/yenyang/Anarchy — disabling placement checks.

## Why is Planet Coaster 2's path system disliked?

Planet Coaster 1 built paths in fixed segments up to 8 m wide, and players made plazas by tiling grid squares. Planet Coaster 2 replaced this with several tools: a line builder with a width setting and a merge toggle, a stamp builder for shapes that scale and snap together, a draw tool for plaza outlines, rounding and edit tools, and the old segments as a classic mode.

The complaints share a few causes:

- Paths fuse with nearby paths unexpectedly and distort finished work. Merging works by proximity, so a new path rewrites old ones.
- Paths snap with snapping turned off, because the toggle doesn't cover every snap.
- Joins leave seams, holes, and see-through triangles, because merges are mesh unions, not derived topology.
- Deleting removes more than intended, and the paint brush can't touch merged areas. Once merged, a stroke loses its identity.
- Height snaps to about 0.25 m, so queues drift, and paths on uneven ground don't mesh.
- Queues can't branch, since the line tool only extends.
- The three build modes write different kinds of state. Mixing them in one preview left paths at different heights and trapped hundreds of guests.
- On wide paths and plazas, guests walk single file or zig-zag, stall in the middle, and loop.

Planet Coaster 1's crowd system explains the last complaint. Walkable space is split into 4 m chunks, and guests follow a flow field for each goal. Any paved area is walkable, but a plaza gives guests no structure beyond the field.

Updates through mid-2026 added invisible paths, a circle stamp, square corners, paths that follow the terrain, and smoother queue movement. None of them changed merging by proximity, deletion from the merged surface, or single-file walking on plazas, so the complaints still stand. The terrain option suggests players expect paving to follow the ground's height, which this slice leaves out.

The lessons for this slice are these. Save the strokes and outlines the player made as intent. Derive merges, joins, and seams from them every time, so nothing is rewritten behind the player's back and every stroke stays editable on its own. Make every snap visible and suppressible. Have every tool save the same kind of intent.

Rejected: merging by proximity that rewrites existing intent, because it edits what the player didn't touch. Saving merged areas as one mesh, because strokes lose their identity. Hidden or unconditional snaps, because they break symmetry and trust. Several modes that save different state, because they give mismatched results. Drawing that only extends a path's end, because then queues and side paths can't branch.

Sources: https://www.thegamer.com/planet-coaster-2-path-types-build-how-to-use-guide/ — the tools, width scale, delete overreach, stamp misalignment; https://www.thegamer.com/planet-coaster-2-preview-pools-paths-pandemonium/ — mixed modes trapping guests; https://steamcommunity.com/app/2688950/discussions/0/604149632150356405 — snapping with snapping off, see-through triangles; https://steamcommunity.com/app/2688950/discussions/0/684115425021153532/ — queues that only extend; https://steamcommunity.com/app/2688950/discussions/0/4634862582207881074/ — height snapping drift; https://steamcommunity.com/app/2688950/discussions/0/600764072244386601/ — guests single file and stuck on plazas; https://steamcommunity.com/app/2688950/discussions/0/579397732059348240/ — the change lists of later updates; https://www.planetcoaster.com/en-US/news/2026-06-25/parades-pack-available-now-planet-coaster-2 — the June 2026 update; https://steamcommunity.com/app/493340/discussions/0/1739968490552187813/ — Planet Coaster 1 plazas by tiling; https://www.gamedeveloper.com/audio/game-design-deep-dive-creating-believable-crowds-in-i-planet-coaster-i- — the 4 m chunks and flow-field crowds.

## How do other games make plazas, wide paths, and buildings along paths?

Some games build plazas from path pieces: Planet Coaster 1, Planet Zoo, and Parkitect. These show holes, seams, clutter at junctions, and distances that depend on the tiling. Other games draw plazas as areas: Planet Coaster 2's draw and stamp tools, and Cities: Skylines 2's surfaces. Areas are cleaner to author. Planet Coaster 2's areas fail only where they merge into the path mesh and give guests no structure. Cities: Skylines 1's pedestrian plazas are a painted policy over road networks, not open walkable ground.

Width works as a property of a path. Jurassic World Evolution 3 has width tiers that raise capacity, Planet Coaster 2 has a width scale, Two Point Campus has a slider, and Cities: Skylines 2 has path widths. Parkitect shows the opposite. Its wider paths are more tiles, so guests walk more tiles and tire faster, and they circle on wide paths. Players fake width with decorative covers over one real path.

Buildings along curves work best with a frame for each building, anchored to the path. Platter does this with parcels and setbacks. Anno 117 stores roads as node-to-node graphs, and its buildings turn to face the road beside them. The approach fails with one grid per road, as in vanilla Cities: Skylines 2. It also fails when the player tunes plot corners by hand, as in Manor Lords, where players spend minutes fixing corners that jump across the road.

Tiny Glade and Townscaper are the purest form of the art-director model: the player gives a little intent, and hand-written rules resolve it into arches, stairs, and fitted modules.

Rejected: plazas tiled from path pieces, because they are bricklaying, leave seams, and make distance depend on the tiling. Wide paths built as parallel paths, because they inflate walked distance and leave seams. Plot corners tuned by hand, because that is the bricklaying this game avoids. Grids that quantize free angles, as Anno 117's subtiles do, because this game builds without a grid.

Sources: https://steamcommunity.com/app/453090/discussions/1/3264459260621669531/ — Parkitect wide paths inflating distance; https://themeparkitect.tumblr.com/post/136005228372/devlog-update-77 — Parkitect guests circling on wide paths; https://www.jurassicworldevolution.com/3/help/player-guides/construction-guide — width tiers and capacity; https://itemlevel.net/two-point-campus-how-to-make-delete-paths/ — width slider and perimeter mode; https://steamcommunity.com/app/1363080/discussions/0/689744580780860150/ — Manor Lords plot snapping; https://citybuilder.tools/tools/manor-lords/burgage-planner — plots divided along a frontage; https://www.anno-union.com/devblog-roads-building-in-the-grid/ — buildings turning to roads; https://www.anno-union.com/devblog-all-roads-lead-to-anno/ — Anno 117's roads as node graphs; https://skylines.paradoxwikis.com/Plazas_and_Promenades — plaza as a painted policy; https://80.lv/articles/exclusive-tiny-glade-developers-discuss-bevy-proceduralism-publishers-cozy-games — authored rules resolving intent; https://www.gamedeveloper.com/game-platforms/how-townscaper-works-a-story-four-games-in-the-making — Townscaper's resolution.

## How is a route graph derived from paved ground of any shape?

With no plaza object, the graph has to come from the paved ground alone, whatever shape the stacked paving gives: narrow or wide, with holes or without. The candidates were a medial axis, portals and visibility edges, a uniform grid, a navmesh, and evenly spread points.

A medial axis runs down the middle of the ground. Valhalla routes plazas this way, and the Explicit Corridor Map uses it to structure crowds. It gives routes down the middle, but on wide ground it overstates distance badly. Take two shops on the same edge of a 45 m promenade, 20 m apart. The axis runs 22.5 m in from the edge, so walking from one to the other covers 22.5 + 20 + 22.5 = 65 m. To fix that you would need a rule for where ground counts as wide, which is a plaza definition by another name.

Portals and visibility edges give exact shortest walks inside an area. OpenTripPlanner routes areas this way. But they need to know which area a portal belongs to, which again needs a plaza object or a width threshold. Their edges also grow with the square of an area's portals, and OpenTripPlanner caps an area at 200 nodes for that reason.

A uniform grid works for any shape, and its error is bounded. The worst ratio of a grid walk to a straight line is 1 / cos of half the widest angle between neighbor directions. That is 8.2% with eight neighbors and 2.7% with sixteen. But a grid's nodes don't follow the paving. A 3 m path at an angle gets a ragged chain of nodes, some on its middle and some off it.

A navmesh splits the ground into convex polygons, as Recast builds it, and the funnel algorithm straightens routes through it. It is the standard for 3D games. It gives exact walks but no graph whose places can hold field values the way this game's networks do.

Evenly spread points fit best. Lloyd's algorithm moves each point to the centroid of its Voronoi cell, the part of the ground nearer to it than to any other point. It repeats until the points settle into a centroidal Voronoi tessellation (Du, Faber, and Gunzburger). A cell can't reach past the ground's edge, so its centroid stays about half a spacing inside the edge. That gives the wanted shapes with no rule for them:

- Ground about one spacing wide settles into one row down its middle. In a test with a 3 m spacing, every point on a 3 m path at 30° ended within 0.07 m of the center line.
- Ground two spacings wide settles into two rows.
- Open ground settles into an even fill.
- Holes have no points, so routes go round them.

Spacing need not be uniform. A density function makes points sit closer on narrow ground and farther apart on open ground, where a detour of one spacing is a small share of a long crossing. Spacing scales with density to the power of minus one quarter in two dimensions, so halving the spacing needs sixteen times the density. Wider spacing on open ground keeps the graph's size near that of the narrow paths alone, instead of growing with paved area.

DistMesh (Persson and Strang) reaches an even spread differently, but it isn't a fit as written. It pushes points outward on purpose and moves points that leave the region onto its boundary, so a path one spacing wide gets points on both edges, not one row down the middle.

Where the points start matters. Lloyd's algorithm finds a local best, so a random start can leave defects, such as a staggered pair on a narrow path. Start from a lattice anchored at the park's origin and clipped to the ground. Use a triangular lattice, not a square one. A square lattice is already settled, so relaxing barely moves it. Its diagonals then sit at exactly 90°, where the link rule below drops them, and the result is a four-neighbor grid. In a test on a 30 m plaza with a 6 m hole and a 3 m path leaving it, the mean ratio of route to straight walk was 1.17 from a square start and 1.075 from a triangular one. The worst was 1.43 and 1.19.

Linking the points gives the graph. Plain Delaunay links are too many. Points in a row are never exactly in line, so the triangulation also links each point to the one two ahead, and those links lie on the ground too. A narrow path would become a strip of triangles. The Gabriel graph keeps a Delaunay link only if no other point lies inside the circle whose diameter is that link. Equivalently, it keeps the link if both angles facing it are under 90°. On a row of points, the angle at the middle point is near 180°, so the skip link goes, and the path becomes a chain where each point has two neighbors. On open ground, the triangles from a triangular lattice have angles near 60°, far from the cutoff, so the mesh stays whole. Keep only links that lie on paving.

The relative neighborhood graph also gives a chain on a path, but it is worse on open ground. It removes one edge from many triangles, and which edge goes depends on tiny differences in position. In the plaza test its mean ratio was 1.36 and its worst 2.93. That sensitivity also threatens identical results across builds (decisions 0022 and 0036). Gabriel's decisions on evenly spread points sit far from its cutoff, so they are robust. Cases near 90° can still arise at bends and corners. Beta-skeletons between the two only move toward the relative neighborhood graph's behavior. A length filter on Delaunay links works only at uniform spacing. Choosing k nearest neighbors isn't symmetric and can't give two neighbors on a path and six on a plaza.

Each link is a straight walk on paving, so route distance is never shorter than the true shortest walk (principle 4). How much longer has no useful bound. The Delaunay triangulation's worst-case stretch is below 1.65, but that covers the full triangulation in open space. A Gabriel graph clipped to paving has no constant bound: its worst case grows with the square root of the point count. The tests above suggest evenly spread points do far better in practice, so the slice measures the error against a reference shortest walk.

Rejected: a medial axis, because it overstates distance on wide ground unless wide ground is defined. Portals and visibility edges, because they need a plaza object or a width threshold to know what a portal belongs to. A width threshold that splits paths from plazas, because it is an arbitrary plaza definition, with a hard switch where a 9.9 m path and a 10.1 m path route differently. A uniform grid, because its nodes ignore the paving's shape. DistMesh as written, because it puts points on the boundary. A square starting lattice, because it relaxes into a four-neighbor grid. The relative neighborhood graph, because its links on open ground are fragile and its routes long.

Sources: https://people.sc.fsu.edu/~mgunzburger/files_papers/gunzburger-cvt-siamreview.pdf — centroidal Voronoi tessellations, Lloyd's algorithm, density functions; https://persson.berkeley.edu/distmesh/persson04mesh.pdf — DistMesh, boundary points, the size function; https://luc.devroye.org/gabriel-spanningratio.pdf — the Gabriel and relative neighborhood graphs' worst-case stretch; https://arxiv.org/abs/2609.07979 — the Delaunay triangulation's stretch below 1.65; https://arxiv.org/abs/1103.4361 — the earlier 1.998 bound; https://www.gamedeveloper.com/audio/game-design-deep-dive-creating-believable-crowds-in-i-planet-coaster-i- — Planet Coaster's chunks; https://valhalla.github.io/valhalla/concepts/pedestrian-areas/ — plaza routing on a medial axis; https://arxiv.org/abs/1701.05141 — the Explicit Corridor Map; https://docs.opentripplanner.org/en/latest/BuildConfiguration/ — area visibility and its node cap; https://www.redblobgames.com/pathfinding/visibility-graphs/ — visibility graphs; http://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html — the funnel algorithm; https://github.com/recastnavigation/recastnavigation — navmeshes.

## How can a derived graph stay local to an edit?

Relaxation repeats, so for the result to be deterministic it needs a fixed start, a fixed number of rounds, and the simulation's arithmetic. An edit must not move points far from it. Otherwise every edit would re-key the whole graph and reset everything held on it, such as hungry footfall. Relaxing only the points near an edit, starting from where they were, would make the graph depend on the order of edits. navigable-networks forbids that: the networks must depend on intent alone.

Recast's tiled navmesh is the precedent from games. It builds each tile from the geometry inside it plus a border margin taken from its neighbors, throws away what falls in the margin, and joins tiles along their shared edges. Its tile cache rebuilds single tiles when geometry changes.

The same scheme fits here. Derive points tile by tile over fixed tiles of the park. Each tile's points come only from the ground within a fixed margin of it, so an edit re-derives only the tiles whose ground it changed. A carrier must not reach across a tile border, or a neighbor's re-derived points would move it. Fixed border points, derived only from the ground near the border, pin the ground where it crosses a border. Each tile relaxes with its border points held. The Gabriel test for a link depends only on the points inside its circle, so test each tile's links against its own points plus the border points. Derive links between border points from the border's data alone. Seams between tiles are where the spread is least even, and the margin keeps them tolerable. A triangular lattice anchored at the park's origin is the same in every tile, so tiles start consistently.

Anything else stored on the network must be just as local. A width measured as the clear width of paving reaches across a whole plaza. An edit outside a tile's margin could change it, and the stored width would go stale. Widths therefore need a cap that the margin covers.

Blue-noise tiles, such as Wang tiles or corner tiles, give deterministic, local point sets whose edges match. But their points ignore the ground, so clipping them leaves ragged rows on narrow paths, the uniform grid's problem. They could serve only as seeds. Poisson disk sampling in Bridson's style depends on the order points are tried, unless each cell seeds from a hash of its position.

Rejected: relaxing the whole park on each edit, because it re-keys every carrier. Relaxing near an edit from the previous points, because the graph would depend on edit history. Blue-noise tiles as the final points, because they leave ragged rows on narrow paths.

Sources: https://github.com/recastnavigation/recastnavigation — tiled navmeshes and the tile cache; https://groups.google.com/g/recastnavigation/c/CJNtbRIb_Xc — tile border margins; https://www.researchgate.net/publication/30012773_Recursive_Wang_Tiles_for_Real-Time_Blue_Noise — recursive Wang tiles; https://www.researchgate.net/publication/220184091_An_alternative_for_Wang_tiles_Colored_edges_versus_colored_corners — corner tiles.

## How do crowd games keep guests from walking single file on open ground?

Planet Coaster 1's guests follow a flow field for each goal. Lanes form when crowds meet head on, and guests stuck in a head-on meeting shrink their collision radius over time until they fit through. Continuum Crowds (Treuille, Cooper, and Popović) makes lanes form with no explicit avoidance, through a cost that depends on velocity. Emerson's flow-field tiles for Supreme Commander 2 cache fields per tile. The Explicit Corridor Map plans a route anywhere between the shortest path and the path with most clearance, and giving each agent its own offset inside the corridor is the cheap way to avoid single file. Mononen's advice is to pull the route straight through its portals, then leave hugging walls and avoiding others to steering.

For this slice, the cheapest fit is to route on the derived graph and give each guest a fixed sideways offset derived from its key. Spread the offset across the walkable width at the guest's place, and vary it smoothly along the walk. If the offset only affects drawing and picking, it needs no simulation state. If guests' positions ever feed the simulation, the offset must stay deterministic.

Rejected: flow fields per goal, because they cost a field per goal over the whole ground and give guests no structure on plazas, as Planet Coaster shows. Crowd avoidance in the simulation, because the slice leaves crowding out.

Sources: https://www.gamedeveloper.com/audio/game-design-deep-dive-creating-believable-crowds-in-i-planet-coaster-i- — Planet Coaster's flow fields and lanes; https://grail.cs.washington.edu/projects/crowd-flows/continuum-crowds.pdf — Continuum Crowds; https://www.semanticscholar.org/paper/Crowd-Pathfinding-and-Steering-Using-Flow-Field-Emerson/a5334e28ea30b1865b87c3881a310b4f702beac3 — flow-field tiles; https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.SoCG.2016.70 — routes inside an Explicit Corridor Map; http://digestingduck.blogspot.com/2010/03/simple-stupid-funnel-algorithm.html — leaving avoidance to steering.

## How should paving shapes be authored and edited?

Cities: Skylines 2's surfaces are polygons of nodes clicked onto the ground, down to 0.75 m apart, filled with a material, and edited by dragging and adding nodes. Vanilla refuses overlapping surfaces. Players turn that check off with Anarchy precisely so they can layer surfaces, with render priority choosing which shows on top. The community dislikes surfaces only as a stand-in for brush-painted ground textures. As layered shapes they work. Planet Coaster 2's stamp delete cuts holes, but into the merged surface, so the hole is lost as an object. Figma gives every vertex its own corner radius, dragged or typed, with zero meaning sharp. Its boolean operations keep the original shapes editable instead of merging them.

For this project, that suggests paving as layered shapes:

- A path is a line with one width.
- An area is a polygon of straight edges with a corner radius at each vertex. A square rounded fully is a circle.
- A stamp is a ready-made area.
- A clearing is a hole that stays its own shape. The player can move, reshape, and delete it, and it never consumes what lies under it.

Shapes stack with the newest on top, and overlap is always allowed. The paved ground is derived from the stack every time and never saved, so deleting a shape removes exactly that shape. Straight edges with corner radii keep editing local: dragging one vertex never reshapes another edge.

Rejected: smooth curves through every area vertex, because dragging one vertex reshapes its neighbors' edges, the finickiness to avoid. Bezier bend handles on edges, because they give the most control and need the most explaining. Refusing overlap, as vanilla surfaces do, because layering is the point. Holes cut into a merged surface, because the hole is lost as an object.

Sources: https://cs2.paradoxwikis.com/Assets:_Import_and_Setup_Surfaces — surfaces as node polygons, node spacing, render priority; https://github.com/yenyang/Anarchy — overlap checks turned off; https://help.figma.com/hc/en-us/articles/360050986854-Adjust-corner-radius-and-smoothing — corner radius per vertex; https://help.figma.com/hc/en-us/articles/360039957634-Edit-vector-layers — vector edit mode; https://www.planetcoaster.com/en-US/player-guides/deinen-park-bauen — Planet Coaster 2's stamp delete.

## How do Cities: Skylines' road tools model nodes and segments, and what does that suggest for paths built from nodes?

Both Cities: Skylines games save networks as nodes and segments. A segment is a Bezier curve between two nodes, with two control points shaping it. It carries a road type, which lays its lanes. A node is an end, a junction, or a middle node that joins two segments of the same type. Cities: Skylines 1 also inserts a middle node every 96 m on long roads. Nodes count against hard limits of 32,768 nodes and 36,864 segments, so players prune unneeded ones with mods. Pathfinding runs on the lanes laid along segments and joined at nodes. Middle nodes still change behavior: cars change lanes, and pedestrians step from a footpath to a sidewalk, only at nodes. Shaping detail and routing are coupled, which is why pruning nodes changes traffic.

The popular mods all manipulate nodes and segments:

- Move It exposes a segment's two end nodes and two control points for dragging, with snapping to a straight line.
- Network Multitool adds, removes, merges, and splits nodes. It also puts a node where two segments cross, arranges nodes on a circle, evens the slope between nodes, builds parallel roads, fits a curve between two segments, and reverses a segment.
- Node Controller changes how a node joins its segments: as a middle, bend, stretch, crossing, or end node. Each segment end's offset, shift, rotation, slope, and stretch can be adjusted. Most of this is the node's visual footprint.
- Intersection Marking Tool draws markings and fillers, paved or grass areas, between points on a node's segment ends. A filled area belongs to a node and follows it.
- Intersection assets are ready-made arrangements of nodes and segments. Once placed, they become ordinary roads, and each piece can be removed or changed on its own.

Planet Coaster 2 is the contrast. Its line, stamp, and draw tools all add to one merged path surface. Overlaps join into a single shape, and stamp delete cuts shapes out of it. The merged result is the real shape, and it shows when something is deleted. Cities: Skylines never has this problem, because every piece of road surface belongs to exactly one segment or one node. When two roads overlap, a node where they cross resolves them. Their surfaces never merge.

The lesson for this project is ownership. Every piece of paving belongs to one saved shape. A stamp is ordinary once placed. No surface is owned by a merge, so deleting removes exactly the piece the player sees. Layered shapes keep that ownership without shared vertices (see "How should paving shapes be authored and edited?").

Cities: Skylines' lesson also gives two layers. The control layer holds every vertex the player edits, including those that exist only for shape. The simulation graph never routes on them: it comes from the paved ground (see "How is a route graph derived from paved ground of any shape?"). Adding a vertex to shape a path then changes nothing about routing or the graph's size.

Rejected: a saved merged surface, as in Planet Coaster 2, because overlaps become hidden shapes that show on delete. Every control node as a simulation node, as Cities: Skylines 1's middle nodes are, because shaping detail inflates the graph and changes behavior.

Sources: https://steamcommunity.com/sharedfiles/filedetails/?id=2290983677 — Node Controller's node types and segment-end controls; https://www.pcgamer.com/perfect-your-cities-skylines-roads-with-the-node-controller-mod/ — what node shaping is used for; https://steamcommunity.com/sharedfiles/filedetails/?id=1999726060 — Move It's segment control points; https://www.nexusmods.com/citiesskylines/mods/124 — Network Multitool's node operations; https://www.nexusmods.com/citiesskylines/mods/160 — Intersection Marking Tool's fillers belonging to nodes; https://steamcommunity.com/sharedfiles/filedetails/?id=2712549268 — node and segment limits; https://steamcommunity.com/app/255710/discussions/0/3113655160533387500/ — middle nodes, lane changes at nodes, pruning; https://cs2.paradoxwikis.com/Creating_and_sharing_intersection_assets — intersection assets; https://steamcommunity.com/app/255710/discussions/0/3011179244456942360 — placed intersections becoming ordinary roads; https://cs2.paradoxwikis.com/Assets:_Common_Asset_Principles — Bezier segments and pedestrian lanes in Cities: Skylines 2; https://github.com/CitiesSkylinesMods/TMPE/wiki/Nodes,-Segments,-Lanes — nodes, segments, and lanes; https://steamcommunity.com/app/2688950/discussions/0/4634861989070424766/ — Planet Coaster 2 paths fusing into odd shapes; https://www.planetcoaster.com/en-US/player-guides/deinen-park-bauen — Planet Coaster 2's stamp and stamp-delete tools.
