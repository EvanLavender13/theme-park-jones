# Tests of the repository-wide checks in cmake/.

set(TPJ_PRIVATE_HEADER_CHECK ${CMAKE_SOURCE_DIR}/cmake/check_private_headers.cmake)

add_test(NAME "private header check passes on the tree"
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -P ${TPJ_PRIVATE_HEADER_CHECK})

# Each case plants its own small tree under the build directory and runs the check on it.
function(tpj_private_header_case name case)
    add_test(NAME "${name}"
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DCHECK=${TPJ_PRIVATE_HEADER_CHECK}
            -DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/private_header_trees
            -P ${CMAKE_CURRENT_SOURCE_DIR}/private_header_check_test.cmake)
endfunction()

tpj_private_header_case("private header check rejects a planted violation"
    planted_violation)
tpj_private_header_case("private header check fails naming ROOT when ROOT is not set"
    missing_root)
tpj_private_header_case("private header check scans .h and .cpp files at any depth under src and tests"
    scan_scope)
tpj_private_header_case("private header check reads includes with optional blanks, in quotes or angle brackets"
    include_forms)
tpj_private_header_case("private header check ignores commented-out includes"
    commented_includes)
tpj_private_header_case("private header check resolves an include against the including file's directory, normalizing . and .."
    own_directory)
tpj_private_header_case("private header check resolves an include against each directory above the including file up to the root"
    directories_above)
tpj_private_header_case("private header check resolves an include against the root's src directory"
    root_src)
tpj_private_header_case("private header check ignores an include that names no file inside the root"
    unnamed_includes)
tpj_private_header_case("private header check checks an include against every file it names"
    every_named_file)
tpj_private_header_case("private header check lets any file under the parent include a private header"
    anywhere_under_parent)
tpj_private_header_case("private header check compares whole path components"
    whole_components)
tpj_private_header_case("private header check reports each internal directory's parent the including file is outside"
    each_parent)
tpj_private_header_case("private header check reports an include once when several bases name the same file"
    once_per_include)
tpj_private_header_case("private header check scans a tree without a tests directory"
    without_tests)
tpj_private_header_case("private header check scans a tree without a src directory"
    without_src)
tpj_private_header_case("private header check prints each report line whole to standard error"
    long_report_line)
