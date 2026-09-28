# The sim's tests link the sim alone, which also proves the sim builds without rendering.
add_executable(tpj_sim_tests
    cycle_test.cpp
    draw_test.cpp
    entity_key_test.cpp
    fp_environment_test.cpp
    medium/carry_over_test.cpp
    medium/field_test.cpp
    medium/flow_test.cpp
    medium/network_test.cpp
    medium/stepped_field_test.cpp
    park/geometry_test.cpp
    park/intent_test.cpp
    park/new_park_test.cpp
    save_test.cpp
    schema_test.cpp
    sim_math_test.cpp
    walk_test.cpp
    world_checks_test.cpp)
target_link_libraries(tpj_sim_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
# So that tests in subdirectories, such as medium/, include the shared fixtures as support/....
target_include_directories(tpj_sim_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# The park tests compare the checked-in new park with the template's save.
target_compile_definitions(tpj_sim_tests PRIVATE TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
# Some tests restate an arithmetic law of the simulation and compare bits, so they must round each
# operation as tpj_sim does, never fusing a multiply and an add.
target_compile_options(tpj_sim_tests PRIVATE -ffp-contract=off)
tpj_configure_target(tpj_sim_tests)
catch_discover_tests(tpj_sim_tests)

# The symbol check keeps the C runtime's transcendental functions out of tpj_sim, whose results
# must not depend on the build's runtime (decision 0022).
add_test(NAME "sim symbol check passes on tpj_sim"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_sim>
        -P ${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake)

# An archive holding a call to exp, which proves the check can fail.
add_library(tpj_planted_exp STATIC support/planted_exp.cpp)
tpj_configure_target(tpj_planted_exp)
add_test(NAME "sim symbol check rejects a call to exp, naming exp and its object"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_planted_exp>
        -DCHECK=${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/support/expect_symbol_rejection.cmake)
