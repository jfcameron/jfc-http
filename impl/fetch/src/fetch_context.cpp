// © Joseph Cameron - All Rights Reserved

#include <jfc/http/fetch_context.h>

#include <jfc/http/impl_fetch_request.h>

#include <utility>

using namespace jfc;

http::context_shared_ptr_type http::fetch_context::make(http::policy aPolicy) {
    return http::context_shared_ptr_type(new http::fetch_context(std::move(aPolicy)));
}

http::fetch_context::fetch_context(http::policy aPolicy)
: m_Policy([&aPolicy]()
    {
        if (!aPolicy.submit) aPolicy.submit = http::make_serial_task_submitter();

        return std::move(aPolicy);
    }())
{}

http::request_shared_ptr_type http::fetch_context::make_get(const std::string &aURL,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) {
    return std::static_pointer_cast<http::request>(
        std::make_shared<http::fetch_get>(weak_from_this(), aURL, aConfig, std::move(aHandler)));
}

http::post_shared_ptr_type http::fetch_context::make_post(const std::string &aURL,
    const std::string &aPostData,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) {
    return std::static_pointer_cast<http::post>(std::make_shared<http::fetch_post>(
        weak_from_this(), aURL, aPostData, aConfig, std::move(aHandler)));
}

bool http::fetch_context::main_try_handle_completed_request() {
    for (const auto &pRequest : m_UnhandledRequests) pRequest->main_abort_if_cancelled();

    for (std::size_t i(0); i < m_UnhandledRequests.size(); ++i) {
        if (!m_UnhandledRequests[i]->main_try_claim()) continue;

        const auto pRequest = m_UnhandledRequests[i];
        m_UnhandledRequests.erase(m_UnhandledRequests.begin() + i);
        pRequest->main_run_handlers();
        return true;
    }

    return false;
}

void http::fetch_context::cancel_all() {
    for (const auto &pRequest : m_UnhandledRequests) pRequest->cancel();
}

std::size_t http::fetch_context::outstanding_request_count() const {
    return m_UnhandledRequests.size();
}

void http::fetch_context::submit(std::shared_ptr<http::fetch_request> aRequest) {
    m_UnhandledRequests.push_back(std::move(aRequest));
}

void http::fetch_context::run_worker_half(http::task_type &&aTask) {
    std::vector<http::task_type> tasks;

    tasks.push_back(std::move(aTask));

    m_Policy.submit(std::move(tasks));
}
