# Research: layered-dependencies

## How should the layers be declared and matched?

Chromium's checkdeps keeps directory rules in a DEPS file per directory, inherited from the parents, and adds specific_include_rules, keyed by a regular expression on file names, for the few files that need different rules from their directory. That shows the need this tree has: a directory-level rule covers almost everything, and one file, sim/park_schema, needs its own place. It also shows the cost of spreading rules across directories: the rules a file obeys are the sum of several files, and the order of regex rules is unspecified.

One table of units ordered by layer gives the same coverage with one place to read. A unit is a directory, or a file path without its extension that holds the header and source of that name, matched by whole path components as the private header check compares paths. A file belongs to the most specific unit that holds it, which gives sim/park_schema its own unit inside sim without a regex. A file that matches no unit is itself a finding, so a new module cannot slip past the table.

Includes are resolved as cmake/check_private_headers.cmake resolves them: against the including file's directory, each directory above it up to the root, and the root's src. An include that names no file inside the root, such as a system or third-party header, is not a dependency between units.

Rejected: a DEPS file per directory — the rules a file obeys would be spread over several files, against declaring the layers once. Regular expressions for file-level units — an extensionless file unit covers the one component that needs it and has no ordering ambiguity. Raw string prefixes — sim/park would also match a file named sim/park_schema.h, which the private header check's whole-component comparison exists to prevent. Listing allowed dependencies per unit instead of ordering units in layers — every unit would repeat what the layer order already says, and sibling units in one layer would need explicit denials.

Sources: https://chromium.googlesource.com/chromium/src/+/master/buildtools/checkdeps/README.md — DEPS include_rules, inheritance, and specific_include_rules; https://chromium.googlesource.com/chromium/src/+/HEAD/base/DEPS — a real DEPS file using per-file rules for a few files.

## How is a check over plans enforced at commit time?

The repository's hooks are shell scripts in .githooks, and its repository-wide checks are CMake scripts in cmake/ run with cmake -P, tested by planting small trees under the build directory (tests/checks/private_header_check_test.cmake). A placement check fits the same split: a CMake script that takes the PLAN.md files a commit adds and fails naming each one with no Placement section, tested by planted cases in tests/checks, and a line in the pre-commit hook that passes it the added PLAN.md files from git diff --cached --diff-filter=A. Checking only added files leaves the plans written before the rule as they are.

Rejected: a ctest check over every PLAN.md in plans/ — it would fail on every plan written before the rule, or need a list of exemptions that only grows. Checking the content of the section — whether a placement is right is judgement, which review carries.

Sources: none external; the patterns are the repository's own, in .githooks/pre-commit, cmake/check_private_headers.cmake, and tests/checks/CMakeLists.txt.
