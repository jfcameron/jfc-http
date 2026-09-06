// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    [[nodiscard]] http::request_config server_config() {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 10000};
    }

    class cancelling_handler final : public http::response_handler {
    public:
        http::request *pSelf = nullptr; 

        std::vector<std::string> calls;

        std::size_t received = 0;

        http::error outcome = http::error::none;

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            received += aChunk.size();

            calls.push_back("data");

            if (pSelf) pSelf->cancel();

            return true;
        }

        void worker_on_complete() override { calls.push_back("complete"); }

        void main_on_success() override { calls.push_back("success"); }

        void main_on_failure(const http::error aError) override
        {
            outcome = aError;

            calls.push_back("failure");
        }
    };
}

TEST_CASE("a request cancelled before it starts never runs", "[jfc::http::request]") {
    test_server server({{"200 OK", {}, "content"}});

    REQUIRE(server.listening());

    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    auto pHandler = std::make_unique<cancelling_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(server.url(), server_config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());

    pGet->cancel();

    REQUIRE(submitter.run_all() == 1);

    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(handler.outcome == http::error::cancelled);
    REQUIRE(handler.received == 0);

    REQUIRE(server.served() == 0);
}

TEST_CASE("a transfer stops at the callback after it is cancelled", "[jfc::http::request]") {
    test_server server({{"200 OK", {}, std::string(512u * 1024u, 'x')}});

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    auto pHandler = std::make_unique<cancelling_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(server.url(), server_config(), std::move(pHandler));

    handler.pSelf = pGet.get();

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(handler.outcome == http::error::cancelled);

    REQUIRE(handler.received > 0);
    REQUIRE(handler.received < 512u * 1024u);

    REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "complete")
        == handler.calls.end());

    REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "success")
        == handler.calls.end());
}

TEST_CASE("a stalled transfer can still be cancelled", "[jfc::http::request]") {
    test_server server({{"200 OK", {}, "the body that never comes", 10000}});

    REQUIRE(server.listening());

    std::vector<std::thread> threads;

    const auto pContext = curl_context(
        [&threads](std::vector<http::task_type> &&aTasks)
        {
            for (auto &task : aTasks) threads.emplace_back(std::move(task));
        });

    auto pHandler = std::make_unique<cancelling_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(server.url(), server_config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());

    const auto started = std::chrono::steady_clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    pGet->cancel();

    for (auto &thread : threads) thread.join();

    const auto elapsed = std::chrono::steady_clock::now() - started;

    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(handler.outcome == http::error::cancelled);

    REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 5);
}

TEST_CASE("cancelling is reported as its own outcome", "[jfc::http::request]") {
    test_server server({{"200 OK", {}, "content"}});

    REQUIRE(server.listening());

    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(server.url(), server_config(),
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());

    pGet->cancel();

    submitter.run_all();

    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::cancelled);
    REQUIRE(reported != http::error::network_error);
    REQUIRE(reported != http::error::aborted_by_handler);
}

TEST_CASE("a cancelled request can be sent again", "[jfc::http::request]") {
    test_server server({{"200 OK", {}, "first"}, {"200 OK", {}, "second"}});

    REQUIRE(server.listening());

    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    std::string body;

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(server.url(), server_config(),
        [&](http::response_data_type aData) { body = to_string(aData); },
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());

    pGet->cancel();

    submitter.run_all();

    REQUIRE(pContext->main_try_handle_completed_request());
    REQUIRE(reported == http::error::cancelled);

    REQUIRE(pGet->try_submit());

    submitter.run_all();

    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(body == "first");
}

TEST_CASE("cancel_all stops everything outstanding", "[jfc::http::context]") {
    test_server server({
        {"200 OK", {}, "a", 10000},
        {"200 OK", {}, "b", 10000},
        {"200 OK", {}, "c", 10000},
    });

    REQUIRE(server.listening());

    std::vector<std::thread> threads;

    std::atomic<std::size_t> cancellations{0};

    const auto started = std::chrono::steady_clock::now();

    {
        const auto pContext = curl_context(
            [&threads](std::vector<http::task_type> &&aTasks)
            {
                for (auto &task : aTasks) threads.emplace_back(std::move(task));
            });

        std::vector<http::request_shared_ptr_type> requests;

        for (std::size_t i = 0; i < 3; ++i)
        {
            requests.push_back(pContext->make_get(server.url(), server_config(),
                [](http::response_data_type) {},
                [&cancellations](http::error aError)
                {
                    if (aError == http::error::cancelled) ++cancellations;
                }));
        }

        for (const auto &pRequest : requests) REQUIRE(pRequest->try_submit());

        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        pContext->cancel_all();

        std::size_t handled = 0;

        while (handled < 3)
        {
            if (pContext->main_try_handle_completed_request()) ++handled;
            else std::this_thread::yield();
        }

    }

    for (auto &thread : threads) thread.join();

    const auto elapsed = std::chrono::steady_clock::now() - started;

    REQUIRE(cancellations.load() == 3);

    REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 6);
}

TEST_CASE("the null implementation accepts a cancel", "[jfc::http::null_context]") {
    const auto pContext = null_context();

    const auto pGet = pContext->make_get(REFUSED_URL, config(),
        [](http::response_data_type) {}, [](http::error) {});

    REQUIRE_NOTHROW(pGet->cancel());
    REQUIRE_NOTHROW(pContext->cancel_all());

    REQUIRE(pContext->outstanding_request_count() == 0);
}
