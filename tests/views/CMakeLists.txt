# The views' tests build what the app shows on the CPU, so they need no GPU.
add_executable(tpj_views_tests
    food_overlay_test.cpp)
target_link_libraries(tpj_views_tests PRIVATE tpj_views Catch2::Catch2WithMain)
# The food overlay tests shade tests/parks/warm.park, whose shop is supplied, so its paths have food.
target_compile_definitions(tpj_views_tests PRIVATE TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_views_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_views_tests PROPERTIES TIMEOUT 300)
