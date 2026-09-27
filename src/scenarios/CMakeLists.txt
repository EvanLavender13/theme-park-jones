# The scenario runner (decision 0022). The library holds the scenarios and the output functions,
# and is simulation code: it builds with tpj_sim's floating-point flags, and only main reads the
# clock. It reads the generated draw and math tables in tests/sim/support.
add_library(tpj_scenarios_lib STATIC
    beacons.cpp
    runner.cpp
    scenarios.cpp
    walkers.cpp)
target_include_directories(tpj_scenarios_lib PUBLIC ${CMAKE_SOURCE_DIR}/src PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tpj_scenarios_lib PUBLIC tpj_sim)
tpj_configure_target(tpj_scenarios_lib)
target_compile_options(tpj_scenarios_lib PRIVATE -ffp-contract=off)

add_executable(tpj_scenarios main.cpp)
target_link_libraries(tpj_scenarios PRIVATE tpj_scenarios_lib)
tpj_configure_target(tpj_scenarios)
