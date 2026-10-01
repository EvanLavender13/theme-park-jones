# Conventions

Naming and file layout, from SteelJones (decision 0015). Formatting is whatever clang-format produces from .clang-format; the language subset is not restricted.

## Naming

Types (classes, structs, enums, aliases) are nouns starting with uppercase: `OrbitCamera`, `CameraView`. Enums used as type tags get a `Kind` suffix.

Functions are verb phrases in camelCase starting with lowercase: `stepWorld`, `drawFrame`. Predicates start with `is`, `has`, or `should`.

Variables are nouns in camelCase with a lowercase first letter. Use descriptive names; short names only where well established: `dt`, loop indices `i` and `j`, coordinates `x`, `y`, `z`.

Constants (`constexpr` at file or namespace scope) are UPPER_SNAKE_CASE: `SIM_TICK_SECONDS`.

Enumerators and public members start uppercase like types: `World::Tick`.

Everything lives in namespace `tpj`. Add a nested namespace only when a subsystem boundary requires one.

## Files and includes

File names are lowercase snake_case and match their contents: `orbit_camera.h`. Headers end in `.h` and use include guards named for the path under src/: `sim/world.h` becomes `TPJ_SIM_WORLD_H`.

Headers must compile from their own include list. Forward-declare types when a pointer or reference is enough.

In a `.cpp` file, include the matching header first, then other project headers, then third-party headers, then system headers, sorted within each group.

A header in a directory named internal is private to that directory's parent: only files under the parent may include it, from any depth. A module declares its private components in such headers, so no other module can name them (principle 6, decision 0016). cmake/check_private_headers.cmake scans src and tests for includes that break the rule, and runs in ctest as "private header check passes on the tree".

Code is arranged in layers (decision 0027), declared in cmake/layers.txt, one layer per line from the lowest. Each unit in a layer is a path under src: a directory, holding every file beneath it; a directory followed by /*, holding only the files directly in it; or a file named without its extension, holding the header and source of that name. A file belongs to the most specific unit that holds it. A file may include headers of its own unit and of units in lower layers, never of a sibling in its own layer or of a unit above it. A module's tests, under tests/ in the directory named for it, count as part of that module and may include any of its units and anything below the highest of them; tests/integration may include anything. A file in no unit fails the check, so a new module or test directory is added to the table before it lands. cmake/check_layers.cmake scans src and tests for includes that break the rule, and runs in ctest as "layer check passes on the tree".

## Tests

Tests mirror src/. A module's tests live in the tests/ directory named for it, as tests/sim/ holds the sim's, and build into one executable per module named for it, such as `tpj_sim_tests`, linking only what that module links. Test files are named for what they test with a `_test.cpp` suffix. Fixtures shared within a module's tests, and any script that generates them, go in that directory's support/ subdirectory, and their include guards are named for the path under tests/: `tests/sim/support/synthetic_types.h` becomes `TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_TYPES_H`. Tests that span modules, such as a slice's integration tests, go in tests/integration/, and checked-in park files in tests/parks/. Tests of the repository-wide checks in cmake/, such as the private header check's planted violation, go in tests/checks/. The tests/ root holds only directories and its CMakeLists.txt, which refuses to configure otherwise.

An integration test checks only what no single module's tests can show: a law that holds with every capability running together, or a consequence that crosses capabilities end to end. A property one module's tests establish gets no integration test, even when a slice criterion restates it. Each integration test names the cross-module bug it would catch; one that cannot name such a bug is a module test or a smoke test and is not written.

Integration tests grow with data, not code. A law is checked once, over every park file in tests/parks/, found by listing the directory, so a new park is covered without a code change. A consequence a slice promises compares runs, such as a park with its supply route cut against the same park with it, rather than only running one. An invariant also asserts that its subject happened, so it cannot pass on a run where nothing moved. A new integration test file says why no existing one can hold its tests.
