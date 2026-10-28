# © Joseph Cameron - MIT License

if (EMSCRIPTEN OR NOT JFCHTTP_VENDORED_CURL)
    return()
endif()

if (NOT TARGET CURL::libcurl)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_STATIC_LIBS ON)
    set(BUILD_CURL_EXE OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_LIBCURL_DOCS OFF)
    set(BUILD_MISC_DOCS OFF)
    set(ENABLE_CURL_MANUAL OFF)
    set(CURL_USE_LIBPSL OFF)
    set(CURL_USE_LIBSSH2 OFF)
    set(USE_LIBIDN2 OFF)
    set(USE_NGHTTP2 OFF)
    set(CURL_BROTLI OFF CACHE STRING "")
    set(CURL_ZSTD OFF CACHE STRING "")

    if (WIN32)
        set(CURL_USE_SCHANNEL ON)
    elseif (ANDROID)
        set(ENABLE_PROGRAMS OFF CACHE BOOL "")
        set(ENABLE_TESTING OFF CACHE BOOL "")
        set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "")
        set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "")
        set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "")

        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mbedtls")

        set(MBEDTLS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include" CACHE PATH "")
        set(MBEDTLS_LIBRARY mbedtls CACHE STRING "")
        set(MBEDX509_LIBRARY mbedx509 CACHE STRING "")
        set(MBEDCRYPTO_LIBRARY mbedcrypto CACHE STRING "")

        set(CURL_USE_MBEDTLS ON)

        set(CURL_CA_PATH "/system/etc/security/cacerts" CACHE STRING "")
        set(CURL_CA_BUNDLE "none" CACHE STRING "")
    endif()

    add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/curl")
endif()
