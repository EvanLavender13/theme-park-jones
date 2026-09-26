include(Catch)

# Tests link the sim alone, which also proves the sim builds without rendering.
add_executable(tpj_tests
    placeholder_test.cpp)
target_link_libraries(tpj_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
tpj_configure_target(tpj_tests)
catch_discover_tests(tpj_tests)
