# Milestone: Layered Dependencies

Slice: none

## Summary

layered-dependencies makes decision 0027 enforceable before any code is restructured under it. It declares the layers once, adds a check in ctest that fails when a file includes a header from a unit it may not depend on, removes the one include in the tree that runs the wrong way, and gives every new plan a Placement section that a hook requires and the reviewer judges. It comes first because rules on paper did not hold: after 0027 was accepted, the explained-food features grew src/app/main.cpp from 635 to 742 lines with new concerns. The check and the hook stop that kind of drift at the point of change. They cost little, since the tree has one violation today.

## Acceptance criteria

- The layers are declared in one place in the repository, as units ordered by layer, lowest first:
  - core;
  - the sim's base, meaning the sim root's files other than sim/park_schema;
  - sim/medium;
  - sim/park;
  - sim/routes;
  - sim/operations;
  - sim/guests;
  - sim/park_schema;
  - tools, render, legible, and scenarios, together in one layer;
  - app.

  Each unit is a path under src, matched by whole components: a directory, which holds every file beneath it, or a file named without its extension, which holds its header and its source, as sim/park_schema holds park_schema.h and park_schema.cpp. A file belongs to the most specific unit that holds it.
- A file in a unit may include headers of its own unit and of any unit in a lower layer, and nothing else. A unit may not include a sibling in its own layer or anything above it.
- Tests follow their module. A file or header under tests/<module>, its support/ directory included, counts as part of that module. It may include anything in the module and anything the module may include, whichever of the module's units it tests. tests/integration may include every module.
- A ctest test, "layer check passes on the tree", runs the check over the repository. The check scans every .h and .cpp file under src and tests. It resolves each include as the private header check does, and an include naming no file inside the repository is not a dependency.
- The check fails with one line per violation, naming the including file, its unit, the header, and the header's unit. It also fails, naming the file, when a file under src or tests belongs to no unit, so a new module cannot bypass the table. Planted trees in tests/checks prove each of these, as the private header check's cases do.
- The tree passes both the layer check and the private header check. sim/park includes nothing from sim/park_schema. makeNewPark stays in sim/park, since it places the park's private components, and it takes the park's schema from its caller. The world it gives is unchanged: tests/parks/new.park is still its save with seed 1.
- planning-features's PLAN.md format has a Placement section. For each behavior the plan adds, it names the module and component that own it and says why, citing 0027. The reviewer's PLAN.md lens and code lens each check placement against 0027.
- The pre-commit hook refuses a commit that adds a PLAN.md with no Placement section, naming the file. A commit that adds a plan with the section, or that changes a plan already in the tree, passes. The script the hook runs is a CMake script tested by planted cases in tests/checks.

## Medium

This milestone introduces, samples, and emits no fields or flows. The check governs includes between units, and the placement step governs plans. Neither is an interaction between things in the park.

## Dependencies

- Decision 0027 accepted: met.
- cmake/check_private_headers.cmake and tests/checks, whose include resolution and planted-tree tests the layer check and the placement script follow: met.
- The pre-push hook runs ctest on linux-debug, so the layer check gates every push once it is registered: met.

## Core feature

layer-check is the core. It makes the layer order a property of the build, catching any backward or sibling include at the next ctest run and at every push. On its own it guards the whole tree, including principle 10's independence of the simulation from rendering, before the placement step exists.

## Features

1. `layer-check`: the declared layer table, the check and its ctest registration, the planted cases in tests/checks, and makeNewPark taking the park's schema from its caller, so that sim/park no longer includes sim/park_schema and the tree passes. Depends on: none.
2. `placement-step`: the Placement section in planning-features's PLAN.md format, the reviewer's 0027 lens for plans and code, and the pre-commit refusal of a new PLAN.md without the section, with its CMake script and planted cases. Depends on: none, though it lands second so the reviewer's lens can cite the layer check.

## Deepening candidates

- Link-level agreement: a check that each CMake target links exactly the modules its unit may include, so a target cannot link a module the table forbids or omit one its includes need.
- A placement report: the layer check prints each unit's actual dependencies when asked, so a review can see where a unit's reach is widening before it becomes a violation.
- A hook test: a ctest that commits in a scratch repository through .githooks/pre-commit and checks the placement refusal end to end, once the hook's temporary paths survive Git for Windows's conversion of path-like arguments.

## Open questions

- Where the layer table lives, whether as a CMake list beside the check or a plain text file the check reads. Resolved when layer-check is planned, by which the check and the planted cases read most simply.
- Whether callers of makeNewPark pass makeParkSchema() themselves, or a function at sim/park_schema's level does it for them. Resolved when layer-check is planned, by counting callers, which today are in src/app, src/scenarios, tests/render, tests/scenarios, and tests/sim.

## Research notes

- Chromium's checkdeps needs specific_include_rules for single files. One table of units, each a directory or an extensionless file path matched by whole components, covers sim/park_schema without regular expressions.
- Placement is checked only on added PLAN.md files, so plans written before the rule need no exemption list.

Depth is in RESEARCH.md.
