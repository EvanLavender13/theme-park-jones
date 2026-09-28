# The renderer's tests cover what it builds on the CPU, so they need no GPU.
add_executable(tpj_render_tests
    ghost_mesh_test.cpp
    park_mesh_test.cpp
    picking_test.cpp)
target_link_libraries(tpj_render_tests PRIVATE tpj_render Catch2::Catch2WithMain)
tpj_configure_target(tpj_render_tests)
catch_discover_tests(tpj_render_tests)
