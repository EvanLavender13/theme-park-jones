# The tools' tests link tpj_tools alone, which links the sim alone, so they drive the tools with
# synthetic input and no window (principle 10).
add_executable(tpj_tools_tests
    path_tool_test.cpp
    picking_test.cpp
    tools_test.cpp)
target_link_libraries(tpj_tools_tests PRIVATE tpj_tools Catch2::Catch2WithMain)
# So that the tests include the shared fixtures as support/....
target_include_directories(tpj_tools_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
tpj_configure_target(tpj_tools_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_tools_tests PROPERTIES TIMEOUT 300)
