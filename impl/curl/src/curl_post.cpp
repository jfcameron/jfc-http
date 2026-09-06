// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_post.h>

#include <utility>

using namespace jfc;

http::curl_post::curl_post(std::weak_ptr<http::curl_context> pContext,
    const std::string &aURL,
    const std::string &aPostData,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) 
: http::curl_request(pContext, aURL, aConfig, std::move(aHandler))
, m_PostData(aPostData)
{}

void http::curl_post::worker_on_enqueued_perform_extra_configuration(CURL *const apCURL) {
    curl_easy_setopt(apCURL, CURLOPT_POSTFIELDSIZE, static_cast<long>(m_PostData.size()));
    curl_easy_setopt(apCURL, CURLOPT_COPYPOSTFIELDS, m_PostData.data());
}

bool http::curl_post::try_update_postdata(const std::string &aPostData) {
    if (is_in_flight()) return false;

    m_PostData = aPostData;
    return true;
}

const std::string &http::curl_post::postdata() const {
    return m_PostData;
}

bool http::curl_post::try_submit() {
    return curl_request::try_submit();
}

void http::curl_post::cancel() {
    curl_request::cancel();
}

