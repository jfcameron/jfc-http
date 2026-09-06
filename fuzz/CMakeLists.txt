# © Joseph Cameron - All Rights Reserved

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
endif()

jfc_add_fuzz_tests(
    C++_STANDARD 20
    C_STANDARD 90

    FUZZ_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/header_fuzz.cpp"

    CORPUS_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/corpus/headers"

    INCLUDE_DIRECTORIES
        "${jfchttp_INCLUDE_DIRECTORIES}"

        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/include"

        "${CURL_INCLUDE_DIR}"

    LIBRARIES
        "${jfchttp_LIBRARIES}"
        ${CMAKE_THREAD_LIBS_INIT}

    DEPENDENCIES
        "jfchttp"
)

