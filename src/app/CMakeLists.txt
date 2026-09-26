add_executable(tpj_app
    debug_panel.cpp
    main.cpp
    orbit_camera.cpp)
set_target_properties(tpj_app PROPERTIES OUTPUT_NAME ThemeParkJones)
target_link_libraries(tpj_app PRIVATE tpj_render tpj_sim)
tpj_configure_target(tpj_app)

if(TPJ_SANITIZE)
    target_sources(tpj_app PRIVATE sanitizer_support.cpp)
endif()
