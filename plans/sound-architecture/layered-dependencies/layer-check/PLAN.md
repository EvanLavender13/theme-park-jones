# Implementation Plan: Layer Check

## Goal

Declare the layers in cmake/layers.txt, enforce them with cmake/check_layers.cmake in ctest, and remove sim/park's one upward include by having makeNewPark take its schema from its caller.

## Approach

makeNewPark(schema, seed) stays in sim/park, and an overload makeNewPark(seed) in sim/park_schema.h hands it makeParkSchema(), so callers are unchanged. The check is a CMake script run with cmake -P, shaped like cmake/check_private_headers.cmake. It reads the table named by LAYERS and gives each scanned file a unit and a layer. Then it resolves each include against the same bases as the private header check, tests each include against the rule, and prints one whole line per finding before failing.

## Placement

- The layer table is a file of its own, cmake/layers.txt, because it is the one declaration of the layers and is read by a check, not by the build.
- The layer check is a script of its own, cmake/check_layers.cmake, beside the private header check. It is a separate rule over the same scan, so either can change without the other.
- The new-park template stays in sim/park, in intent.h, because it places that unit's private components.
- The choice of the park's schema moves to sim/park_schema, the unit that assembles the whole park's schema, so no unit below it names it.

## Tasks

### Task 1: Write the layer rule into the conventions

Files:
- Modify: `docs/conventions.md`

Step 1: Add the paragraph in FEATURE.md's Spec changes for docs/conventions.md, with its exact text. It goes as a new paragraph directly after the paragraph that begins "A header in a directory named internal is private".

### Task 2: Update the sim specs

Files:
- Modify: `src/sim/park/SPEC.md:68`
- Modify: `src/sim/SPEC.md:13`

Step 1: In src/sim/park/SPEC.md, in the paragraph under "## The new park", replace the first sentence and the last sentence with FEATURE.md's exact text.

Step 2: In src/sim/SPEC.md, in the paragraph on component registration, insert FEATURE.md's sentence directly after the sentence that ends "commands, in a written order.", before "Park files are loaded with it".

### Task 3: Hand makeNewPark its schema

Files:
- Modify: `src/sim/park/intent.h:99-100`
- Modify: `src/sim/park/intent.cpp:4,49-50`
- Modify: `src/sim/park_schema.h`
- Modify: `src/sim/park_schema.cpp`
- Modify: `tests/sim/guests/walks_test.cpp`, `tests/sim/park/validity_test.cpp`, `tests/sim/routes/connections_test.cpp`, `tests/sim/routes/previous_networks_test.cpp`

Step 1: In src/sim/park/intent.h, replace the makeNewPark declaration and its comment with:

```cpp
// The new-park template with the schema and the seed, resolution pending: an entrance on the
// park's edge facing in, and a guest path running into the park from in front of it.
World makeNewPark(std::shared_ptr<const WorldSchema> schema, uint64_t seed);
```

Add `#include <memory>` to its system includes, keeping them sorted.

Step 2: In src/sim/park/intent.cpp, remove `#include "sim/park_schema.h"`, change makeNewPark's signature to match, and build the world with `World world(std::move(schema), seed);`. `<utility>` is already included.

Step 3: In src/sim/park_schema.h, after makeParkSchema's declaration, add:

```cpp
// The new park: makeNewPark's template with makeParkSchema's schema and the seed.
World makeNewPark(uint64_t seed);
```

Add `#include "sim/world.h"` and `#include <stdint.h>` to its includes, in their groups.

Step 4: In src/sim/park_schema.cpp, define it inside namespace tpj as `return makeNewPark(makeParkSchema(), seed);`. sim/park/intent.h is already included.

Step 5: Add `#include "sim/park_schema.h"` to each of the four test files, in its project include group in sorted order. Make no other change to them. Each one calls makeNewPark(seed), which sim/park/intent.h no longer declares.

Step 6: Build and run the sim tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: the build is clean, and every test passes, the new-park tests included.

### Task 4: Declare the layers and stub the check

Files:
- Create: `cmake/layers.txt`
- Create: `cmake/check_layers.cmake`

