# The scenario runner's tests link the runner's library alone. The executable's tests run
# tpj_scenarios itself, so it is built first.
add_executable(tpj_scenarios_tests
    food_shop_test.cpp
    main_test.cpp
    park_edits_test.cpp
    runner_test.cpp
    scenarios_test.cpp
    slice_parks_test.cpp
    stalls_test.cpp)
target_include_directories(tpj_scenarios_tests PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tpj_scenarios_tests PRIVATE tpj_scenarios_lib Catch2::Catch2WithMain)
target_compile_definitions(tpj_scenarios_tests PRIVATE
    TPJ_SCENARIOS_EXECUTABLE="$<TARGET_FILE:tpj_scenarios>"
    TPJ_SCENARIOS_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/scratch"
    # The slice parks' tests read the checked-in parks.
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
add_dependencies(tpj_scenarios_tests tpj_scenarios)
tpj_configure_target(tpj_scenarios_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_scenarios_tests PROPERTIES TIMEOUT 300)

# The scenarios library is simulation code, so the C runtime's transcendental functions stay out
# of it as they do out of tpj_sim (decision 0022).
add_test(NAME "sim symbol check passes on tpj_scenarios_lib"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_scenarios_lib>
        -P ${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake)
