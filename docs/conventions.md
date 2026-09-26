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
