# The legible module's tests link tpj_legible alone, which links the sim alone, so they compute
# explanations of a world without a window (principle 10).
add_executable(tpj_legible_tests
    food_test.cpp
    path_place_test.cpp
    preview_test.cpp)
target_link_libraries(tpj_legible_tests PRIVATE tpj_legible Catch2::Catch2WithMain)
# The availability tests read tests/parks/warm.park, whose shop has guests queued, and the preview
# tests edit it, since its guests have left hungry footfall on its paths.
target_compile_definitions(tpj_legible_tests PRIVATE TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_legible_tests)
# Every test finishes within TIMEOUT seconds under the parallel ctest run; a slower one fails.
catch_discover_tests(tpj_legible_tests PROPERTIES TIMEOUT 10)
