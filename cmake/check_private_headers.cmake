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
