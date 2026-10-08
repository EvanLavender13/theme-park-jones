# Research: inspectable-simulation

## How do engines and simulators draw their internal state for debugging?

Physics and navigation libraries separate what to draw from how to draw it. Box2D's b2Draw is an interface the game implements, with calls for polygons, solid polygons, segments, circles, and transforms. The world calls it for each thing it holds. A set of flag bits chooses which kinds of state get drawn: shapes, joints, bounding boxes, broad-phase pairs, centers of mass. Recast and Detour do the same with duDebugDraw. Its interface has only begin, vertex, and end over points, lines, triangles, and quads. Helpers for boxes, arcs, and arrows are written once on top of it. Engines supply their own backends, among them OpenGL, a display list, Torque, O3DE, OpenMW, and Unreal. O3DE adds a bounding-box filter so only a chosen volume is drawn, and it turns lines off by default because Recast's line output is noisy.

This suggests three pieces. Each kind of state gets its own producer, which says what to draw. A narrow drawing interface sits between the producers and the output. Flags choose the producers and a window chooses the area. With that split, a new kind of state is one new producer. The same producers could later drive an in-app overlay through a second backend.

Rejected: drawing through the game's renderer — it draws in perspective on the GPU and needs a window. A rendering bug could also hide a geometry bug, or look like one.

Sources: https://box2d.org/doc_version_2_4/classb2_draw.html — b2Draw's calls and flag bits; https://recastnav.com/structduDebugDraw.html — duDebugDraw's primitive interface; https://docs.o3de.org/docs/api/gems/recastnavigation/class_recast_navigation_1_1_recast_navigation_debug_draw — a backend with a bounding-box filter and lines off by default.

## Which image format, and how to write it?

The Read tool shows PNG and JPEG images to the agent but refuses BMP as binary. This was checked on build/corners-check.bmp. The scratch plotter went through Python and PIL for that reason. SDL 3.4.0 added SDL_SavePNG, and the project pins SDL release-3.4.16. An SDL surface needs no window, so a headless tool can fill a surface's pixels and save it as PNG with no new dependency.

Rejected: BMP — the agent can't view it. Python and PIL — a second language, outside the build, and not on the Windows toolchain. stb_image_write — a new dependency for something SDL already does. A hand-written PNG with stored, uncompressed deflate blocks — simple, but a whole park at 20 pixels per meter would be about 78 MB. SVG — browsers zoom it well, but the agent can't view it, and the agent is a main reader.

Sources: .cpm-cache/sdl3 include/SDL3/SDL_surface.h — SDL_SavePNG, available since SDL 3.4.0; https://flutter.googlesource.com/third_party/stb/+/refs/heads/master/stb_image_write.h — stb's PNG writer and its own deflate.

## How to draw filled shapes and lines into an image?

Software rasterizers such as AGG and FreeType fill a polygon of any shape with a scanline pass that accumulates each edge's signed coverage in each cell. The winding rule then decides what is inside. Nonzero winding fills a shape that crosses itself in a way that shows the crossing: the overlap fills, and nothing is silently untangled. A plain scanline fill with nonzero winding needs no triangulation and draws a broken piece as it is, which is the reason to plot. Antialiasing is optional at the zoom levels used for checking, around 20 pixels per meter. Supersampling a few rows per pixel is the cheap way to add it later. A thick line can be drawn as a filled quad per segment, with the same filler.

SDL's software renderer can also draw onto a surface without a window. SDL_RenderGeometry draws triangles and SDL_RenderLines draws one-pixel lines. It needs the caller to triangulate. A crossed quad drawn as a triangle fan comes out as a shape that doesn't show the crossing.

Rejected: SDL's software renderer for filled shapes — it needs triangulation, and it would misdraw the broken shapes the tool exists to show.

Sources: https://lists.freedesktop.org/archives/cairo/2007-July/011119.html — survey of polygon rasterizers, including FreeType's cell coverage; https://pdfium.googlesource.com/pdfium/+/chromium/6945/third_party/agg23/agg_rasterizer_scanline_aa.cpp — AGG's exact cell coverage; https://rasterizer.readthedocs.io/en/stable/algorithm.html — signed area and winding in a coverage rasterizer.

## What counts as broken geometry, and how to find it?

GEOS, Shapely, and PostGIS follow the OGC model. A ring is simple if it doesn't cross itself, except where it closes. A shape that touches itself at one point, like a bow tie, is invalid under OGC but valid under Esri's model, so the rule has to be chosen, not assumed. GEOS reports the first problem with a reason and a location, for example "Self-intersection" at a point. A location lets a picture mark the spot. The scratch check tested only that a four-cornered piece's opposite sides don't cross.

The paved surface is small: pieces have a handful of corners, and an area of interest has hundreds of pieces, not millions. A brute-force pair test over a piece's sides, or over nearby edge lines, costs nothing at that size. Bentley-Ottmann's sweep isn't needed.

Rejected: linking GEOS — a large LGPL library for a few checks. Bentley-Ottmann — needless at this scale.

Sources: https://shapely.readthedocs.io/en/2.0.6/reference/shapely.is_valid_reason.html — validity reasons with locations; https://postgis.net/docs/manual-3.1/ST_IsValidDetail.html — reason, location, and the self-touching flag; https://gdal.org/user/geometry_validity.html — OGC validity rules and how other models differ.

## How to stress an editor's handles to find broken shapes?

No published work on fuzzing the handles of a geometry editor turned up. The robustness literature names where geometry code breaks: degenerate and extreme cases, near-coincident points, and tangent contact. Its worst failure is silent bad output, a result the code accepts that is no possible shape. A validity check used as the oracle catches that.

The scratch check, build/predict-check/jank.cpp, worked this way. For each radius, edge, and width handle that the edit tool offers, it pressed the pointer on the handle and moved it 80 m along the handle's direction, then 80 m against it. It released the pointer and committed the edit if the world accepted it, then counted crossed pieces. It ran on four drawn shapes, on a junction case, and on any park file. It found the bugs that predictable-editing fixed. It predates taper handles, so it never dragged them. Driving the tools' public pointer calls, as a player's mouse does, means the check tests what the player can reach, not states the tools would refuse.

Sources: https://ftp1.cs.wisc.edu/pub/users/prem/for-prem/surveys/michelucci-robustness.pdf — failure modes of geometric programs; https://docs.safe.com/fme/2024.2/html/FME-Form-Documentation/FME-Transformers/Transformers/geometryvalidator.htm — repairs that themselves produce self-intersections and degenerate output.
