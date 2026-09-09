# © Joseph Cameron - All Rights Reserved

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    add_subdirectory ("${CMAKE_CURRENT_SOURCE_DIR}/curl")

    set (JFC_CURL_TARGET "libcurl" PARENT_SCOPE)

    set (JFC_CURL_SYSTEM_LIBRARIES
        ws2_32.lib wldap32.lib advapi32.lib kernel32.lib comdlg32.lib crypt32.lib normaliz.lib
        PARENT_SCOPE)
else()
    FIND_PACKAGE(CURL REQUIRED)

    set_target_properties(CURL::libcurl PROPERTIES IMPORTED_GLOBAL TRUE)

    set (JFC_CURL_TARGET "CURL::libcurl" PARENT_SCOPE)
endif()
