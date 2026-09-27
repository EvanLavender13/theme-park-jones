# The sim's tests link the sim alone, which also proves the sim builds without rendering.
add_executable(tpj_sim_tests
    cycle_test.cpp
    draw_test.cpp
    entity_key_test.cpp
    fp_environment_test.cpp
    medium/network_test.cpp
    save_test.cpp
    schema_test.cpp
    sim_math_test.cpp
    walk_test.cpp
    world_checks_test.cpp)
target_link_libraries(tpj_sim_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
# So that tests in subdirectories, such as medium/, include the shared fixtures as support/....
target_include_directories(tpj_sim_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
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
