// © Joseph Cameron - MIT License

#ifndef JFC_HTTP_CURL_SHARE_H
#define JFC_HTTP_CURL_SHARE_H

#include <array>
#include <mutex>

#include <curl/curl.h>

namespace jfc::http {
    /// \brief a hold on libcurl's global state, set up with the first and torn down with the last
    /// \exception jfc::http::exception libcurl could not be initialized
    class curl_global final {
    public:
        curl_global();

        ~curl_global();

        curl_global(const curl_global &) = delete;
        curl_global &operator=(const curl_global &) = delete;
    };

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

        const curl_global m_Global;

        std::array<std::mutex, CURL_LOCK_DATA_LAST> m_Mutexes;

        CURLSH *m_pHandle = nullptr;
    };
}

#endif
