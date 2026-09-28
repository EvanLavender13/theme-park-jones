include(Catch)

# Tests mirror src/: each module's tests live in the directory named for it, with their shared
# fixtures in its support/ subdirectory. Nothing but directories and this file sits at the root.
file(GLOB TPJ_STRAY_TEST_FILES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
if(TPJ_STRAY_TEST_FILES)
    message(FATAL_ERROR "Test sources belong in the tests/ directory of the module they test, "
        "not at the tests/ root: ${TPJ_STRAY_TEST_FILES}")
endif()

add_subdirectory(sim)
add_subdirectory(scenarios)
add_subdirectory(checks)
# The renderer and the app build only with the windowed application.
if(TPJ_BUILD_APP)
    add_subdirectory(render)
    add_subdirectory(app)
endif()
