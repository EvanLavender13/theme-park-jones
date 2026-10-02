# The runner's tests link tpj_bench_lib, to compute in-process what tpj_bench writes, and run
# tpj_bench itself, so it is built first. No test checks a time.
add_executable(tpj_bench_tests
    command_line_test.cpp
    options_test.cpp
    stages_test.cpp
    timing_test.cpp)
target_link_libraries(tpj_bench_tests PRIVATE tpj_bench_lib Catch2::Catch2WithMain)
# The stage tests time tests/parks/warm.park, whose guests and stocked shop make every stage's
# result depend on its work, and tests/parks/new.park, which holds no box; the command line tests
# run tests/parks/fed.park.
target_compile_definitions(tpj_bench_tests PRIVATE
    TPJ_BENCH_EXECUTABLE="$<TARGET_FILE:tpj_bench>"
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks"
    TPJ_BENCH_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/scratch")
add_dependencies(tpj_bench_tests tpj_bench)
tpj_configure_target(tpj_bench_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_bench_tests PROPERTIES TIMEOUT 300)
