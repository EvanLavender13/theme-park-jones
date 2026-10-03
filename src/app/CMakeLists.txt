# The app's components that need no window, so the app's tests call them through their headers.
add_library(tpj_app_core STATIC
    frame_clock.cpp
    frame_times.cpp
    input/cursor.cpp
    input/input_map.cpp
    input/interaction.cpp
    input/orbit_camera.cpp
    options.cpp
    scene/scene_sync.cpp
    session/park_file.cpp
    session/park_file_requests.cpp
    session/park_session.cpp)
target_include_directories(tpj_app_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_app_core PUBLIC tpj_legible tpj_render tpj_sim tpj_tools PRIVATE SDL3::SDL3)
tpj_configure_target(tpj_app_core)

add_executable(tpj_app
    application.cpp
    input/platform_input.cpp
    main.cpp
    platform.cpp
    scene/scene_uploads.cpp
    session/park_dialogs.cpp
    ui/debug_panel.cpp
    ui/food_tooltip.cpp
    ui/graph_view.cpp
    ui/inspector_window.cpp
    ui/shop_context_tooltip.cpp
    ui/tool_panel.cpp
    ui/tooling_ui.cpp)
set_target_properties(tpj_app PROPERTIES OUTPUT_NAME ThemeParkJones)
target_link_libraries(tpj_app PRIVATE tpj_app_core tpj_legible tpj_render tpj_sim tpj_tools tpj_views)
tpj_configure_target(tpj_app)

if(TPJ_SANITIZE)
    target_sources(tpj_app PRIVATE sanitizer_support.cpp)
endif()

# The dialogs start in the parks folder beside the executable. It links to the source tree's, so
# saved parks survive a clean build: a junction on Windows, which needs no privilege where a
# symbolic link needs developer mode.
if(NOT EXISTS ${CMAKE_BINARY_DIR}/parks)
    if(WIN32)
        file(TO_NATIVE_PATH ${CMAKE_BINARY_DIR}/parks PARKS_LINK)
        file(TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/parks PARKS_TARGET)
        execute_process(COMMAND cmd /c mklink /J ${PARKS_LINK} ${PARKS_TARGET}
            RESULT_VARIABLE PARKS_LINK_RESULT OUTPUT_QUIET)
    else()
        file(CREATE_LINK ${CMAKE_SOURCE_DIR}/parks ${CMAKE_BINARY_DIR}/parks
            RESULT PARKS_LINK_RESULT SYMBOLIC)
    endif()
    if(NOT PARKS_LINK_RESULT EQUAL 0)
        message(FATAL_ERROR "cannot link ${CMAKE_BINARY_DIR}/parks to ${CMAKE_SOURCE_DIR}/parks")
    endif()
endif()
