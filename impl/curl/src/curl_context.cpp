// © Joseph Cameron - MIT License

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_get.h>
#include <jfc/http/curl_post.h>
#include <jfc/http/curl_request.h>
#include <jfc/http/curl_share.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

using namespace jfc;

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
{}

http::curl_context::~curl_context() {
    cancel_all();

    auto &inFlight = *m_pInFlightTaskCount;

    for (auto count = inFlight.load(std::memory_order_acquire); count; count = inFlight.load(std::memory_order_acquire))
        inFlight.wait(count, std::memory_order_acquire);
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
    struct task_state final {
        std::shared_ptr<http::curl_request> request;
        std::shared_ptr<std::atomic<std::size_t>> in_flight;
        bool ran = false;

        task_state(std::shared_ptr<http::curl_request> aRequest, std::shared_ptr<std::atomic<std::size_t>> aInFlight)
        : request(std::move(aRequest))
        , in_flight(std::move(aInFlight))
        {
            in_flight->fetch_add(1, std::memory_order_release);
        }

        task_state(const task_state &) = delete;
        task_state &operator=(const task_state &) = delete;

        ~task_state() {
            if (!ran) request->worker_abandon();

            in_flight->fetch_sub(1, std::memory_order_release);
            in_flight->notify_all();
        }
    };

    const auto pState = std::make_shared<task_state>(aRequest, m_pInFlightTaskCount);

    m_UnhandledRequests.push_back(std::move(aRequest));

    std::vector<http::task_type> tasks;

    tasks.emplace_back([pState]() {
        pState->ran = true;

        pState->request->worker_fetch_task();
    });

    try {
        m_Policy.submit(std::move(tasks));
    }
    catch (...) {
        m_UnhandledRequests.pop_back();
        throw;
    }
}

