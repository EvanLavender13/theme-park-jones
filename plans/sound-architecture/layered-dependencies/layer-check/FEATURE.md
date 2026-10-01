# Feature: Layer Check

## Summary

layer-check makes decision 0027's layers a property of the build. cmake/layers.txt declares the units of code in layers, lowest first. cmake/check_layers.cmake scans every .h and .cpp file under src and tests and fails, naming each violation, when a file includes a header from a unit its own unit may not depend on, when a file belongs to no unit, or when the table names a unit that holds no file or names one twice. ctest runs it on the repository as "layer check passes on the tree". The tree has one violation today, sim/park reaching up to makeParkSchema. This feature removes it: makeNewPark stays in sim/park and takes the schema from its caller, and an overload at sim/park_schema's level hands it the park's schema.

## Acceptance criteria

- The repository passes. cmake/layers.txt declares these layers, lowest first:
  - core;
  - sim/*, the files directly in src/sim;
  - sim/medium;
  - sim/park;
  - sim/routes;
  - sim/operations;
  - sim/guests;
  - sim/park_schema;
  - tools, render, legible, and scenarios, together in one layer;
  - app.

  "layer check passes on the tree" runs cmake/check_layers.cmake with ROOT set to the repository and LAYERS set to cmake/layers.txt, and it exits with status 0. "private header check passes on the tree" still passes.
- A file belongs to the most specific unit that holds it. A unit is a path under ROOT/src. It holds every file beneath that path as a directory, compared by whole path components, and every file whose path, with its last extension removed, is that path. A unit written as a path followed by /* holds only the files directly in that directory, not those in its subdirectories, so a new subdirectory is in no layer until the table names it. When several units hold a file, a file unit is the most specific, then the directory unit with the most path components, and of two for the same directory the /* unit. A unit's module is its path's first component.
- An include is allowed exactly when one of these holds, and every other include is reported:
  - the header is in the including file's own unit;
  - the header's layer is lower than the including file's;
  - the including file is a test of a module, and the header is in one of that module's units or is a test of that module;
  - the including file is under ROOT/tests/integration.
- A file under ROOT/tests/<m>, where m is some unit's module, is a test of module m. It sits in the highest layer that holds one of m's units. A file under ROOT/tests/integration sits above every layer. Any other .h or .cpp file under ROOT/src or ROOT/tests is in no layer and is reported. A file in no layer is reported only as a file: its own includes are not checked, and it is not reported as the target of each include of it.
- Includes are read and resolved as the private header check reads and resolves them. That means:
  - the same scan scope and include forms, with commented-out includes ignored;
  - each include resolved against the including file's directory, every directory above it up to ROOT, and ROOT/src, each existing file it names checked;
  - each pair of file and header reported once.

  A named file that is not under ROOT/src or ROOT/tests is not a dependency.
- Each finding is one whole, unprefixed line on standard error, with paths relative to ROOT, in exactly one of these forms. Units are written relative to ROOT too: a unit of the table as src/<unit>, such as src/sim/park or src/sim/*, a test of module m's unit as tests/m, and a file under ROOT/tests/integration's as tests/integration.
  - `<file> in <unit> includes <header> in <unit>, which is not below it`
  - `<file> is in no layer`
  - `<unit> holds no file`
  - `<unit> is listed more than once`, once however many times the unit is listed

  The check exits with status 0 exactly when it reports no finding. Run without ROOT or without LAYERS, it fails naming the missing variable. Given a LAYERS file it cannot read, it fails naming the file.
- The table is read one layer per line, lowest first, with the units of a layer separated by blanks. Blank lines and lines whose first non-blank character is # are not layers.
- makeNewPark(schema, seed), in sim/park/intent.h, gives the new-park template with the given schema and seed. makeNewPark(seed), in sim/park_schema.h, gives a world equal to makeNewPark(makeParkSchema(), seed). tests/parks/new.park is still the save of makeNewPark(1).

## Medium

None. The check governs includes between units of code, and makeNewPark's change moves where the park's schema is chosen, not what the park holds.

## Principle checks

- Principle 10: the simulation runs independently of rendering. The tree passing the layer check with sim below tools, render, legible, scenarios, and app means no file in src/sim includes a header from any of them.
- Principle 6: the private header check still passes on the tree. makeNewPark keeps placing the park's private components from inside sim/park.
- Principle 1: the new park's world is unchanged. Its save is still tests/parks/new.park, so nothing derived or authored moved with the schema.

## Spec changes

docs/conventions.md, after the paragraph on private headers in "Files and includes", gains:

> Code is arranged in layers (decision 0027), declared in cmake/layers.txt, one layer per line from the lowest. Each unit in a layer is a path under src: a directory, holding every file beneath it; a directory followed by /*, holding only the files directly in it; or a file named without its extension, holding the header and source of that name. A file belongs to the most specific unit that holds it. A file may include headers of its own unit and of units in lower layers, never of a sibling in its own layer or of a unit above it. A module's tests, under tests/ in the directory named for it, count as part of that module and may include any of its units and anything below the highest of them; tests/integration may include anything. A file in no unit fails the check, so a new module or test directory is added to the table before it lands. cmake/check_layers.cmake scans src and tests for includes that break the rule, and runs in ctest as "layer check passes on the tree".

src/sim/park/SPEC.md, the paragraph on the new park, changes its first sentence from "makeNewPark(seed) gives a world with makeParkSchema's schema, the seed, ..." to:

> makeNewPark(schema, seed) gives a world with the given schema, the seed, tick 0, next key 3, and resolution pending, holding exactly two entities.

The rest of the paragraph stays, and its last sentence becomes: "tests/parks/new.park is the save of makeNewPark(seed), the park's schema's overload in sim/park_schema.h (sim/SPEC.md), with seed 1."

src/sim/SPEC.md, after the sentence ending "in a written order." that introduces makeParkSchema, gains:

> sim/park_schema.h also gives makeNewPark(seed), which is makeNewPark(makeParkSchema(), seed): the new-park template (sim/park/SPEC.md) with the park's schema.

## Files affected

- Create: cmake/layers.txt
- Create: cmake/check_layers.cmake
- Modify: docs/conventions.md
- Modify: src/sim/park/SPEC.md
- Modify: src/sim/SPEC.md
- Modify: src/sim/park/intent.h
- Modify: src/sim/park/intent.cpp
- Modify: src/sim/park_schema.h
- Modify: src/sim/park_schema.cpp
- Modify: tests/sim/guests/walks_test.cpp, tests/sim/park/validity_test.cpp, tests/sim/routes/connections_test.cpp, tests/sim/routes/previous_networks_test.cpp (an include of sim/park_schema.h, so their existing calls to makeNewPark(seed) still compile)
- Create (test pass): tests/checks/layer_check_test.cmake
- Modify (test pass): tests/checks/CMakeLists.txt, tests/sim/park/new_park_test.cpp

## Dependencies

- cmake/check_private_headers.cmake, whose scan and include resolution the check repeats: met.
- tests/checks's planted-tree pattern: met.
- CMake 3.28 for cmake_path: met.

## Out of scope

- The Placement section and its hook, which are the placement-step feature.
- Link-level agreement between CMake targets and the table, and a report of each unit's actual dependencies: deepening candidates in MILESTONE.md.
- Cycles among files within one unit.

## Open questions

None.
