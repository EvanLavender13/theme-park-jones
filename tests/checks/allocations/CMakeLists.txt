# The allocation check (decision 0041). It is an executable of its own because it replaces
# operator new and delete for the whole program.
add_executable(tpj_allocation_check allocation_check.cpp watch.cpp)
target_link_libraries(tpj_allocation_check PRIVATE tpj_app_core tpj_sim)
target_compile_definitions(tpj_allocation_check PRIVATE
    TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
tpj_configure_target(tpj_allocation_check)
add_test(NAME "allocation check: steady play on full.park frees no block it allocated"
    COMMAND tpj_allocation_check)
# A run past TIMEOUT seconds has hung.
set_tests_properties("allocation check: steady play on full.park frees no block it allocated"
    PROPERTIES TIMEOUT 1800)
