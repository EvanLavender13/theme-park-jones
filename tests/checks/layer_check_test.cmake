# Plants a small tree and layer table for one CASE under SCRATCH, runs the layer check on it, and
# fails unless the check's exit status and report are the ones the rule gives. Run as:
#
#     cmake -DCASE=<case> -DCHECK=<check_layers.cmake> -DSCRATCH=<dir> -DTABLE=<cmake/layers.txt>
#         -P layer_check_test.cmake
#
# TABLE is the repository's own layer table, read only by the case that checks its layers. Unless a
# case says otherwise, each include is named by exactly one base directory, so the case does not
# depend on how the check merges the same file named through several bases.

cmake_minimum_required(VERSION 3.28)

foreach(required CASE CHECK SCRATCH TABLE)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is not set")
    endif()
endforeach()

set(TREE ${SCRATCH}/${CASE})
set(ROOT ${TREE}/root)
# The planted table sits outside ROOT, so it is never part of the scanned tree.
set(LAYERS ${TREE}/layers.txt)
file(REMOVE_RECURSE ${TREE})
file(MAKE_DIRECTORY ${ROOT}/src ${ROOT}/tests)

# Writes the file at path, relative to ROOT, holding the remaining arguments as its lines.
function(plant path)
    list(JOIN ARGN "\n" content)
    file(WRITE ${ROOT}/${path} "${content}\n")
endfunction()

# Writes the planted layer table, holding the arguments as its lines.
function(layers)
    list(JOIN ARGN "\n" content)
    file(WRITE ${LAYERS} "${content}\n")
endfunction()

# Runs the check, leaving its exit status in checkResult, its standard error in checkErrors, and
# both of its streams in checkOutput.
function(run_check)
    execute_process(
        COMMAND ${CMAKE_COMMAND} ${ARGN} -P ${CHECK}
        RESULT_VARIABLE checkResult
        OUTPUT_VARIABLE standardOutput
        ERROR_VARIABLE checkErrors)
    set(checkResult "${checkResult}" PARENT_SCOPE)
    set(checkErrors "${checkErrors}" PARENT_SCOPE)
    set(checkOutput "${standardOutput}${checkErrors}" PARENT_SCOPE)
endfunction()

