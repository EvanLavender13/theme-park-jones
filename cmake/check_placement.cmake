# Fails when a plan it is given has no Placement section (decision 0027): no line that is
# "## Placement" followed by nothing but blanks. The pre-commit hook runs it on the staged versions
# of the PLAN.md files a commit adds.
#
# Usage: cmake -DROOT=<tree> -DPLANS=<plans> -P check_placement.cmake
# PLANS is a list of paths relative to ROOT. On failure it prints a line
# "<plan> has no Placement section" for each such plan, with its path as given.

cmake_minimum_required(VERSION 3.28)

if(NOT ROOT OR NOT DEFINED PLANS)
    message(FATAL_ERROR "usage: cmake -DROOT=<tree> -DPLANS=<plans> -P check_placement.cmake")
endif()

# ROOT as an absolute, normalized path with no trailing separator.
cmake_path(ABSOLUTE_PATH ROOT NORMALIZE)
string(REGEX REPLACE "(.)/$" "\\1" ROOT "${ROOT}")

# file(STRINGS) drops carriage returns, so a CRLF plan reads as an LF one. Read as UTF-8, a
# character outside ASCII stays in its line instead of splitting it.
set(PLACEMENT_LINE "^## Placement[ \t]*$")
set(report "")
foreach(plan IN LISTS PLANS)
    set(path "${ROOT}/${plan}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Cannot read the plan ${plan}")
    endif()
    file(STRINGS "${path}" placement ENCODING UTF-8 REGEX "${PLACEMENT_LINE}")
    if(NOT placement)
        list(APPEND report "${plan} has no Placement section")
    endif()
endforeach()

# NOTICE prints each line as it is; FATAL_ERROR would wrap long ones.
if(report)
    list(REMOVE_DUPLICATES report)
    foreach(line IN LISTS report)
        message(NOTICE "${line}")
    endforeach()
    message(FATAL_ERROR "New plans need a Placement section (decision 0027).")
endif()
message(STATUS "Each plan has a Placement section")
