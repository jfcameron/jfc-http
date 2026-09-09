# © Joseph Cameron - All Rights Reserved

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
endif()

project(jfchttp-demo)

add_executable(jfchttp-demo
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_link_libraries(jfchttp-demo PRIVATE jfchttp ${CMAKE_THREAD_LIBS_INIT})

target_compile_features(jfchttp-demo PRIVATE cxx_std_20)

set_property(TARGET jfchttp-demo PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET jfchttp-demo PROPERTY CXX_STANDARD_REQUIRED ON)

