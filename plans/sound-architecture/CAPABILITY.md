# Capability: Sound Architecture

## Summary

sound-architecture deepens how well the code's structure holds up as features land. It owns decision 0027 and its practice across the whole codebase. Modules form layers that depend only downward, each concern is a component owning its state, an entry point only composes, derived state has one owner, and every behavior a plan adds is placed in the component that owns it.

It owns the mechanical check that enforces the layers, the placement step every plan passes, and the restructuring of code that predates 0027, starting with src/app/main.cpp. Entity encapsulation (principle 6, decision 0016) stays with shared-medium. This capability owns the structure of the code around and between entities, not the rule between them.

Its value is that the next feature is as cheap to place and to test as the last one. A concern has one obvious home, a change has a small reach, and a component can be tested through its header without the rest of the program. It deepens with every module and component that grows past one concern, and never finishes.

## Foundation criteria

- The layers are declared once in the repository: core; then sim; then tools, render, legible, and scenarios, which do not depend on each other; then app. Within sim, the order is medium, then park, then routes, then operations, then guests, with the sim root's files below them all except sim/park_schema, which sits above them. Under tests, a module's directory, its support/ included, counts as part of that module, and tests/integration may include any module. The check's planted trees are built outside the tree it scans.
- A check in ctest fails when a file under src or tests includes a header from a layer its own layer does not depend on, and names the file, the header, and both layers. A planted violation in tests/checks proves it fails, as the private header check's does.
- The whole tree passes the check, and the private header check still passes. park/intent.cpp no longer reaches up to makeParkSchema: makeNewPark stays in sim/park, since it places the park's private components, and the composition level hands it the schema.
- Every PLAN.md written after the first milestone has a placement section naming the module and component that owns each behavior it adds. The reviewer cites 0027 when a plan or diff places a behavior in a component that does not own it.
- src/app/main.cpp only composes. It parses the options, runs the headless hash path, builds the application, and runs it. Each concern 0027's context lists has its own component behind its own header.
- Replacing the world reaches every piece of state derived from it through one path, and the choice of what to rebuild can be tested without a window.
- Every acquired platform resource has one owner that releases it, in the reverse of the order they were acquired.
- The restructure changes no behavior. The app's existing tests pass unchanged, and src/app/SPEC.md changes only where it describes structure.

## Medium

This capability introduces, samples, and emits no fields or flows. It shapes the code every other capability's fields and flows are written in.

Every capability's plans pass its placement step, and every capability's code is subject to the layer check. Restructures move code that other capabilities own without changing its behavior or its module's public contract; where a public header changes, that module's SPEC.md changes in the same work.

## Principles

- Principle 10 says the simulation runs independently of rendering. The layer check makes that a property of the build: no file in sim can include a header from render, tools, or app.
- Principle 1 says visible state is derived and never the source of truth. It is at risk in the app's caches of what was last drawn, which today are reset by hand when the world is replaced. The single owner and single invalidation path carry it, and the test of what to rebuild checks it.
- Principle 6 is shared-medium's, checked by the private header check. The layer check sits beside it and does not replace it.

## Dependencies

- Decision 0027 accepted: met.
- The include scan in cmake/check_private_headers.cmake, which the layer check follows: met.
- tpj_park_files and tpj_app_tests, the precedent for app code tested through a library: met.
- planning-features and the reviewer agent, which the placement step changes: met.

## Foundation

The foundation is 0027 made enforceable and then applied to the worst case. The rules come first because they stop new drift at once and cost almost nothing, since the tree already nearly passes. The app restructure follows under those rules, and it proves them on the code that prompted them: if 0027 cannot produce a sound application layer, it is the record that needs rework.

## Milestones

1. `layered-dependencies`: the declared layers, the layer check in ctest with its planted violation, removing park/intent.cpp's upward include by having the composition level hand makeNewPark the park's schema, so the tree passes, the placement section in planning-features's PLAN.md format, and the reviewer's 0027 check. Depends on: 0027 accepted.
2. `composed-app`: src/app restructured into components under 0027. The pieces are a composing entry point; a park session that owns the world, the command queue, and the park file flow, and is the one place the world is replaced; a scene sync that owns every cache derived from the world, the preview and food overlay meshes included; and components for input, picking under the cursor, the tooling UI with the graph view, and the frame clock, with the Debug panel's park numbers from a park summary in legible. Platform resources get single owners, and the logic that needs no window is tested through its headers. Depends on: milestone 1.

## Deepening candidates

- render/renderer.cpp, at 608 lines the next largest file, audited for concerns that deserve their own components. Gated on: an audit showing it holds more than one.
- Levelization within a module: a check that a module's files form no include cycle among themselves, as Lakos requires of components. Gated on: a cycle found in review, or a module large enough that one is likely.
- src/scenarios/main.cpp restructured so it only composes. It holds file reading, output comparison, timing reports, and slice-park writing beside its composition. Gated on: composed-app, whose shape it follows.
- A standing architecture audit: the reviewer run over the whole tree against 0027 rather than one diff, at the close of each milestone in any capability.
- A signal for files that outgrow one concern, such as a line count past which the reviewer must justify the file. Gated on: drift the function size limits and placement step miss.
- Public surfaces listed per module, so a module's SPEC.md names exactly which headers other modules may include.

## Open questions

None. composed-app settled the two it held: the app keeps its own loop around an Application rather than SDL3's main callbacks, and the Debug panel's park numbers move to a park summary in legible while the graph view stays an app component (plans/sound-architecture/composed-app/RESEARCH.md).

## Research notes

- Chromium's checkdeps, a script testing every include against declared rules, is the model; it has the shape of the existing private header check.
- Clang's layering check needs Bazel-generated module maps and does not work with GCC, so it is not an option here.
- Lakos's levelization techniques remove park/intent.cpp's upward include. Escalating makeNewPark would break the private header check, since it places park-private components, so the schema is passed down to it instead.
- The composition root and functional core, imperative shell patterns shape composed-app: wiring in one place, logic without a window behind headers, SDL and ImGui at the edges.
- A generation that changes when the world is replaced, stored by each derived cache, gives the one invalidation path.

Depth is in RESEARCH.md.
