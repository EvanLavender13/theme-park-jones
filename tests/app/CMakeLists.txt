# The app's tests run ThemeParkJones itself, and compare its --hash line with what tpj_scenarios
# writes for the same park file, so both are built first. They link the sim and tpj_app_core, to
# read the errors a park file's load gives and to call the app's window-free components, and SDL,
# to make the events input mapping takes.
add_executable(tpj_app_tests
    command_line_test.cpp
    frame_clock_test.cpp
    input/cursor_test.cpp
    input/input_map_test.cpp
    input/interaction_test.cpp
    options_test.cpp
    scene/scene_sync_test.cpp
    session/park_file_requests_test.cpp
    session/park_file_test.cpp
    session/park_session_test.cpp)
target_link_libraries(tpj_app_tests PRIVATE tpj_app_core tpj_sim SDL3::SDL3 Catch2::Catch2WithMain)
target_compile_definitions(tpj_app_tests PRIVATE
    TPJ_APP_EXECUTABLE="$<TARGET_FILE:tpj_app>"
    TPJ_SCENARIOS_EXECUTABLE="$<TARGET_FILE:tpj_scenarios>"
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks"
    TPJ_APP_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/scratch")
add_dependencies(tpj_app_tests tpj_app tpj_scenarios)
tpj_configure_target(tpj_app_tests)
# A --ticks value parsed as an enormous count would run for as long as ctest allows.
catch_discover_tests(tpj_app_tests PROPERTIES TIMEOUT 300)
