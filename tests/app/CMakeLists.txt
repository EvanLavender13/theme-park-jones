# The app's tests run ThemeParkJones itself, and compare its --hash line with what tpj_scenarios
# writes for the same park file, so both are built first. They link only the sim, to read the
# errors a park file's load gives.
add_executable(tpj_app_tests
    command_line_test.cpp)
target_link_libraries(tpj_app_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
target_compile_definitions(tpj_app_tests PRIVATE
    TPJ_APP_EXECUTABLE="$<TARGET_FILE:tpj_app>"
    TPJ_SCENARIOS_EXECUTABLE="$<TARGET_FILE:tpj_scenarios>"
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks"
    TPJ_APP_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/scratch")
add_dependencies(tpj_app_tests tpj_app tpj_scenarios)
tpj_configure_target(tpj_app_tests)
# A --ticks value parsed as an enormous count would run for as long as ctest allows.
catch_discover_tests(tpj_app_tests PROPERTIES TIMEOUT 300)
