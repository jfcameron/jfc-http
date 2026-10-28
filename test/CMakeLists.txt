# © Joseph Cameron - MIT License

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
        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/src/include")
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

    DEPENDENCIES
        "jfchttp"
)

set_property(TARGET ${JFC_TEST_TARGET} PROPERTY CXX_EXTENSIONS OFF)


if (NOT CMAKE_CROSSCOMPILING)
    add_test(NAME jfchttp_install_and_consume
        COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIR=${PROJECT_BINARY_DIR}"
            "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install_and_consume"
            "-DCONFIG=$<CONFIG>"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
            "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}"
            "-DEXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
            -P "${CMAKE_CURRENT_LIST_DIR}/install/install_and_consume.cmake")
endif()
