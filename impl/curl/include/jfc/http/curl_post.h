// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CURL_POST_H
#define JFC_HTTP_CURL_POST_H

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_request.h>
#include <jfc/http/post.h>
#include <jfc/http/types.h>

#include <memory>
#include <string>

namespace jfc::http
{
    /// \brief libcurl implementation of a http POST
    class curl_post final : public http::post, public http::curl_request
    {
    public:
    /// \name external interface
    ///@{
    //
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        [[nodiscard]] bool try_update_postdata(const std::string &aPostData) override;
    ///@}

    /// \name internal interface
    ///@{
    //
        /// \brief the body this post will send
        [[nodiscard]] const std::string &postdata() const;
    ///@}

        curl_post(std::weak_ptr<http::curl_context> pContext,
            const std::string &aURL,
            const std::string &aPostData,
            const http::request_config &aConfig,
            http::response_handler_ptr_type &&aHandler);

        ~curl_post() override = default;

    private:
        void worker_on_enqueued_perform_extra_configuration(CURL *const apCURL) override;

        std::string m_PostData;
    };
}

#endif
