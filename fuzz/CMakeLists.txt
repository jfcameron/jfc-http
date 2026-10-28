# © Joseph Cameron - MIT License

jfc_add_fuzz_tests(
    C++_STANDARD 20

    FUZZ_SOURCE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/header_fuzz.cpp"

    CORPUS_DIRECTORIES
        "${CMAKE_CURRENT_LIST_DIR}/corpus/headers"

    INCLUDE_DIRECTORIES

        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/include"

        "${CMAKE_CURRENT_LIST_DIR}/../impl/curl/src/include"

    LIBRARIES
        jfchttp

    DEPENDENCIES
        "jfchttp"
)

