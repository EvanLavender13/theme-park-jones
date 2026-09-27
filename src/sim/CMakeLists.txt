# The simulation. It must build and run without any rendering, windowing, or input library
# (principle 10), so this target links nothing beyond tpj_core and EnTT.
add_library(tpj_sim STATIC
    cycle.cpp
    draw.cpp
    save.cpp
    schema.cpp
    walk.cpp
    world.cpp)
target_include_directories(tpj_sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_sim PUBLIC tpj_core EnTT::EnTT)
tpj_configure_target(tpj_sim)

# Results must be bit-identical across the Windows and Linux builds (decision 0022).
target_compile_options(tpj_sim PRIVATE -ffp-contract=off)
