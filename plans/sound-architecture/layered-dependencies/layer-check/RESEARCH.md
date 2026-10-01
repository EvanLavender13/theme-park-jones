# Research: layer-check

## How does the check match units and read the table?

cmake_path, available since CMake 3.20 and already required at 3.28 by the repository's checks, compares paths by component. IS_PREFIX is what the private header check uses to tell src/simulation from src/sim, and its whole_components case proves it. A directory unit holds a file when the unit's path under src is a component prefix of the file's. An extensionless file unit such as sim/park_schema holds a file when the file's path with its last extension removed (REMOVE_EXTENSION LAST_ONLY) equals the unit's path, which holds park_schema.h and park_schema.cpp and nothing named park_schema_test.cpp or park_schemas.h. The sim's base is the sim root's own files, so its unit is written sim/* and holds a file only when the file's parent directory is src/sim. A plain sim directory unit would also hold any new subdirectory of src/sim, which would then pass unlisted, in the base layer.

The table is a plain text file read with file(STRINGS), one layer per line, lowest first, with the units of a layer separated by blanks and lines beginning with # ignored. The check takes its path as LAYERS beside ROOT, so the planted cases each write their own table and the tree test passes the repository's, cmake/layers.txt. A table that lives at a fixed path inside ROOT would force every planted tree to carry a cmake/ directory, and a CMake list would need separators for the units within a layer.

Rejected: a CMake list in an included file — the units within one layer need a second separator inside a list. A fixed table path under ROOT — planted trees would all carry the same scaffolding, and the check could not be pointed at a table under test.

Sources: https://cmake.org/cmake/help/latest/command/cmake_path.html — IS_PREFIX, REMOVE_EXTENSION with LAST_ONLY, and RELATIVE_PATH; cmake/check_private_headers.cmake and tests/checks/private_header_check_test.cmake — the scan, resolution, and planted-tree pattern followed.

## How does makeNewPark take its schema from above?

makeNewPark places the park's private components, the entrance and the first guest path, so it must stay in sim/park (decision 0016). The only upward reach is its call to makeParkSchema. Passing the schema in removes it. An overload at sim/park_schema's level, makeNewPark(seed), hands the template makeParkSchema(), so every caller keeps calling makeNewPark(seed). Four test files call makeNewPark without including sim/park_schema.h: tests/sim/guests/walks_test.cpp, tests/sim/park/validity_test.cpp, tests/sim/routes/connections_test.cpp, and tests/sim/routes/previous_networks_test.cpp. They gain that include and nothing else.

Rejected: every caller passing makeParkSchema() — sixteen call sites change for no gain in what they express. Defining makeNewPark(seed) in park_schema.cpp while declaring it in sim/park/intent.h — the include would pass the check while the park unit's interface still depended on the level above it.
