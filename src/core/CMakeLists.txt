# Header-only utilities shared by every layer. Nothing here may depend on rendering.
add_library(tpj_core INTERFACE)
target_include_directories(tpj_core INTERFACE ${CMAKE_SOURCE_DIR}/src)

if(TPJ_TRACY)
    target_link_libraries(tpj_core INTERFACE Tracy::TracyClient)
    target_compile_definitions(tpj_core INTERFACE TPJ_TRACY)
endif()
