# The integration tests run every capability together on the checked-in parks. They link
# tpj_legible, which links the sim alone, so they read food availability and previews as the app
# does, without a window (principle 10).
add_executable(tpj_integration_tests
    food_loop_test.cpp
    park_standards_test.cpp
    preview_test.cpp)
# So that the tests include their shared fixtures as support/....
target_include_directories(tpj_integration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpj_integration_tests PRIVATE tpj_legible Catch2::Catch2WithMain)
# The standards run over every park file in tests/parks/, and the other tests open parks by name.
target_compile_definitions(tpj_integration_tests PRIVATE
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_integration_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_integration_tests PROPERTIES TIMEOUT 300)
