# The boxes-and-tubes slice's integration tests run the checked-in park files through the
# simulation's public interface, and read them through tpj_legible, which links the sim alone.
add_executable(tpj_integration_tests
    food_availability_test.cpp
    slice_parks_test.cpp)
target_link_libraries(tpj_integration_tests PRIVATE tpj_legible tpj_sim Catch2::Catch2WithMain)
target_compile_definitions(tpj_integration_tests PRIVATE
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_integration_tests)
# Every test finishes within TIMEOUT seconds under the parallel ctest run; a slower one fails.
catch_discover_tests(tpj_integration_tests PROPERTIES TIMEOUT 10)
