# Fails when a static archive references a C runtime transcendental function, whose results differ
# between the builds' runtimes (decision 0022). Exactly rounded functions such as sqrt, floor, and
# fmod stay allowed: GCC calls them legitimately, and every runtime gives the same result.
#
# Usage: cmake -DNM=<the toolchain's nm> -DARCHIVE=<archive> -P check_sim_symbols.cmake
# On failure it prints a line "<symbol> referenced by <object>" for each denied symbol.

# A -P script takes its policies from here; IN_LIST needs CMP0057.
cmake_minimum_required(VERSION 3.28)

if(NOT NM OR NOT ARCHIVE)
    message(FATAL_ERROR "usage: cmake -DNM=<nm> -DARCHIVE=<archive> -P check_sim_symbols.cmake")
endif()

set(TRANSCENDENTALS
    exp exp2 exp10 expm1 log log2 log10 log1p pow
    sin cos tan asin acos atan atan2 sinh cosh tanh asinh acosh atanh sincos
    cbrt hypot erf erfc tgamma lgamma
    cexp clog cpow csqrt csin ccos ctan casin cacos catan csinh ccosh ctanh casinh cacosh catanh
    cabs carg)
set(DENIED)
foreach(name IN LISTS TRANSCENDENTALS)
    foreach(variant ${name} ${name}f ${name}l)
        list(APPEND DENIED ${variant} __${variant}_finite)
    endforeach()
endforeach()
foreach(variant lgamma_r lgammaf_r lgammal_r)
    list(APPEND DENIED ${variant} __${variant}_finite)
endforeach()

execute_process(COMMAND ${NM} -u ${ARCHIVE}
    OUTPUT_VARIABLE listing ERROR_VARIABLE errors RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${NM} -u ${ARCHIVE} failed: ${errors}")
endif()

# nm prints each object's name followed by a colon, then one line per undefined symbol. MinGW
# may reference an imported function through an __imp_ symbol.
string(REPLACE "\n" ";" lines "${listing}")
set(object "")
set(report "")
foreach(line IN LISTS lines)
    if(line MATCHES "^(.+):$")
        set(object "${CMAKE_MATCH_1}")
    elseif(line MATCHES "^ *[Uw] +(__imp_)?([A-Za-z0-9_]+)$")
        if(CMAKE_MATCH_2 IN_LIST DENIED)
            string(APPEND report "${CMAKE_MATCH_2} referenced by ${object}\n")
        endif()
    endif()
endforeach()

if(report)
    message(FATAL_ERROR "${ARCHIVE} references C runtime transcendental functions:\n${report}")
endif()
message(STATUS "${ARCHIVE} references no C runtime transcendental function")
