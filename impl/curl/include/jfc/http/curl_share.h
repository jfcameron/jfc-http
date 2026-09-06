// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CURL_SHARE_H
#define JFC_HTTP_CURL_SHARE_H

#include <array>
#include <mutex>

#define CURL_STATICLIB
#include <curl/curl.h>

namespace jfc::http {
    class curl_share final {
    public:
        [[nodiscard]] CURLSH *handle() const { return m_pHandle; }

        curl_share();

        ~curl_share();

        curl_share(const curl_share &) = delete;
        curl_share &operator=(const curl_share &) = delete;
        curl_share(curl_share &&) = delete;
        curl_share &operator=(curl_share &&) = delete;

    private:
        /// \brief curl calls these around every access to a shared cache
        ///
        /// **Not optional.** A share touched by more than one thread and given no lock callbacks
        /// is a data race in libcurl's own structures, and this library performs its transfers on
        /// whatever threads the caller lends it.
        static void lock(CURL *, const curl_lock_data aData, curl_lock_access, void *apUser);

        static void unlock(CURL *, const curl_lock_data aData, void *apUser);

        std::array<std::mutex, CURL_LOCK_DATA_LAST> m_Mutexes;

        CURLSH *m_pHandle = nullptr;
    };
}

#endif
