# © Joseph Cameron - All Rights Reserved

if (EMSCRIPTEN)
    return()
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows" OR ANDROID)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_STATIC_LIBS ON)
    set(BUILD_CURL_EXE OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_LIBCURL_DOCS OFF)
    set(BUILD_MISC_DOCS OFF)
    set(ENABLE_CURL_MANUAL OFF)
    set(CURL_DISABLE_INSTALL ON)
    set(CURL_USE_LIBPSL OFF)
    set(CURL_USE_LIBSSH2 OFF)
    set(USE_LIBIDN2 OFF)
    set(USE_NGHTTP2 OFF)
    set(CURL_BROTLI OFF)
    set(CURL_ZSTD OFF)

    if (WIN32)
        set(CURL_USE_SCHANNEL ON)
    else()
        set(CURL_USE_MBEDTLS ON)

        set(CURL_CA_PATH "/system/etc/security/cacerts")
        set(CURL_CA_BUNDLE "none")
    endif()

    add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/curl")

    set (JFC_CURL_TARGET "libcurl" PARENT_SCOPE)

    if (WIN32)
        set (JFC_CURL_SYSTEM_LIBRARIES
            ws2_32.lib wldap32.lib advapi32.lib kernel32.lib comdlg32.lib crypt32.lib normaliz.lib
            PARENT_SCOPE)
    endif()
else()
    FIND_PACKAGE(CURL REQUIRED)

    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_GLOBAL TRUE)

    set (JFC_CURL_TARGET "CURL::libcurl" PARENT_SCOPE)
endif()
