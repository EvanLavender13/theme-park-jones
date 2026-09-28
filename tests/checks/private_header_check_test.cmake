# Plants a small tree for one CASE under SCRATCH, runs the private header check on it, and fails
# unless the check's exit status and report are the ones the rule gives. Run as:
#
#     cmake -DCASE=<case> -DCHECK=<check_private_headers.cmake> -DSCRATCH=<dir> -P private_header_check_test.cmake
#
# Unless a case says otherwise, each include is named by exactly one base directory, so the case
# does not depend on how the check merges the same file named through several bases.

cmake_minimum_required(VERSION 3.28)

foreach(required CASE CHECK SCRATCH)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is not set")
    endif()
endforeach()

set(TREE ${SCRATCH}/${CASE})
set(ROOT ${TREE}/root)
file(REMOVE_RECURSE ${TREE})
file(MAKE_DIRECTORY ${ROOT}/src ${ROOT}/tests)

# Writes the file at path, relative to ROOT, holding the remaining arguments as its lines.
function(plant path)
    list(JOIN ARGN "\n" content)
    file(WRITE ${ROOT}/${path} "${content}\n")
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

# The report lines in the check's standard error, sorted. A report line is a whole line with no
# prefix, so an indented, prefixed, or wrapped report is not counted, while CMake's own error text,
# which it indents, is ignored.
function(report_lines errors out)
    set(lines "")
    string(REGEX MATCHALL "[^\r\n]+" allLines "${errors}")
    foreach(line IN LISTS allLines)
        if(line MATCHES "^[^ \t]+ includes [^ \t]+, which is private to [^ \t]+$")
            list(APPEND lines "${line}")
        endif()
    endforeach()
    list(SORT lines)
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# Runs the check on ROOT and fails unless its report is exactly the given lines, each as many times
# as given, and it exits with status 0 exactly when there are none.
function(expect_report)
    run_check(-DROOT=${ROOT})
    set(expected ${ARGN})
    list(SORT expected)
    if(expected AND checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0 on a tree with violations:\n${checkOutput}")
    endif()
    if(NOT expected AND NOT checkResult EQUAL 0)
        message(FATAL_ERROR "The check failed (${checkResult}) on a tree with no violation:\n"
            "${checkOutput}")
    endif()
    report_lines("${checkErrors}" actual)
    if(NOT "${actual}" STREQUAL "${expected}")
        list(JOIN expected "\n" expectedText)
        list(JOIN actual "\n" actualText)
        message(FATAL_ERROR "The check's standard error does not hold exactly the expected report "
            "lines, each whole and unprefixed.\nExpected:\n${expectedText}\nReported:\n"
            "${actualText}\nStandard error:\n${checkErrors}")
    endif()
endfunction()

if(CASE STREQUAL "planted_violation")
    plant(src/a/internal/h.h)
    plant(src/b/x.cpp "#include \"a/internal/h.h\"")
    run_check(-DROOT=${ROOT})
    if(checkResult EQUAL 0)
        message(FATAL_ERROR "The check accepted a planted violation:\n${checkOutput}")
    endif()
    string(FIND "${checkOutput}" "src/b/x.cpp" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "The check failed without naming the planted file src/b/x.cpp:\n"
            "${checkOutput}")
    endif()

elseif(CASE STREQUAL "missing_root")
    run_check()
    if(checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0 without ROOT:\n${checkOutput}")
    endif()
    string(FIND "${checkOutput}" "ROOT" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "The check failed without naming ROOT:\n${checkOutput}")
    endif()

elseif(CASE STREQUAL "scan_scope")
    plant(tests/m/internal/h.h)
    plant(src/x.cpp "#include \"tests/m/internal/h.h\"")
    plant(src/a/b/c/y.h "#include \"tests/m/internal/h.h\"")
    plant(tests/z.h "#include \"tests/m/internal/h.h\"")
    plant(tests/a/b/c/w_test.cpp "#include \"tests/m/internal/h.h\"")
    expect_report(
        "src/x.cpp includes tests/m/internal/h.h, which is private to tests/m"
        "src/a/b/c/y.h includes tests/m/internal/h.h, which is private to tests/m"
        "tests/z.h includes tests/m/internal/h.h, which is private to tests/m"
        "tests/a/b/c/w_test.cpp includes tests/m/internal/h.h, which is private to tests/m")

elseif(CASE STREQUAL "include_forms")
    plant(src/m/internal/h.h)
    plant(tests/quoted.cpp "#include \"src/m/internal/h.h\"")
    plant(tests/angled.cpp "#include <src/m/internal/h.h>")
    plant(tests/blanks.cpp " \t#\t include\t \"src/m/internal/h.h\"")
    plant(tests/no_blanks.cpp "#include<src/m/internal/h.h>")
    expect_report(
        "tests/quoted.cpp includes src/m/internal/h.h, which is private to src/m"
        "tests/angled.cpp includes src/m/internal/h.h, which is private to src/m"
        "tests/blanks.cpp includes src/m/internal/h.h, which is private to src/m"
        "tests/no_blanks.cpp includes src/m/internal/h.h, which is private to src/m")

elseif(CASE STREQUAL "commented_includes")
    plant(src/m/internal/h.h)
    plant(tests/x.cpp
        "// #include \"src/m/internal/h.h\""
        "//#include <src/m/internal/h.h>"
        "    // #include \"src/m/internal/h.h\"")
    expect_report()

elseif(CASE STREQUAL "own_directory")
    plant(src/m/internal/h.h)
    plant(src/a/x.cpp "#include \"./../m/internal/h.h\"")
    expect_report("src/a/x.cpp includes src/m/internal/h.h, which is private to src/m")

elseif(CASE STREQUAL "directories_above")
    # One include is named from a directory two levels up, the other from ROOT itself.
    plant(tests/a/support/internal/h.h)
    plant(tests/a/b/c_test.cpp "#include \"support/internal/h.h\"")
    plant(tests/m/internal/h.h)
    plant(src/x.cpp "#include \"tests/m/internal/h.h\"")
    expect_report(
        "tests/a/b/c_test.cpp includes tests/a/support/internal/h.h, which is private to tests/a/support"
        "src/x.cpp includes tests/m/internal/h.h, which is private to tests/m")

elseif(CASE STREQUAL "root_src")
    plant(src/m/internal/h.h)
    plant(tests/m/x_test.cpp "#include \"m/internal/h.h\"")
    expect_report("tests/m/x_test.cpp includes src/m/internal/h.h, which is private to src/m")

elseif(CASE STREQUAL "unnamed_includes")
    # The last include names an existing private header, but one outside ROOT.
    file(WRITE ${TREE}/outside/internal/h.h "\n")
    plant(tests/x.cpp
        "#include <vector>"
        "#include \"missing/internal/h.h\""
        "#include \"../../outside/internal/h.h\"")
    expect_report()

elseif(CASE STREQUAL "every_named_file")
    # The include names one file from its own directory and another from ROOT/src, so a check
    # that stopped at the first file found would report only one of them.
    plant(tests/m/n/internal/h.h)
    plant(src/n/internal/h.h)
    plant(tests/m/x_test.cpp "#include \"n/internal/h.h\"")
    expect_report(
        "tests/m/x_test.cpp includes tests/m/n/internal/h.h, which is private to tests/m/n"
        "tests/m/x_test.cpp includes src/n/internal/h.h, which is private to src/n")

elseif(CASE STREQUAL "anywhere_under_parent")
    plant(src/m/internal/h.h)
    plant(src/m/x.cpp "#include \"internal/h.h\"")
    plant(src/m/a/b/y.h "#include \"m/internal/h.h\"")
    plant(src/m/internal/z.cpp "#include \"h.h\"")
    plant(src/m/internal/deeper/w.cpp "#include \"../h.h\"")
    expect_report()

elseif(CASE STREQUAL "whole_components")
    # src/simulation begins with the characters of src/sim but is not under it.
    plant(src/sim/internal/h.h)
    plant(src/simulation/x.cpp "#include \"../sim/internal/h.h\"")
    expect_report("src/simulation/x.cpp includes src/sim/internal/h.h, which is private to src/sim")

elseif(CASE STREQUAL "each_parent")
    plant(src/a/internal/b/internal/h.h)
    plant(src/a/x.cpp "#include \"internal/b/internal/h.h\"")
    plant(tests/y.cpp "#include \"src/a/internal/b/internal/h.h\"")
    plant(src/a/internal/b/z.cpp "#include \"internal/h.h\"")
    expect_report(
        "src/a/x.cpp includes src/a/internal/b/internal/h.h, which is private to src/a/internal/b"
        "tests/y.cpp includes src/a/internal/b/internal/h.h, which is private to src/a"
        "tests/y.cpp includes src/a/internal/b/internal/h.h, which is private to src/a/internal/b")

elseif(CASE STREQUAL "once_per_include")
    # The include is named both from src, a directory above the including file, and from ROOT/src.
    plant(src/sim/internal/state.h)
    plant(src/app/main.cpp "#include \"sim/internal/state.h\"")
    expect_report("src/app/main.cpp includes src/sim/internal/state.h, which is private to src/sim")

elseif(CASE STREQUAL "without_tests")
    file(REMOVE_RECURSE ${ROOT}/tests)
    plant(src/a/internal/h.h)
    plant(src/b/x.cpp "#include \"../a/internal/h.h\"")
    expect_report("src/b/x.cpp includes src/a/internal/h.h, which is private to src/a")

elseif(CASE STREQUAL "without_src")
    file(REMOVE_RECURSE ${ROOT}/src)
    plant(tests/a/internal/h.h)
    plant(tests/b/x_test.cpp "#include \"../a/internal/h.h\"")
    expect_report("tests/b/x_test.cpp includes tests/a/internal/h.h, which is private to tests/a")

elseif(CASE STREQUAL "long_report_line")
    # Longer than any width CMake wraps message text to.
    set(module src/a_module_whose_name_is_long_enough/and_nested_well_below_the_source_root)
    plant(${module}/internal/a_private_header_with_a_long_name.h)
    plant(tests/an_including_directory_with_a_long_name/an_including_file_with_a_long_name_test.cpp
        "#include \"${module}/internal/a_private_header_with_a_long_name.h\"")
    expect_report("tests/an_including_directory_with_a_long_name/an_including_file_with_a_long_name_test.cpp includes ${module}/internal/a_private_header_with_a_long_name.h, which is private to ${module}")

else()
    message(FATAL_ERROR "Unknown CASE ${CASE}")
endif()
