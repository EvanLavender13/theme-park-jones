# The integration tests run every capability together on the checked-in parks. They link the sim
# alone, without a window (principle 10).
add_executable(tpj_integration_tests
    food_loop_test.cpp)
# So that the tests include their shared fixtures as support/....
target_include_directories(tpj_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpj_integration_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
# The tests open the parks of tests/parks/ by name.
target_compile_definitions(tpj_integration_tests PRIVATE
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_integration_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_integration_tests PROPERTIES TIMEOUT 300)
