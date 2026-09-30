# The renderer's tests cover what it builds on the CPU, so they need no GPU.
add_executable(tpj_render_tests
    food_overlay_test.cpp
    ghost_mesh_test.cpp
    graph_overlay_test.cpp
    guest_mesh_test.cpp
    park_mesh_test.cpp
    picking_test.cpp)
target_link_libraries(tpj_render_tests PRIVATE tpj_render Catch2::Catch2WithMain)
# The walkway tests draw tests/parks/routes.park, whose doors connect to both networks, and the food
# overlay tests draw tests/parks/fed.park.
target_compile_definitions(tpj_render_tests PRIVATE TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_render_tests)
# Every test finishes within TIMEOUT seconds under the parallel ctest run; a slower one fails.
catch_discover_tests(tpj_render_tests PROPERTIES TIMEOUT 10)
