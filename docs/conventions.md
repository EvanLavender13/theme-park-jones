# Conventions

Naming and file layout come from SteelJones (decision 0015). Formatting is whatever clang-format produces from .clang-format. The language subset is not restricted.

## Naming

Types (classes, structs, enums, aliases) are nouns starting with uppercase: `OrbitCamera`, `CameraView`. Enums used as type tags get a `Kind` suffix.

Functions are verb phrases in camelCase starting with lowercase: `stepWorld`, `drawFrame`. Predicates start with `is`, `has`, or `should`.

Variables are nouns in camelCase with a lowercase first letter. Use descriptive names. Use short names only where they are well established: `dt`, loop indices `i` and `j`, coordinates `x`, `y`, `z`.

Constants (`constexpr` at file or namespace scope) are UPPER_SNAKE_CASE: `SIM_TICK_SECONDS`.

Enumerators and public members start uppercase like types: `World::Tick`.

Everything lives in namespace `tpj`. Add a nested namespace only when a subsystem boundary requires one.

## Files and includes

File names are lowercase snake_case and match their contents: `orbit_camera.h`. Headers end in `.h`. Their include guards are named for the path under src/: `sim/world.h` becomes `TPJ_SIM_WORLD_H`.

Headers must compile from their own include list. Forward-declare types when a pointer or reference is enough.

In a `.cpp` file, include the matching header first, then other project headers, then third-party headers, then system headers. Sort the includes within each group.

A header in a directory named internal is private to that directory's parent. Only files under the parent may include it, from any depth. A module declares its private components in such headers, so no other module can name them (principle 6, decision 0016). cmake/check_private_headers.cmake scans src and tests for includes that break this rule. It runs in ctest as "private header check passes on the tree".

Code is arranged in layers (decision 0027). The layers are declared in cmake/layers.txt, one layer per line, starting from the lowest. Each unit in a layer is a path under src, in one of three forms:

- a directory, which holds every file beneath it;
- a directory followed by /*, which holds only the files directly in it;
- a file named without its extension, which holds the header and source of that name.

A file belongs to the most specific unit that holds it. A file may include headers from its own unit and from units in lower layers. It may not include headers from a sibling unit in its own layer or from a unit in a higher layer.

A module's tests live under tests/ in the directory named for the module. They count as part of that module. They may include any of the module's units and anything below the highest of them. Files in tests/integration may include anything.

A file that is in no unit fails the check, so add a new module or test directory to the table before it lands. cmake/check_layers.cmake scans src and tests for includes that break the rule. It runs in ctest as "layer check passes on the tree".

## Tests

The tests live under tests/, and docs/testing.md says which tests exist.

The sim root's tests are in tests/sim/. They build into `tpj_sim_tests`, which links the sim alone. Name test files for what they test, with a `_test.cpp` suffix.

Put fixtures shared within a test directory, and any script that generates them, in that directory's support/ subdirectory. Their include guards are named for the path under tests/: `tests/sim/support/synthetic_types.h` becomes `TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_TYPES_H`.

Other places for tests and test data:

- Tests that span modules go in tests/integration/.
- Checked-in park files go in tests/parks/. Parks that only tpj_bench runs go in tests/parks/stress/.
- Tests of the repository-wide checks in cmake/, such as the private header check's planted violation, go in tests/checks/.

The tests/ root holds only directories and its CMakeLists.txt. The CMakeLists.txt refuses to configure if anything else is there.

## Specs

A module's SPEC.md tells a reader what the module does and what other modules may rely on (decision 0037). It has three parts:

- A short opening says what the module is for and which principles and decisions it serves.
- The boundary says what the module registers, publishes, and reads, and which of its headers other modules call.
- The behavior section has one short paragraph for each thing a caller or the player relies on. Each paragraph starts with the claim.

A spec states observable behavior, not how it is achieved. That a constructor refuses malformed input is behavior. The following are implementation, and belong in the feature's PLAN.md and the code:

- the list of inputs the constructor refuses;
- the order it breaks ties in, when no caller relies on that order;
- how it builds a value;
- its formulas;
- the values of its constants.

A cost a caller relies on is behavior. An example is a query that allocates nothing on the tick path.
