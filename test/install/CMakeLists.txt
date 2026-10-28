# © Joseph Cameron - MIT License

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(jfc-http_consumer LANGUAGES CXX)

find_package(jfc-http 0.75 REQUIRED)

add_executable(consumer main.cpp)

target_link_libraries(consumer PRIVATE jfc::http)

set_target_properties(consumer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<0:>")
