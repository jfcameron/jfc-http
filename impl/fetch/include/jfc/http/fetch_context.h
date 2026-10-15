// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_FETCH_CONTEXT_H
#define JFC_HTTP_FETCH_CONTEXT_H

#include <jfc/http/context.h>
#include <jfc/http/policy.h>
#include <jfc/http/types.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace jfc::http
{
    class fetch_request;

    /// \brief implementation using emscripten's Fetch API
    class fetch_context final : public http::context, public std::enable_shared_from_this<fetch_context>
    {
    public:
    /// \name external interface
    ///@{
    //
        /// \brief create a context backed by the browser's fetch
        /// \param aPolicy everything the caller owns rather than this library; see \ref policy
        [[nodiscard]] static context_shared_ptr_type make(http::policy aPolicy = {});

        using http::context::make_get;
        using http::context::make_post;

        [[nodiscard]] request_shared_ptr_type make_get(const std::string &aURL,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) override;

        [[nodiscard]] post_shared_ptr_type make_post(const std::string &aURL,
            const std::string &aPostData,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) override;

        bool main_try_handle_completed_request() override;

        void cancel_all() override;

        [[nodiscard]] std::size_t outstanding_request_count() const override;
    ///@}

    /// \name internal interface
    ///@{
    //
        /// \brief a request has started its fetch: main owes its handlers
        void submit(std::shared_ptr<http::fetch_request> aRequest);

        /// \brief hand a request's worker half to the caller's submitter
        void run_worker_half(http::task_type &&aTask);
    ///@}

        ~fetch_context() override = default;

    private:
        explicit fetch_context(http::policy aPolicy);

        const http::policy m_Policy;

        std::vector<std::shared_ptr<http::fetch_request>> m_UnhandledRequests;
    };
}

#endif
