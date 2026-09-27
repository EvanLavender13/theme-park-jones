# Runs the symbol check on ARCHIVE, an archive whose planted_exp object calls the C runtime's exp,
# and fails unless the check fails and its report names exp and that object. Run as:
#
#     cmake -DNM=<nm> -DARCHIVE=<archive> -DCHECK=<check_sim_symbols.cmake> -P expect_symbol_rejection.cmake

foreach(required NM ARCHIVE CHECK)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is not set")
    endif()
endforeach()

execute_process(
    COMMAND ${CMAKE_COMMAND} -DNM=${NM} -DARCHIVE=${ARCHIVE} -P ${CHECK}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)

if(result EQUAL 0)
    message(FATAL_ERROR "The symbol check accepted an archive that calls exp:\n${output}")
endif()
# exp itself, not a longer name that ends in exp.
if(NOT output MATCHES "(^|[^A-Za-z0-9_])exp referenced by [^\n]*planted_exp")
    message(FATAL_ERROR "The symbol check failed without reporting 'exp referenced by' the "
        "planted_exp object:\n${output}")
endif()
