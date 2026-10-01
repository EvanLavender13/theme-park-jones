# Plants a small tree of plans for one CASE under SCRATCH, runs the placement check on it, and fails
# unless the check's exit status and report are the ones the rule gives. Run as:
#
#     cmake -DCASE=<case> -DCHECK=<check_placement.cmake> -DSCRATCH=<dir> -P placement_check_test.cmake

cmake_minimum_required(VERSION 3.28)

foreach(required CASE CHECK SCRATCH)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is not set")
    endif()
endforeach()

set(TREE ${SCRATCH}/${CASE})
set(ROOT ${TREE}/root)
file(REMOVE_RECURSE ${TREE})
file(MAKE_DIRECTORY ${ROOT})

# Writes the file at path, relative to ROOT, holding the remaining arguments as its lines.
function(plant path)
    list(JOIN ARGN "\n" content)
    file(WRITE ${ROOT}/${path} "${content}\n")
endfunction()

# Writes the file at path, relative to ROOT, holding exactly text, for content whose line endings
# or semicolons plant's list of lines cannot carry.
function(plant_text path text)
    file(WRITE ${ROOT}/${path} "${text}")
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

# Runs the check on ROOT with PLANS set to the given list, quoted so that it reaches the check as
# one CMake list, leaving its results as run_check does.
function(run_check_on_plans)
    set(plans "${ARGN}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -DROOT=${ROOT} "-DPLANS=${plans}" -P ${CHECK}
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
        if(line MATCHES "^[^ \t]+ has no Placement section$")
            list(APPEND lines "${line}")
        endif()
    endforeach()
    list(SORT lines)
    set(${out} "${lines}" PARENT_SCOPE)
endfunction()

# Runs the check on the plans after PLANS and fails unless its report is exactly the lines after
# REPORT, each as many times as given, and it exits with status 0 exactly when there are none.
function(expect_report)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "" "PLANS;REPORT")
    run_check_on_plans(${arg_PLANS})
    set(expected ${arg_REPORT})
    list(SORT expected)
    if(expected AND checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0 on plans it reports:\n${checkOutput}")
    endif()
    if(NOT expected AND NOT checkResult EQUAL 0)
        message(FATAL_ERROR "The check failed (${checkResult}) on plans it should not report:\n"
            "${checkOutput}")
    endif()
    report_lines("${checkErrors}" actual)
    if(NOT "${actual}" STREQUAL "${expected}")
        list(JOIN expected "\n" expectedText)
        list(JOIN actual "\n" actualText)
        message(FATAL_ERROR "The check's standard error does not hold exactly the expected reports, "
            "each whole and unprefixed.\nExpected:\n${expectedText}\nReported:\n${actualText}\n"
            "Standard error:\n${checkErrors}")
    endif()
endfunction()

# Fails unless the check, last run, failed naming the given text.
function(expect_failure_naming text)
    if(checkResult EQUAL 0)
        message(FATAL_ERROR "The check exited with status 0:\n${checkOutput}")
    endif()
    string(FIND "${checkOutput}" "${text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "The check failed without naming ${text}:\n${checkOutput}")
    endif()
endfunction()

if(CASE STREQUAL "missing_root")
    plant(plans/a/PLAN.md "## Placement")
    run_check(-DPLANS=plans/a/PLAN.md)
    expect_failure_naming(ROOT)

elseif(CASE STREQUAL "empty_root")
    # Run from inside the tree on a plan that has the section, so a check that took an empty ROOT
    # as the working directory would pass, and one that read the plan from the filesystem's root
    # would fail naming the plan instead of ROOT.
    plant(plans/a/PLAN.md "## Placement")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -DROOT= -DPLANS=plans/a/PLAN.md -P ${CHECK}
        WORKING_DIRECTORY ${ROOT}
        RESULT_VARIABLE checkResult
        OUTPUT_VARIABLE standardOutput
        ERROR_VARIABLE checkErrors)
    set(checkOutput "${standardOutput}${checkErrors}")
    expect_failure_naming(ROOT)

elseif(CASE STREQUAL "missing_plans")
    run_check(-DROOT=${ROOT})
    expect_failure_naming(PLANS)

elseif(CASE STREQUAL "unreadable_plan")
    plant(plans/present/PLAN.md "## Placement")
    run_check_on_plans(plans/present/PLAN.md plans/absent/PLAN.md)
    expect_failure_naming(plans/absent/PLAN.md)

elseif(CASE STREQUAL "section_lines")
    # The heading with nothing after it, with blanks after it, and as the file's last line with no
    # line ending.
    plant(plans/exact/PLAN.md
        "# Feature plan" "" "## Approach" "" "Text." "" "## Placement" "" "- A: b." "" "## Tasks")
    plant(plans/trailing_blanks/PLAN.md "# Feature plan" "" "## Placement \t " "" "## Tasks")
    plant_text(plans/last_line/PLAN.md "# Feature plan\n\n## Tasks\n\n## Placement")
    expect_report(
        PLANS plans/exact/PLAN.md plans/trailing_blanks/PLAN.md plans/last_line/PLAN.md)

elseif(CASE STREQUAL "carriage_returns")
    # Plans written on Windows end their lines with a carriage return.
    plant_text(plans/crlf/PLAN.md "# Feature plan\r\n\r\n## Placement\r\n\r\n## Tasks\r\n")
    plant_text(plans/crlf_without/PLAN.md "# Feature plan\r\n\r\n## Placements\r\n\r\n## Tasks\r\n")
    expect_report(
        PLANS plans/crlf/PLAN.md plans/crlf_without/PLAN.md
        REPORT "plans/crlf_without/PLAN.md has no Placement section")

elseif(CASE STREQUAL "utf8_lines")
    # A reader that took the bytes of a character outside ASCII as a line break would find the
    # heading inside the last two plans' lines.
    plant(plans/accent_above/PLAN.md "# Café plan" "" "## Placement" "" "## Tasks")
    plant(plans/accent_after/PLAN.md "# Feature plan" "" "## Placement é" "" "## Tasks")
    plant(plans/accent_before/PLAN.md "# Feature plan" "" "é## Placement" "" "## Tasks")
    expect_report(
        PLANS plans/accent_above/PLAN.md plans/accent_after/PLAN.md plans/accent_before/PLAN.md
        REPORT
            "plans/accent_after/PLAN.md has no Placement section"
            "plans/accent_before/PLAN.md has no Placement section")

elseif(CASE STREQUAL "other_lines")
    # A semicolon separates the elements of a CMake list, so a reader that split lines into a list
    # would find the heading inside the last plan's line.
    plant(plans/level_one/PLAN.md "# Placement" "" "## Tasks")
    plant(plans/level_three/PLAN.md "## Approach" "" "### Placement" "" "## Tasks")
    plant(plans/plural/PLAN.md "## Placements" "" "## Tasks")
    plant(plans/more_words/PLAN.md "## Placement of code" "" "## Tasks")
    plant(plans/leading_blank/PLAN.md " ## Placement" "" "## Tasks")
    plant(plans/lower_case/PLAN.md "## placement" "" "## Tasks")
    plant(plans/prose/PLAN.md "## Approach" "" "The ## Placement section places each behavior.")
    plant_text(plans/semicolon/PLAN.md "## Placement; where code goes\n\n## Tasks\n")
    expect_report(
        PLANS
            plans/level_one/PLAN.md
            plans/level_three/PLAN.md
            plans/plural/PLAN.md
            plans/more_words/PLAN.md
            plans/leading_blank/PLAN.md
            plans/lower_case/PLAN.md
            plans/prose/PLAN.md
            plans/semicolon/PLAN.md
        REPORT
            "plans/level_one/PLAN.md has no Placement section"
            "plans/level_three/PLAN.md has no Placement section"
            "plans/plural/PLAN.md has no Placement section"
            "plans/more_words/PLAN.md has no Placement section"
            "plans/leading_blank/PLAN.md has no Placement section"
            "plans/lower_case/PLAN.md has no Placement section"
            "plans/prose/PLAN.md has no Placement section"
            "plans/semicolon/PLAN.md has no Placement section")

elseif(CASE STREQUAL "once_per_plan")
    plant(plans/a/PLAN.md "## Tasks")
    plant(plans/b/PLAN.md "## Tasks")
    plant(plans/c/PLAN.md "## Placement")
    expect_report(
        PLANS plans/a/PLAN.md plans/c/PLAN.md plans/b/PLAN.md plans/a/PLAN.md plans/c/PLAN.md
        REPORT
            "plans/a/PLAN.md has no Placement section"
            "plans/b/PLAN.md has no Placement section")

elseif(CASE STREQUAL "empty_plans")
    plant(plans/a/PLAN.md "## Tasks")
    expect_report()

elseif(CASE STREQUAL "long_report_line")
    # Longer than any width CMake wraps message text to.
    set(plan plans/a_milestone_whose_name_is_long_enough_to_wrap/and_a_feature_named_at_great_length_beneath_it/PLAN.md)
    plant(${plan} "## Tasks")
    expect_report(PLANS ${plan} REPORT "${plan} has no Placement section")

else()
    message(FATAL_ERROR "Unknown CASE ${CASE}")
endif()
