// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_get.h>

#include <utility>

using namespace jfc;

http::curl_get::curl_get(
    std::weak_ptr<http::curl_context> pContext,
    const std::string &aURL,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) : http::curl_request(pContext, aURL, aConfig, std::move(aHandler))
{}

bool http::curl_get::try_submit() {
    return curl_request::try_submit();
}

void http::curl_get::cancel() {
    curl_request::cancel();
}

