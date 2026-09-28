# Park files, apart from the window, so the app's tests read and write them without a display.
add_library(tpj_park_files STATIC
    park_file.cpp)
target_include_directories(tpj_park_files PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_park_files PUBLIC tpj_sim PRIVATE SDL3::SDL3)
tpj_configure_target(tpj_park_files)

add_executable(tpj_app
    debug_panel.cpp
    main.cpp
    orbit_camera.cpp
    tool_panel.cpp)
set_target_properties(tpj_app PROPERTIES OUTPUT_NAME ThemeParkJones)
target_link_libraries(tpj_app PRIVATE tpj_park_files tpj_render tpj_sim tpj_tools)
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
