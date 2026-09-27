cmake_minimum_required(VERSION 3.28)
project(ThemeParkJones LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Sanitizers default on for Debug where the toolchain supports them. MinGW GCC has no ASan
# runtime, so the Windows build runs without them; the Linux build is the sanitized one.
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MINGW AND NOT MSVC)
    set(TPJ_SANITIZE_DEFAULT ON)
else()
    set(TPJ_SANITIZE_DEFAULT OFF)
endif()
option(TPJ_SANITIZE "Build project targets with AddressSanitizer and UBSan" ${TPJ_SANITIZE_DEFAULT})
option(TPJ_CLANG_TIDY "Run clang-tidy on project targets during the build" OFF)
option(TPJ_TRACY "Enable the Tracy profiler client" OFF)
option(TPJ_BUILD_APP "Build the windowed application (the sim and tests build without it)" ON)
option(TPJ_BUILD_TESTS "Build the test suite" ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(CPM)
include(ProjectOptions)

if(TPJ_TRACY)
    CPMAddPackage(
        NAME Tracy
        URL https://github.com/wolfpld/tracy/archive/refs/tags/v0.14.1.tar.gz
        SYSTEM YES
        EXCLUDE_FROM_ALL YES
        OPTIONS "TRACY_ENABLE ON" "TRACY_ON_DEMAND ON")
endif()

CPMAddPackage(
    NAME EnTT
    URL https://github.com/skypjack/entt/archive/refs/tags/v4.0.0.tar.gz
    SYSTEM YES
    EXCLUDE_FROM_ALL YES)

add_subdirectory(src/core)
add_subdirectory(src/sim)
add_subdirectory(src/scenarios)

if(TPJ_BUILD_APP)
    CPMAddPackage(
        NAME SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.16.tar.gz
        SYSTEM YES
        EXCLUDE_FROM_ALL YES
        OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_EXAMPLES OFF")

    # glslang compiles the GLSL shaders to SPIR-V at build time. Only the standalone
    # compiler is needed; the optimizer would pull in SPIRV-Tools.
    CPMAddPackage(
        NAME glslang
        URL https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.tar.gz
        SYSTEM YES
        EXCLUDE_FROM_ALL YES
        OPTIONS
            "ENABLE_OPT OFF" "ENABLE_HLSL OFF" "ENABLE_SPVREMAPPER OFF"
            "GLSLANG_TESTS OFF" "GLSLANG_ENABLE_INSTALL OFF" "ENABLE_GLSLANG_BINARIES ON")

    # Dear ImGui ships no CMake build; tpj_imgui compiles it with the SDL3 and SDL_GPU backends.
    CPMAddPackage(
        NAME imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b-docking.tar.gz
        DOWNLOAD_ONLY YES)
    add_library(tpj_imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)
    target_include_directories(tpj_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    target_link_libraries(tpj_imgui PUBLIC SDL3::SDL3)

    add_subdirectory(src/render)
    add_subdirectory(src/app)
endif()

if(TPJ_BUILD_TESTS)
    CPMAddPackage(
        NAME Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz
        SYSTEM YES
        EXCLUDE_FROM_ALL YES)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)

    enable_testing()
    add_subdirectory(tests)
endif()
