// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CURL_CONTEXT_H
#define JFC_HTTP_CURL_CONTEXT_H

#include <jfc/http/context.h>
#include <jfc/http/policy.h>
#include <jfc/http/types.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jfc::http
{
    class curl_request;
    class curl_share;

    /// \brief libcurl implementation of the library
    ///
    /// Including this header is how a caller chooses libcurl. \see jfc::http::context
    class curl_context final : public http::context, public std::enable_shared_from_this<curl_context>
    {
    public:
    /// \name external interface
    ///@{
    //
        /// \brief create a context backed by libcurl
        /// \param aPolicy everything the caller owns rather than this library; see \ref policy
        /// \exception jfc::http::exception the implementation could not be initialized
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
        /// \brief wrap a request as a task and hand it to the caller's submitter
        void submit(std::shared_ptr<http::curl_request> aRequest);

        /// \brief the caches every request from this context draws on
        [[nodiscard]] std::shared_ptr<http::curl_share> share() const;
    ///@}

        ~curl_context() override;

    private:
        explicit curl_context(http::policy aPolicy);

        const std::shared_ptr<http::curl_share> m_pShare;

        const http::policy m_Policy;

        std::atomic<std::size_t> m_InFlightTaskCount{0};

        std::vector<std::shared_ptr<http::curl_request>> m_UnhandledRequests;
    };
}

#endif
