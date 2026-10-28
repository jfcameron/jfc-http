# © Joseph Cameron - MIT License

cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

if (TARGET jfc::http)
    return()
endif()

set(JFCHTTP_VERSION 0.75)

project(jfchttp
    VERSION ${JFCHTTP_VERSION}
    DESCRIPTION "concurrent, object oriented http lib")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/jfc-cmake/jfclib.cmake")

option(JFCHTTP_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(JFCHTTP_BUILD_DEMO "Build the demo" OFF)
option(JFCHTTP_BUILD_DOCS "Build documentation" OFF)

if (WIN32 OR ANDROID)
    set(_jfc_http_vendor_curl_by_default ON)
else()
    set(_jfc_http_vendor_curl_by_default OFF)
endif()

option(JFCHTTP_VENDORED_CURL "Build extern/curl rather than finding the system's" ${_jfc_http_vendor_curl_by_default})

include(GNUInstallDirs)

find_package(Threads REQUIRED)

add_subdirectory(extern)

if (NOT EMSCRIPTEN AND NOT TARGET CURL::libcurl)
    find_package(CURL REQUIRED)
endif()

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

if (EMSCRIPTEN)
    set(_jfc_http_implementation_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/fetch/src/fetch_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/fetch/src/fetch_request.cpp)

    set(_jfc_http_implementation_include "${CMAKE_CURRENT_SOURCE_DIR}/impl/fetch/include")
    set(_jfc_http_implementation_private_include "${CMAKE_CURRENT_SOURCE_DIR}/impl/fetch/src/include")
else()
    set(_jfc_http_implementation_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_get.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_post.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_request.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/curl_share.cpp)

    set(_jfc_http_implementation_include "${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/include")
    set(_jfc_http_implementation_private_include "${CMAKE_CURRENT_SOURCE_DIR}/impl/curl/src/include")
endif()

add_library(jfchttp STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffering_response_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/exception.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/policy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/request.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/types.cpp
    ${_jfc_http_implementation_sources}
    ${CMAKE_CURRENT_SOURCE_DIR}/impl/null/src/null_context.cpp)

add_library(jfc::http ALIAS jfchttp)

set_target_properties(jfchttp PROPERTIES EXPORT_NAME http)

set_target_properties(jfchttp PROPERTIES PREFIX "lib")

target_include_directories(jfchttp
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${_jfc_http_implementation_include}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/impl/null/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        ${_jfc_http_implementation_private_include}
    )

target_compile_features(jfchttp PUBLIC cxx_std_20)

target_link_libraries(jfchttp PUBLIC Threads::Threads)

if (EMSCRIPTEN)
    target_link_options(jfchttp PUBLIC "-sFETCH")
else()
    target_link_libraries(jfchttp PUBLIC CURL::libcurl)
endif()

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

include(CMakePackageConfigHelpers)

install(TARGETS jfchttp
    EXPORT jfc-http-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        "${_jfc_http_implementation_include}/"
        "${CMAKE_CURRENT_SOURCE_DIR}/impl/null/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(EXPORT jfc-http-targets
    FILE jfc-http-targets.cmake
    NAMESPACE jfc::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/jfc-http")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jfc-http-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

if (EMSCRIPTEN)
    set(_jfc_http_dependencies "find_dependency(Threads)\n")
else()
    set(_jfc_http_dependencies "find_dependency(Threads)\nfind_dependency(CURL)\n")
endif()

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/jfc-http-config.cmake"
"include(CMakeFindDependencyMacro)
${_jfc_http_dependencies}include(\"\${CMAKE_CURRENT_LIST_DIR}/jfc-http-targets.cmake\")
")

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/jfc-http-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/jfc-http-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/jfc-http")

if (JFCHTTP_BUILD_DEMO)
    add_subdirectory(demo)
endif()

if (JFCHTTP_BUILD_TESTS)
    add_subdirectory(test)
endif()

if (JFC_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if (JFCHTTP_BUILD_DOCS)
    add_subdirectory(docs)
endif()
