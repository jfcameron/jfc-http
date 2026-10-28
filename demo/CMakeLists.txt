# © Joseph Cameron - MIT License

project(jfchttp-demo)

add_executable(jfchttp-demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_link_libraries(jfchttp-demo PRIVATE jfchttp)

target_compile_features(jfchttp-demo PRIVATE cxx_std_20)

set_property(TARGET jfchttp-demo PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET jfchttp-demo PROPERTY CXX_STANDARD_REQUIRED ON)

