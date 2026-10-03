# The sim's tests link the sim alone, which also proves the sim builds without rendering.
add_executable(tpj_sim_tests
    cycle_test.cpp
    draw_test.cpp
    entity_key_test.cpp
    fp_environment_test.cpp
    guests/add_guest_test.cpp
    guests/arrivals_test.cpp
    guests/choice_test.cpp
    guests/footfall_test.cpp
    guests/guest_edits_test.cpp
    guests/guest_options_test.cpp
    guests/visits_test.cpp
    guests/walks_test.cpp
    medium/carry_over_test.cpp
    medium/field_index_test.cpp
    medium/field_sampling_test.cpp
    medium/field_test.cpp
    medium/flow_test.cpp
    medium/kept_field_test.cpp
    medium/network_test.cpp
    medium/source_sample_test.cpp
    medium/stepped_field_test.cpp
    operations/food_offer_test.cpp
    operations/operations_edits_test.cpp
    operations/operations_test.cpp
    operations/service_test.cpp
    operations/shop_records_test.cpp
    park/edit_sequences_test.cpp
    park/edits_test.cpp
    park/geometry_test.cpp
    park/intent_test.cpp
    park/sketch_park_test.cpp
    park/validity_test.cpp
    routes/connections_test.cpp
    routes/network_edits_test.cpp
    routes/route_distance_test.cpp
    routes/networks_test.cpp
    routes/previous_networks_test.cpp
    routes/route_entry_test.cpp
    routes/routes_park_test.cpp
    save_test.cpp
    schema_test.cpp
    sim_math_test.cpp
    walk_test.cpp
    world_checks_test.cpp)
target_link_libraries(tpj_sim_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
# So that tests in subdirectories, such as medium/, include the shared fixtures as support/....
target_include_directories(tpj_sim_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# The park tests read the checked-in parks.
target_compile_definitions(tpj_sim_tests PRIVATE TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
# Some tests restate an arithmetic law of the simulation and compare bits, so they must round each
# operation as tpj_sim does, never fusing a multiply and an add.
target_compile_options(tpj_sim_tests PRIVATE -ffp-contract=off)
tpj_configure_target(tpj_sim_tests)
# A test that runs past TIMEOUT seconds has hung, and fails.
catch_discover_tests(tpj_sim_tests PROPERTIES TIMEOUT 300)

# The symbol check keeps the C runtime's transcendental functions out of tpj_sim, whose results
# must not depend on the build's runtime (decision 0022).
add_test(NAME "sim symbol check passes on tpj_sim"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_sim>
        -P ${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake)

# An archive holding a call to exp, which proves the check can fail.
add_library(tpj_planted_exp STATIC support/planted_exp.cpp)
tpj_configure_target(tpj_planted_exp)
add_test(NAME "sim symbol check rejects a call to exp, naming exp and its object"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_planted_exp>
        -DCHECK=${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/support/expect_symbol_rejection.cmake)

# The scenarios library is simulation code too, so the same check runs on its archive.
add_test(NAME "sim symbol check passes on tpj_scenarios_lib"
    COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DARCHIVE=$<TARGET_FILE:tpj_scenarios_lib>
        -P ${CMAKE_SOURCE_DIR}/cmake/check_sim_symbols.cmake)
