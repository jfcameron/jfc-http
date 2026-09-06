// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CURL_GET_H
#define JFC_HTTP_CURL_GET_H

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_request.h>
#include <jfc/http/request.h>
#include <jfc/http/types.h>

#include <memory>
#include <string>

namespace jfc::http
{
    /// \brief libcurl implementation of a http GET
    class curl_get final : public http::request, public http::curl_request
    {
    public:
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        curl_get(std::weak_ptr<http::curl_context> pContext,
            const std::string &aURL,
            const http::request_config &aConfig,
            http::response_handler_ptr_type &&aHandler);

        ~curl_get() override = default;
    };
}

#endif
