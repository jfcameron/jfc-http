# © Joseph Cameron - All Rights Reserved

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(null)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/jfc-cmake/jfclib.cmake")

option(JFC_BUILD_DEMO "Build the demo" ON)
option(JFC_BUILD_DOCS "Build documentation" OFF)
option(JFC_BUILD_TESTS "Build unit tests" OFF)

set(JFCHTTP_VERSION 0.75)

add_subdirectory(extern)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
else()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-pedantic>)
    endif()

    include(CheckCXXCompilerFlag)

    foreach (_strict_flag "-Werror=c++20-extensions" "-Werror=c++23-extensions")
        string(MAKE_C_IDENTIFIER "HAS${_strict_flag}" _strict_flag_symbol)

        check_cxx_compiler_flag("${_strict_flag}" ${_strict_flag_symbol})

        if (${_strict_flag_symbol})
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_strict_flag}>)
        endif()
    endforeach()
endif()

jfc_project(library
    NAME "jfchttp"
    VERSION ${JFCHTTP_VERSION}
    DESCRIPTION "concurrent, object oriented http lib"
    C++_STANDARD 20
    C_STANDARD 90
    TYPE STATIC

    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/include
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/null/include

    PRIVATE_INCLUDE_DIRECTORIES
        ${CURL_INCLUDE_DIR}

    SOURCE_LIST
        ${CMAKE_CURRENT_SOURCE_DIR}/src/buffering_response_handler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/exception.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/policy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/request.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/types.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_get.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_post.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_request.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_share.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/null/src/null_context.cpp

    LIBRARIES
        ${CURL_LIBRARIES}

    DEPENDENCIES
        ${CURL_LIBRARY_NAME}
)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/include/jfc/http/request.h" _request_header)

string(REGEX MATCH "LIBRARY_PRODUCT_TOKEN = \"jfc-http_request/([0-9.]+)\""
    _token_match "${_request_header}")

if (NOT _token_match)
    message(FATAL_ERROR
        "could not find LIBRARY_PRODUCT_TOKEN in include/jfc/http/request.h")
endif()

if (NOT "${CMAKE_MATCH_1}" VERSION_EQUAL "${JFCHTTP_VERSION}")
    message(FATAL_ERROR
        "LIBRARY_PRODUCT_TOKEN says version ${CMAKE_MATCH_1} but JFCHTTP_VERSION is "
        "${JFCHTTP_VERSION}. Update include/jfc/http/request.h to match.")
endif()

set_property(TARGET jfchttp PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET jfchttp PROPERTY CXX_STANDARD_REQUIRED ON)

if (JFC_BUILD_DEMO)
    add_subdirectory(demo)
endif()

if (JFC_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (JFC_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if (JFC_BUILD_DOCS)
    add_subdirectory(docs)
endif()

