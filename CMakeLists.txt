cmake_minimum_required(VERSION 3.10)
project(httpserver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    #src/main.c
    #src/network.c
    src/http.c
    src/client.cpp
    src/listener.cpp
    src/main.cpp
    src/http.cpp
)

execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/public
    ${CMAKE_CURRENT_BINARY_DIR}/public)

add_executable(httpserver ${SOURCES})