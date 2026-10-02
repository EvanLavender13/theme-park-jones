# What the app shows, joined from render's mesh builders and legible's explanations, which share a
# layer and so cannot join themselves (decision 0027). Built on the CPU, so tests need no GPU.
add_library(tpj_views STATIC
    food_overlay.cpp)
target_include_directories(tpj_views PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_views PUBLIC tpj_legible tpj_render tpj_sim)
tpj_configure_target(tpj_views)
