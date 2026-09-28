# Implementation Plan: Private Header Check

## Goal

Add cmake/check_private_headers.cmake, which fails when a file under src or tests includes a header in a directory named internal from outside that directory's parent, run by ctest on the repository and on a planted violation, with the rule in docs/conventions.md.

## Approach

A cmake -P script, like cmake/check_sim_symbols.cmake, globs the .h and .cpp files under ROOT/src and ROOT/tests, reads their include lines with file(STRINGS REGEX), and joins each path to the including file's directory, every directory above it up to ROOT, and ROOT/src. For every existing file that gives, it walks the file's directories up to ROOT and, for each one named internal, requires the including file to lie under that directory's parent, testing with cmake_path(IS_PREFIX). The test pass writes the ctest tests in tests/checks/, including the planted tree, which is written into the build directory so the real scan never sees it.

## Tasks

### Task 1: State the rule in the conventions

Files:
- Modify: `docs/conventions.md`

Step 1: Make the two edits in FEATURE.md's Spec changes, with their exact text: the paragraph after the include order paragraph in "Files and includes", and the sentence after the one on tests/integration/ and tests/parks/ in "Tests".

### Task 2: Add the check's stub

Files:
- Create: `cmake/check_private_headers.cmake`

Step 1: Create the script with its header comment and usage check, and a pass that scans nothing:

```cmake
# Fails when a file under src or tests includes a header private to another part of the tree
# (principle 6, decision 0016). A header in a directory named internal is private to that
# directory's parent, and only files under the parent may include it.
#
# Usage: cmake -DROOT=<tree> -P check_private_headers.cmake
# On failure it prints a line "<file> includes <header>, which is private to <parent>" for each
# violation, with paths relative to ROOT.

cmake_minimum_required(VERSION 3.28)

if(NOT ROOT)
    message(FATAL_ERROR "usage: cmake -DROOT=<tree> -P check_private_headers.cmake")
endif()

message(STATUS "${ROOT} includes no private header from outside its parent")
```

Step 2: Run it.

Run: `cmake -DROOT=$PWD -P cmake/check_private_headers.cmake; echo "status $?"`
Expected: `-- <repository path> includes no private header from outside its parent` and `status 0`.

Run: `cmake -P cmake/check_private_headers.cmake; echo "status $?"`
Expected: an error with the usage line naming ROOT, and `status 1`.

### Task 3: Run the test pass

Run the test pass as implementing-features describes, with FEATURE.md, docs/conventions.md, and cmake/check_private_headers.cmake as the interface. Then reconfigure and build, since the test pass adds a directory under tests/.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug && ctest --preset linux-debug -R "private header"`
Expected: the build succeeds without warnings. "private header check passes on the tree" passes, and "private header check rejects a planted violation" fails because the stub accepts the planted tree. Any test the test pass adds for criteria 1 to 3 that plants a violation fails for the same reason.

### Task 4: Implement the scan

Files:
- Modify: `cmake/check_private_headers.cmake`

Step 1: Replace everything after the usage check with:

```cmake
# ROOT as an absolute, normalized path with no trailing separator, so that walks up from a file
# stop at it.
cmake_path(ABSOLUTE_PATH ROOT NORMALIZE)
string(REGEX REPLACE "(.)/$" "\\1" ROOT "${ROOT}")

file(GLOB_RECURSE files LIST_DIRECTORIES false
    "${ROOT}/src/*.h" "${ROOT}/src/*.cpp" "${ROOT}/tests/*.h" "${ROOT}/tests/*.cpp")
list(SORT files)

set(INCLUDE_LINE "^[ \t]*#[ \t]*include[ \t]*[\"<]([^\">]+)[\">]")
set(report "")
foreach(file IN LISTS files)
    # An include is read against the file's directory, every directory above it up to ROOT, and
    # ROOT/src: a superset of every target's search path, so it is checked against every file it
    # could name.
    set(bases "${ROOT}/src")
    cmake_path(GET file PARENT_PATH base)
    while(TRUE)
        list(APPEND bases "${base}")
        if(base STREQUAL ROOT)
            break()
        endif()
        cmake_path(GET base PARENT_PATH base)
    endwhile()

    file(STRINGS "${file}" includes REGEX "${INCLUDE_LINE}")
    foreach(line IN LISTS includes)
        string(REGEX MATCH "${INCLUDE_LINE}" match "${line}")
        set(path "${CMAKE_MATCH_1}")
        foreach(base IN LISTS bases)
            set(header "${base}/${path}")
            cmake_path(NORMAL_PATH header)
            cmake_path(IS_PREFIX ROOT "${header}" NORMALIZE inside)
            if(NOT inside OR NOT EXISTS "${header}" OR IS_DIRECTORY "${header}")
                continue()
            endif()
            # Each directory named internal above the header makes it private to its parent.
            cmake_path(GET header PARENT_PATH directory)
            while(NOT directory STREQUAL ROOT)
                cmake_path(GET directory FILENAME name)
                cmake_path(GET directory PARENT_PATH parent)
                if(name STREQUAL "internal")
                    cmake_path(IS_PREFIX parent "${file}" NORMALIZE allowed)
                    if(NOT allowed)
                        cmake_path(RELATIVE_PATH file BASE_DIRECTORY "${ROOT}"
                            OUTPUT_VARIABLE fileText)
                        cmake_path(RELATIVE_PATH header BASE_DIRECTORY "${ROOT}"
                            OUTPUT_VARIABLE headerText)
                        cmake_path(RELATIVE_PATH parent BASE_DIRECTORY "${ROOT}"
                            OUTPUT_VARIABLE parentText)
                        list(APPEND report
                            "${fileText} includes ${headerText}, which is private to ${parentText}")
                    endif()
                endif()
                set(directory "${parent}")
            endwhile()
        endforeach()
    endforeach()
endforeach()

# NOTICE prints each line as it is; FATAL_ERROR would wrap long ones.
if(report)
    list(REMOVE_DUPLICATES report)
    foreach(line IN LISTS report)
        message(NOTICE "${line}")
    endforeach()
    message(FATAL_ERROR "Private headers are included from outside their parents.")
endif()
message(STATUS "${ROOT} includes no private header from outside its parent")
```

Step 2: Run it on the repository.

Run: `cmake -DROOT=$PWD -P cmake/check_private_headers.cmake; echo "status $?"`
Expected: `-- <repository path> includes no private header from outside its parent` and `status 0`.

Step 3: Run the check's tests.

Run: `ctest --preset linux-debug -R "private header"`
Expected: every test passes.

### Task 5: Verify both builds

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs -r clang-format -i`
Expected: no output.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostics, and every test passes.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds and every test passes, the private header check's included.

Run: `scripts/cross-build-check.sh`
Expected: it reports that both builds wrote the same lines and passed, as the simulation is unchanged.

### Task 6: Commit

Via the commit-hygiene skill, stage everything and commit once with the subject `Build: Add the private header check` and the Co-Authored-By trailer.
