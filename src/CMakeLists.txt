file(GLOB_RECURSE MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(main ${MAIN_SRC})
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)