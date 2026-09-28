# The park's editing tools. They link the simulation alone, so tests drive them without a window
# (principle 10).
add_library(tpj_tools STATIC
    tools.cpp)
target_include_directories(tpj_tools PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_tools PUBLIC tpj_sim)
tpj_configure_target(tpj_tools)
