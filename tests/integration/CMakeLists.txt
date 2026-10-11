# The integration tests run every capability together on the checked-in parks. They link the sim
# and the tools, which need no window, so they run headless (principle 10).
add_executable(tpj_integration_tests
    crossing_split_test.cpp
    frontage_test.cpp
    inserted_point_test.cpp
    opening_hours_test.cpp
    shop_stock_test.cpp
    supply_route_test.cpp
    toilet_block_test.cpp
    undo_test.cpp)
# So that the tests include their shared fixtures as support/....
target_include_directories(tpj_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpj_integration_tests PRIVATE tpj_sim tpj_tools Catch2::Catch2WithMain)
# The tests open the parks of tests/parks/ by name.
target_compile_definitions(tpj_integration_tests PRIVATE
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_integration_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_integration_tests PROPERTIES TIMEOUT 300)
