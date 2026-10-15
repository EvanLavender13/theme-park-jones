# Conventions

Naming and file layout come from SteelJones (decision 0015). Formatting is whatever clang-format produces from .clang-format. The language subset is not restricted.

## Naming

Types (classes, structs, enums, aliases) are nouns starting with uppercase: `OrbitCamera`, `CameraView`. If an enum is used as a type tag, give it a `Kind` suffix.

Functions are verb phrases in camelCase starting with lowercase: `stepWorld`, `drawFrame`. Predicates start with `is`, `has`, or `should`.

Variables are nouns in camelCase with a lowercase first letter. Use descriptive names. Use short names only where they are well established: `dt`, loop indices `i` and `j`, coordinates `x`, `y`, `z`.

Constants (`constexpr` at file or namespace scope) are UPPER_SNAKE_CASE: `SIM_TICK_SECONDS`.

Enumerators and public members start uppercase like types: `World::Tick`.

Everything lives in namespace `tpj`. Add a nested namespace only when a subsystem boundary requires one.

## Files and includes

File names are lowercase snake_case and match their contents: `orbit_camera.h`. Headers end in `.h`. Their include guards are named for the path under src/: `sim/world.h` becomes `TPJ_SIM_WORLD_H`.

Headers must compile from their own include list. Forward-declare types when a pointer or reference is enough.

In a `.cpp` file, include the matching header first, then other project headers, then third-party headers, then system headers. Sort the includes within each group.

If a header is in a directory named internal, it is private to that directory's parent, and only files under the parent may include it, from any depth. Declare a module's private components in such headers so that no other module can name them (principle 6, decision 0039). cmake/check_private_headers.cmake scans src and tests for includes that break this rule. It runs in ctest as "private header check passes on the tree".

Code is arranged in layers (decision 0027). The layers are declared in cmake/layers.txt, one layer per line, starting from the lowest. Each unit in a layer is a path under src, in one of three forms:

- a directory, which holds every file beneath it;
- a directory followed by /*, which holds only the files directly in it;
- a file named without its extension, which holds the header and source of that name.

If more than one unit holds a file, the file belongs to the most specific one. In a file, you may include headers from the file's own unit and from units in lower layers. Don't include headers from a sibling unit in the same layer or from a unit in a higher layer.

Put a module's tests under tests/, in the directory named for the module. Those tests count as part of the module, and they may include any of the module's units and anything below the highest of them. Files in tests/integration may include anything.

If a file is in no unit, it fails the check. Add each new module or test directory to the table before it lands. cmake/check_layers.cmake scans src and tests for includes that break the rule. It runs in ctest as "layer check passes on the tree".

## Memory in loops

Don't allocate inside loops in per-tick or per-frame code. Allocate buffers once and reuse them. The tick and the frame are loops too, so a buffer made at the start of each tick or each call breaks this rule. So does a copy, a rebuild, or a new list returned at every tick or frame. Work at a player action, such as an edit, an undo, or a click, runs inside the frame loop too. Refill its lists instead of building new ones. Keep each buffer where it outlives the call: the caller owns it and passes it in. A buffer may grow while it is smaller than the largest item so far. If you find an allocation in such a loop, fix it now; don't defer it. The allocation check in tests/checks enforces this rule for all the work the app repeats in steady play, and for its work after an undo or a redo, at a click's pick, at each tool's press and release, and when it queues a command. Only ImGui's own drawing, the resolution steps on the check's exception list, and the resolution inside an undo or a redo stay outside it (decision 0041). Code the check doesn't run must follow this rule all the same.

## Growth with the park

State, saves, copies, and per-tick work may grow with each count in the park: guests, shops, facilities, pieces of furniture, and the guest network. If any of them grows with a product of counts, such as guests times shops, it is a defect. Name it as one, with its size on full.park, and fix it. Don't make room for it with bigger budgets or files.

## Tests

The tests live under tests/, and docs/testing.md says which tests exist.

The sim root's tests are in tests/sim/. They build into `tpj_sim_tests`, which links the sim alone. Name test files for what they test, with a `_test.cpp` suffix.

Put fixtures shared within a test directory, and any script that generates them, in that directory's support/ subdirectory. Their include guards are named for the path under tests/: `tests/sim/support/synthetic_types.h` becomes `TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_TYPES_H`.

Other places for tests and test data:

- Put tests that span modules in tests/integration/.
- Put checked-in park files in tests/parks/. If only tpj_bench runs a park, put it in tests/parks/stress/.
- Put tests of the repository-wide checks in cmake/, such as the private header check's planted violation, in tests/checks/.

The tests/ root holds only directories and its CMakeLists.txt. The CMakeLists.txt refuses to configure if anything else is there.

## Specs

A module's SPEC.md tells a reader what the module does and what other modules may rely on (decision 0037). It has three parts:

- A short opening says what the module is for and which principles and decisions it serves.
- The boundary says what the module registers, publishes, and reads, and which of its headers other modules call.
- The behavior section has one short paragraph for each thing a caller or the player relies on. Each paragraph starts with the claim.

In a spec, state observable behavior, not how it is achieved. That a constructor refuses malformed input is behavior. The following are implementation, and belong in the feature's PLAN.md and the code:

- the list of inputs the constructor refuses;
- the order it breaks ties in, when no caller relies on that order;
- how it builds a value;
- its formulas;
- the values of its constants.

If a caller relies on a cost, that cost is behavior. An example is a query that allocates nothing on the tick path.
