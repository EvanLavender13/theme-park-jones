# Explanations of the simulation for display, computed from its fields, networks, and intent. They
# link the simulation alone, so tests compute them without a window (principle 10).
add_library(tpj_legible STATIC
    food.cpp
    inspect.cpp
    path_place.cpp
    park_summary.cpp
    preview.cpp)
target_include_directories(tpj_legible PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_legible PUBLIC tpj_sim)
tpj_configure_target(tpj_legible)
