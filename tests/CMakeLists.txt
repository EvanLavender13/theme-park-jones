include(Catch)

# Tests link the sim alone, which also proves the sim builds without rendering.
add_executable(tpj_tests
    entity_key_test.cpp
    placeholder_test.cpp
    schema_test.cpp
    walk_test.cpp
    world_checks_test.cpp)
target_include_directories(tpj_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tpj_tests PRIVATE tpj_sim Catch2::Catch2WithMain)
tpj_configure_target(tpj_tests)
catch_discover_tests(tpj_tests)

# Field types a visitFields may not list fail to compile. Each case builds
# compile_checks/field_type.cpp with one field type: case 0 (double) must build, which shows the
# file itself is sound, and every other case must fail. clang-tidy is off for these targets so
# that only the compiler decides. The targets are built only by these tests, one at a time,
# because they share the build tree.
function(tpj_add_field_type_check field_case field_type expect_failure)
    set(target tpj_field_type_check_${field_case})
    add_executable(${target} EXCLUDE_FROM_ALL compile_checks/field_type.cpp)
    target_compile_definitions(${target} PRIVATE TPJ_FIELD_CASE=${field_case})
    target_link_libraries(${target} PRIVATE tpj_sim)
    tpj_configure_target(${target})
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "")
    if(expect_failure)
        set(test_name "compile check: a ${field_type} field fails to compile")
    else()
        set(test_name "compile check: a ${field_type} field compiles")
    endif()
    add_test(NAME "${test_name}"
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${target})
    set_tests_properties("${test_name}" PROPERTIES
        WILL_FAIL ${expect_failure}
        RESOURCE_LOCK tpj_build_tree)
endfunction()

tpj_add_field_type_check(0 "double" FALSE)
tpj_add_field_type_check(1 "float" TRUE)
tpj_add_field_type_check(2 "std::vector<bool>" TRUE)
tpj_add_field_type_check(3 "std::string" TRUE)
tpj_add_field_type_check(4 "std::vector<float>" TRUE)
