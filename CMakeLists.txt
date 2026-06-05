cmake_minimum_required(VERSION 3.10)
project(Editor-Visual)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/../../Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../../Release")

add_compile_options($<$<CONFIG:Release>:-O3> $<$<CONFIG:Release>:-DNDEBUG> $<$<CONFIG:Release>:-flto> $<$<CONFIG:Debug>:-Wall> $<$<CONFIG:Debug>:-fsanitize=address>)
add_link_options($<$<CONFIG:Debug>:-fsanitize=address>)

add_subdirectory(src)