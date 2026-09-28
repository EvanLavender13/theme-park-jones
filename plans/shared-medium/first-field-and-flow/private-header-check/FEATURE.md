# Feature: Private Header Check

## Summary

A header in a directory named internal is private to that directory's parent, and a mechanical check enforces it (principle 6, decision 0016). The check is a CMake script, cmake/check_private_headers.cmake, that scans every C++ file under a tree's src and tests directories, reads each include the way the build could, and fails when a file includes a private header from outside the directory it is private to. Two ctest tests run it: one on the repository, which passes, and one on a violation planted in the build directory, which the check must reject. docs/conventions.md states the rule. No module has an internal directory yet; the check is in place before the first one.

## Acceptance criteria

The check is run as `cmake -DROOT=<tree> -P cmake/check_private_headers.cmake`. It scans every file whose name ends in .h or .cpp under ROOT/src and ROOT/tests, at any depth. A tree without one of those directories has no files there to scan, so it is not an error. It reads a line as an include when, after optional spaces and tabs, it holds `#`, optional spaces and tabs, `include`, optional spaces and tabs, and a path in double quotes or angle brackets. Other lines, including commented-out includes that start with `//`, are not includes. A missing ROOT is a usage error: the script fails with a message naming ROOT.

1. An include path names a file when joining it to a base directory and normalizing the result, resolving `.` and `..`, gives an existing file inside ROOT. The base directories are the including file's own directory, every directory above it up to and including ROOT, and ROOT/src. An include is checked against every file it names, and an include that names none is ignored.
2. A named file is private to the parent of each directory named internal that contains it, at any depth below ROOT. An include is a violation when the including file does not lie under one of those parents, where lying under compares whole path components. A file under the parent may include the header from any depth, including from the internal directory itself.
3. The check exits with status 0 when the scan finds no violation, and otherwise exits with a non-zero status after printing one line for each violating include and each parent it is outside: `<file> includes <header>, which is private to <parent>`, with all three paths relative to ROOT and written with forward slashes. Each report line is printed whole to standard error, unwrapped and with no prefix, and the failure message that follows it may be CMake's usual error text. The same include is reported once even when several bases name the same file.
4. The ctest test "private header check passes on the tree" runs the check with ROOT set to the repository, and passes in linux-debug and windows-debug.
5. The ctest test "private header check rejects a planted violation" writes a tree into the build directory that holds a private header and a file outside its parent that includes it, runs the check on that tree, and passes only when the check fails and its output names the planted including file. It passes in linux-debug and windows-debug.
6. docs/conventions.md states the rule, names the check and where it runs, and says where tests of repository-wide checks live.

## Medium

None. The check guards the medium's boundary: a module's private components, declared in its internal headers, cannot be named outside it, so modules meet only through the medium's public headers.

## Principle checks

Principle 6: no file includes a header private to another part of the tree, which criterion 4 asserts of the repository on every ctest run, and criterion 5 proves the check can fail. No other principle applies: the feature adds no simulation code.

## Spec changes

No module's SPEC.md changes, since the check belongs to no module. docs/conventions.md changes in two places.

In "Files and includes", after the paragraph on include order, add:

"A header in a directory named internal is private to that directory's parent: only files under the parent may include it, from any depth. A module declares its private components in such headers, so no other module can name them (principle 6, decision 0016). cmake/check_private_headers.cmake scans src and tests for includes that break the rule, and runs in ctest as "private header check passes on the tree"."

In "Tests", after the sentence on tests/integration/ and tests/parks/, add:

"Tests of the repository-wide checks in cmake/, such as the private header check's planted violation, go in tests/checks/."

## Files affected

- Create: cmake/check_private_headers.cmake
- Modify: docs/conventions.md
- Create, by the test pass: tests/checks/CMakeLists.txt and the scripts it runs
- Modify, by the test pass: tests/CMakeLists.txt, to add the checks directory

## Dependencies

CMake 3.28, the project's minimum, for cmake_path and file(STRINGS REGEX). No sibling feature.

## Out of scope

- Includes spelled through a macro, such as `#include SOME_HEADER`, and __has_include tests. The project uses neither.
- Includes inside block comments or disabled preprocessor branches, which the scan reads as includes. A false report is fixed by removing the dead include.
- Files outside src and tests, and extensions other than .h and .cpp, which the conventions do not use.
- Running the check as its own pre-push step. It is a ctest test, and pre-push runs ctest.

None of these goes to the backlog: each would matter only if the tree started doing what the conventions rule out.

## Open questions

None.
