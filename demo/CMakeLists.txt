# © Joseph Cameron - All Rights Reserved

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
endif()

jfc_project(executable
    NAME "jfchttp-demo"
    VERSION 1.0
    DESCRIPTION "jfc-http demonstration"
    C++_STANDARD 20
    C_STANDARD 90

    SOURCE_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp

    PRIVATE_INCLUDE_DIRECTORIES
        "${jfchttp_INCLUDE_DIRECTORIES}"

    LIBRARIES
        "${jfchttp_LIBRARIES}"
        ${CMAKE_THREAD_LIBS_INIT}

    DEPENDENCIES
        "jfchttp"
)

set_property(TARGET jfchttp-demo PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET jfchttp-demo PROPERTY CXX_STANDARD_REQUIRED ON)

