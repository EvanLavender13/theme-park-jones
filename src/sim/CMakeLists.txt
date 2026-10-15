# The simulation. It must build and run without any rendering, windowing, or input library
# (principle 10), so this target links nothing beyond tpj_core.
add_library(tpj_sim STATIC
    clock.cpp
    cycle.cpp
    draw.cpp
    furniture/furniture.cpp
    guests/footfall.cpp
    guests/ground.cpp
    guests/guests.cpp
    guests/presence.cpp
    guests/route_ahead.cpp
    medium/flow.cpp
    medium/network.cpp
    musl_exp.cpp
    musl_log.cpp
    operations/facilities.cpp
    operations/operations.cpp
    operations/seats.cpp
    park/curves.cpp
    park/edits.cpp
    park/geometry.cpp
    park/ground_units.cpp
    park/intent.cpp
    park/layout.cpp
    park/meetings.cpp
    park/paving.cpp
    park/surface.cpp
    park/surface_units.cpp
    park/unit_reads.cpp
    park_schema.cpp
    routes/best_places.cpp
    routes/connections.cpp
    routes/networks.cpp
    routes/route_distance.cpp
    routes/walked_surface.cpp
    save.cpp
    schema.cpp
    unit_space.cpp
    walk.cpp
    wayfinding/best_seats.cpp
    world.cpp)
target_include_directories(tpj_sim PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_sim PUBLIC tpj_core)
tpj_configure_target(tpj_sim)

# Results must be bit-identical across the Windows and Linux builds (decision 0022).
target_compile_options(tpj_sim PRIVATE -ffp-contract=off)
