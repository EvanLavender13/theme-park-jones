# Research: private-header-check

## How should a text scan decide which file an include names?

GCC reads a quoted include first against the directory of the file that contains it, then against the -iquote and -I directories in order, then the system directories. An angle-bracket include skips the file's own directory. The project's targets use different -I roots: every target has src, tpj_scenarios_lib and tpj_scenarios_tests add the repository root, and tpj_sim_tests adds tests/sim, which is how tests/sim/medium/flow_test.cpp reaches "support/synthetic_flows.h". A scan that copied each target's search path would have to read the build, and it would miss headers, which belong to no target.

So the scan reads each include against a fixed superset of those roots: the including file's directory, every directory above it up to the repository root, and src. Every project root is one of these for the files that use it. The scan checks every existing file the path could name, not just the first, which can only report more. The cost is a false report when two files share a relative path under different roots and one of them is private, which the tree does not have and a rename fixes. An include that names no file is left alone: it names a system or third-party header, or the build fails on it already.

The rule applies to a file's real location, not to how the include is spelled, so "../internal/x.h" and "sim/guests/internal/x.h" are judged alike once the path is normalized. cmake_path, available since CMake 3.20, normalizes paths and tests prefixes component by component, so src/sim is not a prefix of src/simulation.

The check is a cmake -P script, like cmake/check_sim_symbols.cmake, so it runs in both presets' ctest with no other tool. Its failure test plants a tree in the build directory rather than the source tree, because a violation planted under tests/ would fail the real scan.

Rejected: Clang module maps and PRIVATE include directories — see the capability's RESEARCH.md. Reading resolved dependencies from Ninja or -MMD output — they list every header an object reaches, not which file included which, and cover only compiled files. Judging the include text alone, such as refusing any path with an internal component written outside it — the spelling does not say which directory is meant when roots differ. A planted violation checked into tests/ — the real scan would find it.

Sources: https://gcc.gnu.org/onlinedocs/cpp/Search-Path.html — quote and angle-bracket search order, -I and -iquote; https://cmake.org/cmake/help/latest/command/cmake_path.html — normalization and component-wise prefix tests.
