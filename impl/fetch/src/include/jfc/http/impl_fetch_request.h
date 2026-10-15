// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_IMPL_FETCH_REQUEST_H
#define JFC_HTTP_IMPL_FETCH_REQUEST_H

#include <jfc/http/fetch_context.h>
#include <jfc/http/post.h>
#include <jfc/http/request.h>
#include <jfc/http/response_handler.h>
#include <jfc/http/types.h>

#include <emscripten/fetch.h>

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace jfc::http {
    /// \brief one transfer through emscripten's Fetch API
    class fetch_request : public std::enable_shared_from_this<fetch_request> {
    public:
        [[nodiscard]] bool try_submit();

        void cancel();

        [[nodiscard]] bool is_cancelled() const;

        [[nodiscard]] bool main_try_claim();

        void main_run_handlers();

        void main_abort_if_cancelled();

        virtual ~fetch_request();

    protected:
        fetch_request(std::weak_ptr<http::fetch_context> pContext, const std::string &aURL,
            const http::request_config &aConfig, http::response_handler_ptr_type &&aHandler);

        virtual void configure(emscripten_fetch_attr_t &aAttributes);

        [[nodiscard]] bool is_in_flight() const;

    private:
        static void on_success(emscripten_fetch_t *apFetch);

        static void on_error(emscripten_fetch_t *apFetch);

        static void on_progress(emscripten_fetch_t *apFetch);

        void worker_finish(emscripten_fetch_t *apFetch, const bool aSucceeded);

        void close_fetch();

        std::weak_ptr<http::fetch_context> m_pContext;

        std::string m_URL;

        http::request_config m_Config;

        std::vector<std::string> m_HeaderStrings;

        std::vector<const char *> m_HeaderPointers;

        http::response_handler_ptr_type m_ResponseHandler;

        emscripten_fetch_t *m_pFetch = nullptr;

        bool m_bInFlight = false;

        std::atomic<bool> m_bCancelled{false};

        std::atomic_flag m_bResponseLocked;

        http::error m_RequestError = http::error::none;

        std::exception_ptr m_HandlerException;

        http::response_status_type m_Status = 0;
    };

    class fetch_get final : public http::request, public http::fetch_request {
    public:
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        fetch_get(std::weak_ptr<http::fetch_context> pContext, const std::string &aURL,
            const http::request_config &aConfig, http::response_handler_ptr_type &&aHandler);

        ~fetch_get() override = default;
    };

    class fetch_post final : public http::post, public http::fetch_request {
    public:
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        [[nodiscard]] bool try_update_postdata(const std::string &aPostData) override;

        fetch_post(std::weak_ptr<http::fetch_context> pContext, const std::string &aURL,
            const std::string &aPostData, const http::request_config &aConfig,
            http::response_handler_ptr_type &&aHandler);

        ~fetch_post() override = default;

    private:
        void configure(emscripten_fetch_attr_t &aAttributes) override;

        std::string m_PostData;
    };
}

#endif