# The finding lines in the check's standard error, sorted. A finding is a whole line with no
# prefix, so an indented, prefixed, or wrapped finding is not counted, while CMake's own error
# text, which it indents, is ignored.
function(report_lines errors out)
    set(lines "")
    string(REGEX MATCHALL "[^\r\n]+" allLines "${errors}")
    foreach(line IN LISTS allLines)
        if(line MATCHES "^[^ \t]+ in [^ \t]+ includes [^ \t]+ in [^ \t]+, which is not below it$"
            OR line MATCHES "^[^ \t]+ is in no layer$"
            OR line MATCHES "^[^ \t]+ holds no file$"
            OR line MATCHES "^[^ \t]+ is listed more than once$")
            list(APPEND lines "${line}")
        endif()
    endforeach()
    list(SORT lines)
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# Runs the check on ROOT with the planted table and fails unless its findings are exactly the given
# lines, each as many times as given, and it exits with status 0 exactly when there are none.
function(expect_report)
    run_check(-DROOT=${ROOT} -DLAYERS=${LAYERS})
    set(expected ${ARGN})
    list(SORT expected)
    if(expected AND checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0 on a tree with findings:\n${checkOutput}")
    endif()
    if(NOT expected AND NOT checkResult EQUAL 0)
        message(FATAL_ERROR "The check failed (${checkResult}) on a tree with no finding:\n"
            "${checkOutput}")
    endif()
    report_lines("${checkErrors}" actual)
    if(NOT "${actual}" STREQUAL "${expected}")
        list(JOIN expected "\n" expectedText)
        list(JOIN actual "\n" actualText)
        message(FATAL_ERROR "The check's standard error does not hold exactly the expected findings, "
            "each whole and unprefixed.\nExpected:\n${expectedText}\nReported:\n${actualText}\n"
            "Standard error:\n${checkErrors}")
    endif()
endfunction()

# Runs the check with the given arguments and fails unless it fails, naming the given text.
function(expect_failure_naming text)
    run_check(${ARGN})
    if(checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0:\n${checkOutput}")
    endif()
    string(FIND "${checkOutput}" "${text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "The check failed without naming ${text}:\n${checkOutput}")
    endif()
endfunction()

if(CASE STREQUAL "repository_layers")
    # The table's layers, each with its units sorted, since a layer's units are unordered.
    file(STRINGS ${TABLE} tableLines)
    set(actual "")
    foreach(line IN LISTS tableLines)
        if(line MATCHES "^[ \t]*(#|$)")
            continue()
        endif()
        string(REGEX MATCHALL "[^ \t]+" units "${line}")
        list(SORT units)
        list(JOIN units " " layer)
        list(APPEND actual "${layer}")
    endforeach()
    set(expected core sim/* sim/medium sim/park sim/routes sim/operations sim/guests
        sim/park_schema "legible render scenarios tools" "app/input app/session" "app/scene app/ui"
        app/*)
    if(NOT "${actual}" STREQUAL "${expected}")
        list(JOIN expected "\n" expectedText)
        list(JOIN actual "\n" actualText)
        message(FATAL_ERROR "${TABLE} does not declare the expected layers, lowest first.\n"
            "Expected:\n${expectedText}\nDeclared:\n${actualText}")
    endif()

elseif(CASE STREQUAL "missing_root")
    layers(a)
    expect_failure_naming(ROOT -DLAYERS=${LAYERS})

elseif(CASE STREQUAL "missing_layers")
    expect_failure_naming(LAYERS -DROOT=${ROOT})

elseif(CASE STREQUAL "unreadable_layers")
    expect_failure_naming(absent_layer_table.txt
        -DROOT=${ROOT} -DLAYERS=${TREE}/absent_layer_table.txt)

elseif(CASE STREQUAL "directory_unit")
    # src/simulation begins with the characters of src/sim but is not beneath it.
    layers(sim)
    plant(src/sim/x.h)
    plant(src/sim/deep/er/y.cpp)
    plant(src/simulation/z.cpp)
    expect_report("src/simulation/z.cpp is in no layer")

elseif(CASE STREQUAL "file_unit")
    # Only the last extension is removed, so f.g.h is the file f.g, not f.
    layers(a/f)
    plant(src/a/f.h)
    plant(src/a/f.cpp)
    plant(src/a/f/x.h)
    plant(src/a/fg.h)
    plant(src/a/f.g.h)
    expect_report(
        "src/a/fg.h is in no layer"
        "src/a/f.g.h is in no layer")

elseif(CASE STREQUAL "direct_unit")
    layers(a/*)
    plant(src/a/x.h)
    plant(src/a/x.cpp)
    plant(src/a/sub/y.h)
    expect_report("src/a/sub/y.h is in no layer")

elseif(CASE STREQUAL "most_specific_unit")
    # Each file includes a header of the top layer, so its finding names the unit it belongs to.
    layers(a/b/f a/* a/b a top)
    plant(src/top/t.h)
    plant(src/a/b/f.h "#include \"top/t.h\"")
    plant(src/a/b/g.h "#include \"top/t.h\"")
    plant(src/a/x.h "#include \"top/t.h\"")
    plant(src/a/c/y.h "#include \"top/t.h\"")
    expect_report(
        "src/a/b/f.h in src/a/b/f includes src/top/t.h in src/top, which is not below it"
        "src/a/b/g.h in src/a/b includes src/top/t.h in src/top, which is not below it"
        "src/a/x.h in src/a/* includes src/top/t.h in src/top, which is not below it"
        "src/a/c/y.h in src/a includes src/top/t.h in src/top, which is not below it")

elseif(CASE STREQUAL "own_and_lower_units")
    layers(low high)
    plant(src/low/l.h)
    plant(src/low/deep/m.h "#include \"../l.h\"")
    plant(src/high/h.h "#include \"low/l.h\"")
    plant(src/high/h.cpp "#include \"h.h\"" "#include \"low/deep/m.h\"")
    expect_report()

elseif(CASE STREQUAL "sibling_and_higher_units")
    layers(low "a b")
    plant(src/a/x.h)
    plant(src/b/y.h)
    plant(src/b/y.cpp "#include \"a/x.h\"")
    plant(src/low/l.cpp "#include \"a/x.h\"")
    expect_report(
        "src/b/y.cpp in src/b includes src/a/x.h in src/a, which is not below it"
        "src/low/l.cpp in src/low includes src/a/x.h in src/a, which is not below it")

elseif(CASE STREQUAL "module_tests")
    # Module a's units span two layers; its tests sit in the higher, a/top's.
    layers(a/base b "a/top c" d)
    plant(src/a/base/x.h)
    plant(src/b/y.h)
    plant(src/a/top/z.h)
    plant(src/c/w.h)
    plant(src/d/v.h)
    plant(tests/a/support/s.h)
    plant(tests/b/r.h)
    plant(tests/c/u.h)
    plant(tests/a/t_test.cpp
        "#include \"a/base/x.h\""
        "#include \"b/y.h\""
        "#include \"a/top/z.h\""
        "#include \"support/s.h\""
        "#include \"tests/b/r.h\""
        "#include \"c/w.h\""
        "#include \"d/v.h\""
        "#include \"tests/c/u.h\"")
    expect_report(
        "tests/a/t_test.cpp in tests/a includes src/c/w.h in src/c, which is not below it"
        "tests/a/t_test.cpp in tests/a includes src/d/v.h in src/d, which is not below it"
        "tests/a/t_test.cpp in tests/a includes tests/c/u.h in tests/c, which is not below it")

elseif(CASE STREQUAL "integration_tests")
    layers(a b)
    plant(src/a/x.h)
    plant(src/b/y.h)
    plant(tests/b/s.h)
    plant(tests/integration/support/p.h)
    plant(tests/integration/i_test.cpp
        "#include \"a/x.h\""
        "#include \"b/y.h\""
        "#include \"tests/b/s.h\""
        "#include \"support/p.h\"")
    plant(src/b/z.cpp "#include \"tests/integration/support/p.h\"")
    expect_report(
        "src/b/z.cpp in src/b includes tests/integration/support/p.h in tests/integration, which is not below it")

elseif(CASE STREQUAL "no_layer")
    # src/b/h.h is in no layer, so neither its own include nor the include of it is checked.
    layers(a)
    plant(src/a/y.h "#include \"b/h.h\"")
    plant(src/b/h.h "#include \"a/y.h\"")
    plant(src/r.cpp)
    plant(tests/z.h "#include \"a/y.h\"")
    plant(tests/q/w_test.cpp "#include \"a/y.h\"")
    expect_report(
        "src/b/h.h is in no layer"
        "src/r.cpp is in no layer"
        "tests/z.h is in no layer"
        "tests/q/w_test.cpp is in no layer")

elseif(CASE STREQUAL "scan_scope")
    # Files that are neither .h nor .cpp are not scanned, so their includes and their place in no
    # unit go unreported.
    layers(a b)
    plant(src/b/y.h)
    plant(src/a/x.cpp "#include \"b/y.h\"")
    plant(src/a/p/q/r.h "#include \"b/y.h\"")
    plant(tests/a/v.h "#include \"b/y.h\"")
    plant(tests/a/s/t/u_test.cpp "#include \"b/y.h\"")
    plant(src/a/m.hpp "#include \"b/y.h\"")
    plant(src/a/n.inl "#include \"b/y.h\"")
    plant(src/notes.txt "#include \"b/y.h\"")
    plant(tests/w.cmake "#include \"b/y.h\"")
    expect_report(
        "src/a/x.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "src/a/p/q/r.h in src/a includes src/b/y.h in src/b, which is not below it"
        "tests/a/v.h in tests/a includes src/b/y.h in src/b, which is not below it"
        "tests/a/s/t/u_test.cpp in tests/a includes src/b/y.h in src/b, which is not below it")

elseif(CASE STREQUAL "include_forms")
    layers(a b)
    plant(src/b/y.h)
    plant(src/a/quoted.cpp "#include \"b/y.h\"")
    plant(src/a/angled.cpp "#include <b/y.h>")
    plant(src/a/blanks.cpp " \t#\t include\t \"b/y.h\"")
    plant(src/a/no_blanks.cpp "#include<b/y.h>")
    expect_report(
        "src/a/quoted.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "src/a/angled.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "src/a/blanks.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "src/a/no_blanks.cpp in src/a includes src/b/y.h in src/b, which is not below it")

elseif(CASE STREQUAL "commented_includes")
    layers(a b)
    plant(src/b/y.h)
    plant(src/a/x.cpp
        "// #include \"b/y.h\""
        "//#include <b/y.h>"
        "    // #include \"b/y.h\"")
    expect_report()

elseif(CASE STREQUAL "resolution_bases")
    # One include for each base: the file's own directory, normalizing . and ..; a directory two
    # levels up; ROOT itself; and ROOT/src.
    layers(a b)
    plant(src/b/y.h)
    plant(tests/b/support/s.h)
    plant(src/a/own.cpp "#include \"./../b/y.h\"")
    plant(tests/a/deep/above_test.cpp "#include \"b/support/s.h\"")
    plant(src/a/root.cpp "#include \"src/b/y.h\"")
    plant(tests/a/src_test.cpp "#include \"b/y.h\"")
    expect_report(
        "src/a/own.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "tests/a/deep/above_test.cpp in tests/a includes tests/b/support/s.h in tests/b, which is not below it"
        "src/a/root.cpp in src/a includes src/b/y.h in src/b, which is not below it"
        "tests/a/src_test.cpp in tests/a includes src/b/y.h in src/b, which is not below it")

elseif(CASE STREQUAL "every_named_file")
    # The include names one file from its own directory and another from above it, so a check that
    # stopped at the first file found would report only one of them.
    layers(a "a/c c")
    plant(src/a/c/h.h)
    plant(src/c/h.h)
    plant(src/a/x.cpp "#include \"c/h.h\"")
    expect_report(
        "src/a/x.cpp in src/a includes src/a/c/h.h in src/a/c, which is not below it"
        "src/a/x.cpp in src/a includes src/c/h.h in src/c, which is not below it")

elseif(CASE STREQUAL "once_per_pair")
    # The header is included twice, and each include names it both from src, a directory above
    # the file, and from ROOT/src.
    layers(a b)
    plant(src/b/y.h)
    plant(src/a/z/w.cpp "#include \"b/y.h\"" "#include <b/y.h>")
    expect_report("src/a/z/w.cpp in src/a includes src/b/y.h in src/b, which is not below it")

elseif(CASE STREQUAL "not_dependencies")
    # The last two includes name existing files, one inside ROOT but outside src and tests, the
    # other outside ROOT.
    layers(a)
    plant(src/a/x.h)
    plant(other/h.h)
    file(WRITE ${TREE}/outside/h.h "\n")
    plant(src/a/x.cpp
        "#include <vector>"
        "#include \"missing/h.h\""
        "#include \"other/h.h\""
        "#include \"../../../outside/h.h\"")
    expect_report()

elseif(CASE STREQUAL "table_format")
    # Were a comment read as a layer, b would be listed twice; were the tab and spaces not a
    # separator, the third layer would be one unit holding no file.
    layers(
        "# a comment naming b"
        ""
        a
        "  \t "
        "  \t# b c"
        "b\t  c")
    plant(src/a/x.h)
    plant(src/c/z.h "#include \"a/x.h\"")
    plant(src/b/y.cpp "#include \"c/z.h\"")
    expect_report("src/b/y.cpp in src/b includes src/c/z.h in src/c, which is not below it")

elseif(CASE STREQUAL "units_holding_no_file")
    # src/a holds x.h even though x.h belongs to the more specific src/a/*, while src/c/* holds
    # nothing, since its one file is in a subdirectory.
    layers(a/* a "b c/*")
    plant(src/a/x.h)
    plant(src/c/d/y.h)
    expect_report(
        "src/b holds no file"
        "src/c/* holds no file"
        "src/c/d/y.h is in no layer")

elseif(CASE STREQUAL "units_listed_twice")
    layers(a b "c a" "d d")
    plant(src/a/x.h)
    plant(src/b/y.h)
    plant(src/c/z.h)
    plant(src/d/w.h)
    expect_report(
        "src/a is listed more than once"
        "src/d is listed more than once")

elseif(CASE STREQUAL "long_finding_line")
    # Longer than any width CMake wraps message text to.
    set(low a_module_whose_name_is_long_enough/and_nested_well_below_the_source_root)
    set(high another_module_with_a_long_name/and_a_unit_nested_deep_beneath_it)
    layers(${low} ${high})
    plant(src/${high}/a_header_with_a_long_name_that_the_lower_layer_may_not_include.h)
    plant(src/${low}/an_including_file_with_a_long_name.cpp
        "#include \"${high}/a_header_with_a_long_name_that_the_lower_layer_may_not_include.h\"")
    expect_report("src/${low}/an_including_file_with_a_long_name.cpp in src/${low} includes src/${high}/a_header_with_a_long_name_that_the_lower_layer_may_not_include.h in src/${high}, which is not below it")

else()
    message(FATAL_ERROR "Unknown CASE ${CASE}")
endif()
