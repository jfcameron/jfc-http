# © Joseph Cameron - All Rights Reserved

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
endif()

jfc_add_tests(
    C++_STANDARD 20
    C_STANDARD 90

    TEST_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/cancel_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/context_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/curl_request_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/dispatch_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/handler_exception_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/headers_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/null_context_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/response_handler_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/share_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/status_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/streaming_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/threading_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/user_agent_test.cpp"

    INCLUDE_DIRECTORIES

        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/include"
        "${CMAKE_CURRENT_LIST_DIR}/../impl/null/include"

        "${CURL_INCLUDE_DIR}"

    LIBRARIES
        jfchttp
        ${CMAKE_THREAD_LIBS_INIT}

    DEPENDENCIES
        "jfchttp"
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)

