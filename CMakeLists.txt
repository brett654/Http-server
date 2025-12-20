cmake_minimum_required(VERSION 3.10)
project(httpserver C)

set(SOURCES
    src/main.c
    src/network.c
    src/http.c
)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/public DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_executable(httpserver ${SOURCES})