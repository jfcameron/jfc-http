# © Joseph Cameron - All Rights Reserved

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(Threads REQUIRED)
endif()

if (EMSCRIPTEN)
    set(_jfc_http_test_sources
        "${CMAKE_CURRENT_LIST_DIR}/fetch_context_test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/null_context_test.cpp")

    set(_jfc_http_test_includes "${CMAKE_CURRENT_LIST_DIR}/../impl/fetch/include")
else()
    set(_jfc_http_test_sources
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
        "${CMAKE_CURRENT_LIST_DIR}/user_agent_test.cpp")

    set(_jfc_http_test_includes
        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/include"
        "${CURL_INCLUDE_DIR}")
endif()

jfc_add_tests(
    C++_STANDARD 20

    TEST_SOURCE_FILES
        ${_jfc_http_test_sources}

    INCLUDE_DIRECTORIES

        ${_jfc_http_test_includes}
        "${CMAKE_CURRENT_LIST_DIR}/../impl/null/include"

    LIBRARIES
        jfchttp
        ${CMAKE_THREAD_LIBS_INIT}

    DEPENDENCIES
        "jfchttp"
)

set_property(TARGET ${JFC_TEST_TARGET} PROPERTY CXX_EXTENSIONS OFF)

