# The runtime runner (plans/scalable-runtime). It reads the clock to time each runtime stage on a
# park file and is not simulation code, so it builds with the project's usual flags.
add_library(tpj_bench_lib STATIC
    options.cpp
    park_file.cpp
    stages.cpp
    timing.cpp)
target_include_directories(tpj_bench_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_bench_lib PUBLIC tpj_views tpj_legible tpj_render tpj_sim)
tpj_configure_target(tpj_bench_lib)

add_executable(tpj_bench main.cpp)
target_link_libraries(tpj_bench PRIVATE tpj_bench_lib)
tpj_configure_target(tpj_bench)
