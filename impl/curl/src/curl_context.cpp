// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_get.h>
#include <jfc/http/curl_post.h>
#include <jfc/http/curl_request.h>
#include <jfc/http/curl_share.h>

#include <cstddef>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;

namespace {
    std::mutex g_GlobalInitMutex;
    std::size_t g_ContextInstanceCount = 0;
}

http::context_shared_ptr_type http::curl_context::make(http::policy aPolicy) {
    return http::context_shared_ptr_type(new http::curl_context(std::move(aPolicy)));
}

http::curl_context::curl_context(http::policy aPolicy)
: m_pShare(std::make_shared<http::curl_share>())
, m_Policy([&aPolicy]()
    {
        if (!aPolicy.submit) aPolicy.submit = http::make_serial_task_submitter();

        return std::move(aPolicy);
    }())
{
    const std::lock_guard<std::mutex> lock(g_GlobalInitMutex);

    if (g_ContextInstanceCount++ == 0) curl_global_init(CURL_GLOBAL_ALL);
}

http::curl_context::~curl_context() {
    while (m_InFlightTaskCount.load(std::memory_order_acquire) > 0) std::this_thread::yield();

    const std::lock_guard<std::mutex> lock(g_GlobalInitMutex);

    if (--g_ContextInstanceCount == 0) curl_global_cleanup();
}

void http::curl_context::cancel_all() {
    for (const auto &pRequest : m_UnhandledRequests) pRequest->cancel();
}

std::shared_ptr<http::curl_share> http::curl_context::share() const {
    return m_pShare;
}

std::size_t http::curl_context::outstanding_request_count() const {
    return m_UnhandledRequests.size();
}

http::request_shared_ptr_type http::curl_context::make_get(const std::string &aURL,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) {
    return std::static_pointer_cast<http::request>(
        std::make_shared<http::curl_get>(weak_from_this(), aURL, aConfig, std::move(aHandler)));
}

http::post_shared_ptr_type http::curl_context::make_post(const std::string &aURL,
    const std::string &aPostData,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) {
    return std::static_pointer_cast<http::post>(std::make_shared<http::curl_post>(
        weak_from_this(), aURL, aPostData, aConfig, std::move(aHandler)));
}

bool http::curl_context::main_try_handle_completed_request() {
    for (std::size_t i(0); i < m_UnhandledRequests.size(); ++i) {
        if (!m_UnhandledRequests[i]->main_try_claim()) continue;

        const auto pRequest = m_UnhandledRequests[i];
        m_UnhandledRequests.erase(m_UnhandledRequests.begin() + i);
        pRequest->main_run_handlers();
        return true;
    }

    return false;
}

void http::curl_context::submit(std::shared_ptr<http::curl_request> aRequest) {
    m_UnhandledRequests.push_back(aRequest);

    m_InFlightTaskCount.fetch_add(1, std::memory_order_release);

    const auto pGuard = std::shared_ptr<void>(nullptr, [this](void *) {
        m_InFlightTaskCount.fetch_sub(1, std::memory_order_release);
    });

    std::vector<http::task_type> tasks;

    tasks.emplace_back([pRequest = std::move(aRequest), pGuard]() {
        pRequest->worker_fetch_task();
    });

    try {
        m_Policy.submit(std::move(tasks));
    }
    catch (...) {
        m_UnhandledRequests.pop_back();
        throw;
    }
}

