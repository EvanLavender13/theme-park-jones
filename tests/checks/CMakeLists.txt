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

set(TPJ_LAYER_CHECK ${CMAKE_SOURCE_DIR}/cmake/check_layers.cmake)
set(TPJ_LAYER_TABLE ${CMAKE_SOURCE_DIR}/cmake/layers.txt)

add_test(NAME "layer check passes on the tree"
    COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR} -DLAYERS=${TPJ_LAYER_TABLE}
        -P ${TPJ_LAYER_CHECK})

# Each case plants its own small tree and layer table under the build directory and runs the check
# on them.
function(tpj_layer_case name case)
    add_test(NAME "${name}"
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DCHECK=${TPJ_LAYER_CHECK}
            -DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/layer_trees -DTABLE=${TPJ_LAYER_TABLE}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/layer_check_test.cmake)
endfunction()

tpj_layer_case("cmake/layers.txt declares the layers of the code, lowest first"
    repository_layers)
tpj_layer_case("layer check fails naming ROOT when ROOT is not set"
    missing_root)
tpj_layer_case("layer check fails naming LAYERS when LAYERS is not set"
    missing_layers)
tpj_layer_case("layer check fails naming a layer table it cannot read"
    unreadable_layers)
tpj_layer_case("layer check's directory unit holds every file beneath it, by whole path components"
    directory_unit)
tpj_layer_case("layer check's file unit holds each file that is its path once the last extension is removed"
    file_unit)
tpj_layer_case("layer check's /* unit holds only the files directly in its directory"
    direct_unit)
tpj_layer_case("layer check puts a file in the most specific unit that holds it"
    most_specific_unit)
tpj_layer_case("layer check allows includes of a file's own unit and of lower layers"
    own_and_lower_units)
tpj_layer_case("layer check reports includes of a sibling in the same layer or of a higher layer"
    sibling_and_higher_units)
tpj_layer_case("layer check puts a module's tests in the highest layer of the module's units"
    module_tests)
tpj_layer_case("layer check puts tests/integration above every layer"
    integration_tests)
tpj_layer_case("layer check reports a file in no layer once, checking no include from or of it"
    no_layer)
tpj_layer_case("layer check scans .h and .cpp files at any depth under src and tests"
    scan_scope)
tpj_layer_case("layer check reads includes with optional blanks, in quotes or angle brackets"
    include_forms)
tpj_layer_case("layer check ignores commented-out includes"
    commented_includes)
tpj_layer_case("layer check resolves an include against the file's directory, each directory above it, and the root's src"
    resolution_bases)
tpj_layer_case("layer check checks an include against every file it names"
    every_named_file)
tpj_layer_case("layer check reports each pair of file and header once"
    once_per_pair)
tpj_layer_case("layer check takes no file outside the root's src and tests as a dependency"
    not_dependencies)
tpj_layer_case("layer check reads one layer per line, units separated by blanks, skipping blank and comment lines"
    table_format)
tpj_layer_case("layer check reports each unit that holds no file"
    units_holding_no_file)
tpj_layer_case("layer check reports each unit listed more than once"
    units_listed_twice)
tpj_layer_case("layer check prints each finding whole to standard error"
    long_finding_line)

set(TPJ_PLACEMENT_CHECK ${CMAKE_SOURCE_DIR}/cmake/check_placement.cmake)

# Each case plants its own small tree of plans under the build directory and runs the check on it.
function(tpj_placement_case name case)
    add_test(NAME "${name}"
        COMMAND ${CMAKE_COMMAND} -DCASE=${case} -DCHECK=${TPJ_PLACEMENT_CHECK}
            -DSCRATCH=${CMAKE_CURRENT_BINARY_DIR}/placement_trees
            -P ${CMAKE_CURRENT_SOURCE_DIR}/placement_check_test.cmake)
endfunction()

tpj_placement_case("placement check fails naming ROOT when ROOT is not set"
    missing_root)
tpj_placement_case("placement check fails naming ROOT when ROOT is empty"
    empty_root)
tpj_placement_case("placement check fails naming PLANS when PLANS is not set"
    missing_plans)
tpj_placement_case("placement check fails naming a plan it cannot read"
    unreadable_plan)
tpj_placement_case("placement check finds the section on a line that is the heading followed by nothing but blanks"
    section_lines)
tpj_placement_case("placement check ignores a carriage return at a line's end"
    carriage_returns)
tpj_placement_case("placement check reads a plan as UTF-8, keeping a character outside ASCII on its line"
    utf8_lines)
tpj_placement_case("placement check takes no other line for the section"
    other_lines)
tpj_placement_case("placement check reports each plan without the section once, as listed"
    once_per_plan)
tpj_placement_case("placement check reports nothing and passes when PLANS is empty"
    empty_plans)
tpj_placement_case("placement check prints each report line whole to standard error"
    long_report_line)