Step 1: Create cmake/layers.txt. It opens with comment lines, each beginning with #. They say:
- the file declares decision 0027's layers, one per line, lowest first;
- the units of a layer are separated by blanks;
- each unit is a path under src: a directory, a directory followed by /* for only the files directly in it, or a file named without its extension;
- cmake/check_layers.cmake enforces the layers, and docs/conventions.md states the rule.

Then the layers, one per line:

```
core
sim/*
sim/medium
sim/park
sim/routes
sim/operations
sim/guests
sim/park_schema
tools render legible scenarios
app
```

Step 2: Create cmake/check_layers.cmake. Its header comment states:
- what it fails on, which is the four findings in FEATURE.md, each in its exact report form;
- that paths are relative to ROOT;
- its usage, `cmake -DROOT=<tree> -DLAYERS=<table> -P check_layers.cmake`.

Its body is `cmake_minimum_required(VERSION 3.28)`, and then a check for each of ROOT and LAYERS that fails with `message(FATAL_ERROR "usage: cmake -DROOT=<tree> -DLAYERS=<table> -P check_layers.cmake")` when it is unset. After those comes the stub `message(FATAL_ERROR "check_layers.cmake is not implemented")`.

### Task 5: Test pass

Step 1: Dispatch the test-writer agent for this feature with FEATURE.md, docs/conventions.md, src/sim/park/SPEC.md, src/sim/SPEC.md, the public headers src/sim/park/intent.h and src/sim/park_schema.h, cmake/layers.txt, and the stub cmake/check_layers.cmake. It creates the following:
- tests/checks/layer_check_test.cmake, planting a tree and a table for each case, as tests/checks/private_header_check_test.cmake does;
- the "layer check passes on the tree" test and one test per case, registered in tests/checks/CMakeLists.txt;
- tests of makeNewPark(schema, seed) and makeNewPark(seed), added to tests/sim/park/new_park_test.cpp.

Step 2: Configure, so ctest sees the new tests, and run them.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#new_park_test]" && ctest.exe --preset windows-debug -R "check"`
Expected: the new-park tests pass. The private header check's tests pass. The cases that missing ROOT or LAYERS fails pass against the stub. "layer check passes on the tree" fails, and so do the case that an unreadable table fails naming it and every case that expects status 0 or a report line, since the stub fails before reading the table.

### Task 6: Read the table

Files:
- Modify: `cmake/check_layers.cmake`

Step 1: Replace the stub. Make ROOT absolute and normalized with no trailing separator, as check_private_headers.cmake does. Fail with `message(FATAL_ERROR "Cannot read the layer table <LAYERS>")` when LAYERS is not a readable file.

Step 2: Read LAYERS with file(STRINGS). Skip lines that are blank or whose first non-blank character is #. Number the rest from 0, lowest first. Split each into its units on runs of blanks. Record each unit's layer number and its module, which is the unit's first path component. A unit seen a second time adds the finding `<unit> is listed more than once` once, and keeps its first layer.

### Task 7: Place each file

Files:
- Modify: `cmake/check_layers.cmake`

Step 1: Glob the files as check_private_headers.cmake does: every .h and .cpp under ROOT/src and ROOT/tests, sorted.

Step 2: Give each file a unit label and a layer number.
- A file under ROOT/src: a unit holds it when ROOT/src/<unit> is a component prefix of the file (cmake_path IS_PREFIX with NORMALIZE), or when the file's path with its last extension removed (cmake_path REMOVE_EXTENSION LAST_ONLY) equals ROOT/src/<unit>. A unit ending in /* holds it instead exactly when the file's parent directory (cmake_path GET PARENT_PATH) equals ROOT/src/<unit without /*>. Of the units that hold it, a file unit wins, then the directory unit with the most path components, and of a plain and a /* unit for the same directory the /* unit. Its label is that unit and its layer is the unit's layer.
- A file under ROOT/tests/integration: label tests/integration, and a layer one above the highest.
- A file under ROOT/tests/<m>, where m is some unit's module: label tests/<m>, and the highest layer of m's units.
- Any other file is in no layer. Add the finding `<file> is in no layer`, with its path relative to ROOT.

Step 3: After placing every file, add `<unit> holds no file` for each unit of the table that holds no scanned file.

### Task 8: Check each include

Files:
- Modify: `cmake/check_layers.cmake`

Step 1: For each placed file, read and resolve its includes exactly as check_private_headers.cmake does. Use the same INCLUDE_LINE regular expression and the same bases: ROOT/src, the file's directory, and each directory above it up to ROOT. Each base that names an existing file which is not a directory is checked.

Step 2: For each named header under ROOT/src or ROOT/tests, take the header's label and layer as Task 7 gave them. Skip a header in no layer, since it is already reported as a file. Ignore a header outside ROOT/src and ROOT/tests. The include is allowed when any of these holds:
- the labels are equal;
- the header's layer is lower than the file's;
- the file's label is tests/<m> and the header's label is a unit whose module is m, or is tests/<m>;
- the file's label is tests/integration.

Otherwise add `<file> in <label> includes <header> in <label>, which is not below it`, with both paths relative to ROOT.

Step 3: Remove duplicate findings. Print each with `message(NOTICE ...)`, then fail with `message(FATAL_ERROR "Includes break the layers in <LAYERS>.")`. With no finding, end with `message(STATUS "<ROOT> follows the layers in <LAYERS>")`.

Step 4: Run the checks.

Run: `ctest.exe --preset windows-debug -R "check"`
Expected: every layer check case passes, "layer check passes on the tree" passes, and every private header check test passes.

### Task 9: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `scripts/cross-build-check.sh`
Expected: it passes with the scenarios' output unchanged, since the new park's world is unchanged.

### Task 10: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Build: Check that includes follow the layers`.
