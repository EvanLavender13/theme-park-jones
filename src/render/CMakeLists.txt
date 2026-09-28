# Rendering through SDL_GPU. Shaders are GLSL compiled to SPIR-V at build time, so SDL_GPU
# selects its Vulkan backend on every platform (decision 0009).
add_library(tpj_render STATIC
    math.cpp
    park_mesh.cpp
    picking.cpp
    renderer.cpp)
target_include_directories(tpj_render PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_render PUBLIC tpj_core tpj_sim SDL3::SDL3 tpj_imgui)
tpj_configure_target(tpj_render)

set(TPJ_SHADER_SOURCES
    park.vert
    park.frag
    terrain.vert
    terrain.frag)
set(TPJ_SHADER_OUTPUTS)
foreach(shader ${TPJ_SHADER_SOURCES})
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader})
    set(output ${CMAKE_BINARY_DIR}/shaders/${shader}.spv)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
        COMMAND glslang-standalone -V --quiet -o ${output} ${source}
        DEPENDS ${source} glslang-standalone
        COMMENT "Compiling shader ${shader}"
        VERBATIM)
    list(APPEND TPJ_SHADER_OUTPUTS ${output})
endforeach()
add_custom_target(tpj_shaders DEPENDS ${TPJ_SHADER_OUTPUTS})
add_dependencies(tpj_render tpj_shaders)
