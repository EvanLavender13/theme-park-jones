# Capability: Inspectable Simulation

## Summary

inspectable-simulation lets the people building the game see and check what the simulation holds, without running the app. Its main user is the agent that writes the code. The agent reads images through the Read tool and exact values as text. The user looks at the same pictures.

It owns two things:

- plots: top-down PNG images of chosen kinds of simulation state, drawn from the sim's own data rather than through the renderer, each with a text listing of what it drew;
- checks: headless searches for broken geometry that report each problem with its location and never gate the build.

It is an engineering capability (decision 0028). A player would never notice it missing. It decides how quickly a geometry bug is seen and understood, and whether a fix can be shown working.

The capability is the developer's counterpart to legible-simulation. legible-simulation shows the player why the park behaves as it does, inside the game. This capability shows a developer what the sim actually holds, outside it. Timing what the game does belongs to scalable-runtime, not here.

It deepens whenever a new kind of state needs to be seen, or a new kind of breakage needs to be found, and it never finishes.

This replaces the scratch plotter written during predictable-editing: build/predict-check/parkplot.cpp, the drag check in build/predict-check/jank.cpp, and build/edge-check/plot2.py. It also takes over plans/BACKLOG.md's entry for a top-down plot tool.

## Foundation criteria

A library, tpj_inspect_lib, lives in src/inspect with its own SPEC.md and its own place in cmake/layers.txt, above sim and tools. It builds only with the windowed application, because it writes through SDL.

Each kind of state it can draw has its own producer, chosen by name. A producer draws through a narrow drawing interface of filled polygons, polylines, points, and labels, and never writes pixels itself. A new kind of state is one new producer and touches no other producer.

plotWorld takes any world, the producers to draw, a window, and a scale, and writes two files:

- a PNG, saved with SDL_SavePNG, which the Read tool can show;
- a text listing beside it, with each drawn element's entity key, its coordinates, and its numbers, such as a corner's radius and its limits.

A scratch program can link the library and call plotWorld on a world it built, including a preview candidate.

The PNG labels each element with its entity key, so the picture and the listing line up. It draws a grid with lines every 1 m and every 10 m. By default the window fits everything drawn. A window around an entity's key or a ground point with a radius zooms in.

A filled shape that crosses itself is drawn as it is, with nonzero winding, so a crossed piece looks crossed.

The first producers draw the paved surface: its pieces, its edge lines, and each segment's ground line.

The executable tpj_inspect is a front end over park files. tpj_inspect plot draws a park file with the producers named on its command line. tpj_inspect check runs named checks on a park file. It prints each problem with its kind, the keys involved, and its location, and it can write a plot with each problem marked.

The first checks find two kinds of broken paving: a piece that crosses itself, and a stretch of a segment's ground line that no piece covers. They run in two ways:

- on the surface as the park file holds it;
- the drag check, which drives the edit tool's public pointer calls as a player's mouse would. It drags every radius, edge, side, and taper handle the edit tool offers far along its direction and far against it, commits each edit the world accepts, and checks the surface after each one.

The scratch drag check's drawn shapes become park files that the check runs on.

Plots and checks are deterministic. The same world gives the same PNG, listing, and check output on every build, so outputs from before and after a change can be compared.

No test, hook, or ctest check runs tpj_inspect or fails on what it finds. It is a test utility, checked by running it (docs/testing.md). If a check finds a problem, report it to the capability that owns that geometry. Don't fix the problem in this capability's work.

A plot is a named way to confirm a criterion, beside play, a capture, a scenarios comparison, and a listed test. CLAUDE.md says how to run it. planning-features, implementing-features, and the reviewer agent list it among the means a criterion may name.

The capability's work changes no behavior. Existing tests pass unchanged, tpj_scenarios's windows-debug output is identical before and after, and no module's public contract changes.

## Medium

This capability adds, samples, and publishes no fields or flows. It reads what other capabilities' state looks like, through their public headers, and draws it.

- It reads the paved surface, the segments, the corners, the edges, and the tapers from sim/park's public headers. effortless-building and navigable-networks own that geometry.
- The drag check edits its own copy of a world through the tools module's public pointer calls, which effortless-building owns. It never saves that copy or writes the park file it loaded.

## Principles

- Principle 6 says nothing reads another thing's internals. inspect reads only public headers, and the private header check and the layer check enforce that. If a producer needs state that no public header gives, raise it with the user. Don't include a private header.
- Principle 10 says the simulation runs independently of rendering. Nothing in sim depends on inspect, and the layer check enforces it.
- Principle 1 says visible state is derived and is never the source of truth. Plots and listings are derived from a world and are never loaded back. The drag check never writes a park file.
- Decision 0037 limits the test suite. inspect gets no tests of its own, and its checks report without gating.

## Dependencies

- SDL 3.4's SDL_SavePNG, in the pinned release-3.4.16: met.
- SDL's software renderer and debug text, SDL_CreateSoftwareRenderer and SDL_RenderDebugText, for labels: met.
- The layer table and its check, cmake/layers.txt and cmake/check_layers.cmake: met.
- sim/park's public surface: parkSurface, pieceCorners, edgePoints, parkSegments, parkCorners, parkEdges, and parkTapers: met.
- The tools module's public pointer calls and edit handles: met.

## Foundation

The foundation is the scratch tool made permanent and easy to read. The first milestone gives plots of the paving, from code and from park files. The second gives the checks that found predictable-editing's bugs, with their findings drawn on the same plots.

Plots come first because they produce value alone. They are what the next paving bug will be debugged with. The checks are built on the plots' producers and drawing.

## Milestones

1. `paving-plots`: tpj_inspect_lib and tpj_inspect plot, with the producer and drawing design, the PNG and its listing, labels, the grid, the window choices, and the paving's three producers. It also makes a plot a named means in CLAUDE.md and the planning skills. Depends on: none.
2. `paving-checks`: tpj_inspect check, with the crossed-piece and uncovered-ground-line checks, run on the surface as loaded and under the drag check, the drawn shapes as park files, and problems marked on a plot. Depends on: milestone 1.

## Deepening candidates

- Guests on the paving: draw each guest's position and the line it walks, and check that every guest stands inside the paving's outline.
- Ground-field entries: draw a ground field's entries and a sample's radius.
- The route network: draw network places, their links, and route distances.
- Field and flow values: shade a field's value or a flow's amount along the routes.
- Two worlds in one plot: draw a world and its preview candidate, or a park before and after an edit, in two colors.
- Edits from the command line: draw a path or drag a handle from tpj_inspect's arguments, so a case needs no scratch program. Gated on: scratch programs still being written for cases that a park file can't hold.
- An in-app backend: the same producers drawn as debug overlays in the running game. It reaches into render and the app, which other capabilities own. Gated on: a need that headless plots don't meet.
- PNG captures: have the app's --capture write PNG with SDL_SavePNG, so the agent can view captures without converting them.

## Open questions

- Should a piece that touches itself at a single point, like a bow tie, count as broken? OGC says yes and Esri's model says no. paving-checks resolves it by what the paving produces where two of its sides meet.

## Research notes

- Box2D and Recast separate what to draw from how to draw it, with one producer per kind of state, a narrow drawing interface, and flags to choose what is drawn. This capability copies that split.
- The Read tool shows PNG but not BMP, and the pinned SDL already writes PNG, so there is no new dependency.
- A small scanline fill with nonzero winding draws a crossed piece as it is. SDL's triangle renderer would hide the crossing.
- Brute-force crossing tests are enough at the paving's size. Like GEOS, a check reports each problem with its location.

The details are in RESEARCH.md.
