# Capability: Sound Architecture

## Summary

sound-architecture keeps the code's structure sound as features land. It owns decision 0027 and how the whole codebase follows it. Under 0027:

- modules form layers that depend only downward;
- each concern is a component that owns its state;
- an entry point only composes;
- derived state has one owner;
- every behavior a plan adds goes in the component that owns it.

It owns three things:

- the mechanical check that enforces the layers;
- the placement step that every plan passes;
- restructuring the code that predates 0027, starting with src/app/main.cpp.

Entity encapsulation (principle 6, decision 0016) stays with shared-medium. This capability owns the structure of the code around and between entities, not the rule between them.

This matters because it keeps the next feature as cheap to place and to check as the last one. Each concern has one obvious home, and a change reaches only a small part of the code. You can build and run a component through its header, without the rest of the program.

The capability has more work whenever a module or component grows past one concern, and it never finishes.

## Foundation criteria

The layers are declared once in the repository. From the bottom up, they are:

- core;
- sim;
- tools, render, legible, and scenarios, which don't depend on each other;
- app.

Within sim, the order from the bottom up is medium, park, routes, operations, then guests. The sim root's files sit below all of them, except sim/park_schema, which sits above them.

Under tests, a module's directory counts as part of that module, including its support/ directory. tests/integration may include any module. The check's planted trees are built outside the tree it scans.

A check in ctest fails when a file under src or tests includes a header from a layer that its own layer doesn't depend on. The failure names the file, the header, and both layers. A planted violation in tests/checks proves that the check fails, as the private header check's planted violation does for that check.

The whole tree passes the check, and the private header check still passes. park/intent.cpp no longer reaches up to makeParkSchema. makeNewPark stays in sim/park, because it places the park's private components, and the composition level hands it the schema.

Every PLAN.md written after the first milestone has a placement section. The section names the module and component that own each behavior the plan adds. If a plan or diff puts a behavior in a component that doesn't own it, the reviewer cites 0027.

src/app/main.cpp only composes. It parses the options, runs the headless hash path, builds the application, and runs it. Each concern listed in 0027's context has its own component behind its own header.

When the world is replaced, the change reaches every piece of state derived from it through one path. The choice of what to rebuild needs no window.

Every acquired platform resource has one owner that releases it. Resources are released in the reverse of the order they were acquired.

The restructure changes no behavior. The app's existing tests pass unchanged, and src/app/SPEC.md changes only where it describes structure.

## Medium

This capability adds, samples, and publishes no fields or flows. It shapes the code that every other capability's fields and flows are written in.

Every capability's plans pass this capability's placement step, and every capability's code is subject to the layer check.

Restructures move code that other capabilities own. They don't change its behavior or its module's public contract. If a public header changes, update that module's SPEC.md in the same work.

## Principles

- Principle 10 says the simulation runs independently of rendering. The layer check makes that a property of the build: no file in sim can include a header from render, tools, or app.
- Principle 1 says visible state is derived and is never the source of truth. It is at risk in the app's caches of what was last drawn. Today those caches are reset by hand when the world is replaced. A single owner and a single invalidation path protect it. Check it by playing the game, as docs/testing.md says.
- Principle 6 belongs to shared-medium, and the private header check checks it. The layer check sits beside that check and doesn't replace it.

## Dependencies

- Decision 0027 accepted: met.
- The include scan in cmake/check_private_headers.cmake, which the layer check follows: met.
- tpj_park_files, the precedent for app code built as a library: met.
- planning-features and the reviewer agent, which the placement step changes: met.

## Foundation

The foundation makes 0027 enforceable and then applies it to the worst case.

The rules come first. They stop new drift at once and cost almost nothing, because the tree already nearly passes.

The app restructure follows, under those rules. It tries the rules on the code that prompted them. If 0027 can't produce a sound application layer, the decision record is what needs rework.

## Milestones

1. `layered-dependencies`: the rules of 0027, made checkable. It includes:
   - the declared layers;
   - the layer check in ctest, with its planted violation;
   - removal of park/intent.cpp's upward include, by having the composition level hand makeNewPark the park's schema, so the tree passes;
   - the placement section in planning-features's PLAN.md format;
   - the reviewer's 0027 check.

   Depends on: 0027 accepted.

2. `composed-app`: src/app restructured into components under 0027. The pieces are:
   - a composing entry point;
   - a park session, which owns the world, the command queue, and the park file flow, and is the one place where the world is replaced;
   - a scene sync, which owns every cache derived from the world, including the preview and food overlay meshes;
   - components for input, picking under the cursor, the tooling UI with the graph view, and the frame clock.

   The Debug panel's park numbers come from a park summary in legible. Platform resources get single owners, and the logic that needs no window is tested through its headers. Depends on: milestone 1.

## Deepening candidates

- Audit render/renderer.cpp for concerns that deserve their own components. At 608 lines, it is the next largest file. Gated on: an audit showing it holds more than one.
- Levelization within a module: a check that a module's files form no include cycle among themselves, as Lakos requires of components. Gated on: a cycle found in review, or a module large enough that one is likely.
- Restructure src/scenarios/main.cpp so it only composes. Beside its composition, it holds file reading, output comparison, timing reports, and slice-park writing. Gated on: composed-app, whose shape it follows.

  Its file reading also has a bug. It copies the source stream's buffer into another stream and only then checks the source stream's state. If a read fails after the open, as it does for a directory, the result is empty text instead of an error. Read through the file's own stream instead, as src/bench/park_file.cpp does.
- A standing architecture audit: at the close of each milestone in any capability, run the reviewer over the whole tree against 0027, not over one diff.
- A signal for files that outgrow one concern, such as a line count past which the reviewer must justify the file. Gated on: drift that the function size limits and the placement step miss.
- Each module's public surface, listed, so the module's SPEC.md names exactly which headers other modules may include.
- Values that other modules repeat as literals, read by name instead:
  - src/app/application.cpp's PARK_SIZE_METERS copies sim/park's PARK_SIZE;
  - render's OVERLAY_FULL in food_overlay.h is a meal's relief, written as 0.5.

## Open questions

None. composed-app settled the two questions it held (plans/sound-architecture/composed-app/RESEARCH.md):

- The app keeps its own loop around an Application, instead of using SDL3's main callbacks.
- The Debug panel's park numbers move to a park summary in legible, and the graph view stays an app component.

## Research notes

- Chromium's checkdeps is the model. It is a script that tests every include against declared rules, and it has the same shape as the existing private header check.
- Clang's layering check needs module maps generated by Bazel, and it doesn't work with GCC. It isn't an option here.
- Lakos's levelization techniques remove park/intent.cpp's upward include. Moving makeNewPark up a layer would break the private header check, because makeNewPark places park-private components. The schema is passed down to it instead.
- Two patterns shape composed-app: the composition root, and functional core with imperative shell. Wiring sits in one place, logic that needs no window sits behind headers, and SDL and ImGui stay at the edges.
- If each derived cache stores a generation that changes when the world is replaced, the caches get one invalidation path.

The details are in RESEARCH.md.
