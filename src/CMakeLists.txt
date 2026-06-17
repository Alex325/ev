file(GLOB_RECURSE MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(ev ${MAIN_SRC})
target_include_directories(ev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(ev m)