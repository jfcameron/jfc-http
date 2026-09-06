// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <jfc/http/curl_request.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    enum class throw_from { 
        complete, 
        data, 
        main_failure,
        main_success, 
        none, 
        progress, 
        status
    };

    class throwing_handler final : public http::response_handler {
    public:
        throw_from when = throw_from::none;

        std::vector<std::string> calls;

        explicit throwing_handler(const throw_from aWhen) : when(aWhen) {}

        void worker_on_status(const http::response_status_type) override {
            calls.push_back("status");

            if (when == throw_from::status) throw std::runtime_error("threw from status");
        }

        bool worker_on_data(http::response_chunk_type) override {
            calls.push_back("data");

            if (when == throw_from::data) throw std::runtime_error("threw from data");

            return true;
        }

        void worker_on_progress(const std::size_t, const std::optional<std::size_t>) override {
            if (when == throw_from::progress) throw std::runtime_error("threw from progress");
        }

        void worker_on_complete() override {
            calls.push_back("complete");

            if (when == throw_from::complete) throw std::runtime_error("threw from complete");
        }

        void main_on_success() override {
            calls.push_back("main_success");

            if (when == throw_from::main_success) throw std::runtime_error("threw from success");
        }

        void main_on_failure(const http::error) override {
            calls.push_back("main_failure");

            if (when == throw_from::main_failure) throw std::runtime_error("threw from failure");
        }
    };

    [[nodiscard]] http::request_config server_config() {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 5000};
    }
}

TEST_CASE("an exception from a worker callback is delivered on main", "[jfc::http::exception]") {
    const auto when = GENERATE(throw_from::status, throw_from::data,
        throw_from::progress, throw_from::complete);

    test_server server("200 OK", "a body worth several callbacks");

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    const auto pGet = pContext->make_get(server.url(), server_config(),
        std::make_unique<throwing_handler>(when));

    REQUIRE_NOTHROW(static_cast<void>(pGet->try_submit()));

    REQUIRE_THROWS_AS(pContext->main_try_handle_completed_request(), std::runtime_error);

    REQUIRE(pContext->outstanding_request_count() == 0);

    REQUIRE_FALSE(pContext->main_try_handle_completed_request());
}

TEST_CASE("an exception never reaches the caller's thread pool", "[jfc::http::exception]") {
    test_server server("200 OK", "a body");

    REQUIRE(server.listening());

    std::atomic<bool> escapedIntoWorker{false};

    std::vector<std::thread> threads;

    {
        const auto pContext = curl_context(
            [&](std::vector<http::task_type> &&aTasks)
            {
                for (auto &task : aTasks)
                {
                    threads.emplace_back([t = std::move(task), &escapedIntoWorker]()
                    {
                        try { t(); }
                        catch (...) { escapedIntoWorker = true; }
                    });
                }
            });

        const auto pGet = pContext->make_get(server.url(), server_config(),
            std::make_unique<throwing_handler>(throw_from::data));

        REQUIRE(pGet->try_submit());

        for (auto &thread : threads) thread.join();

        REQUIRE_FALSE(escapedIntoWorker.load());

        REQUIRE(pContext->outstanding_request_count() == 1);

        REQUIRE_THROWS_AS(pContext->main_try_handle_completed_request(), std::runtime_error);

        REQUIRE(pContext->outstanding_request_count() == 0);
    }
}

TEST_CASE("the task body is noexcept", "[jfc::http::exception]") {
    STATIC_REQUIRE(noexcept(std::declval<http::curl_request &>().worker_fetch_task()));
}

TEST_CASE("an exception from a main callback leaves nothing stuck", "[jfc::http::exception]") {
    const auto when = GENERATE(throw_from::main_success, throw_from::main_failure);

    const bool expectSuccess = when == throw_from::main_success;

    test_server server(expectSuccess ? "200 OK" : "404 Not Found", "body");

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    const auto pGet = pContext->make_get(server.url(), server_config(),
        std::make_unique<throwing_handler>(when));

    REQUIRE(pGet->try_submit());

    REQUIRE_THROWS_AS(pContext->main_try_handle_completed_request(), std::runtime_error);

    REQUIRE(pContext->outstanding_request_count() == 0);
}

TEST_CASE("the first exception is the one delivered", "[jfc::http::exception]") {
    class two_throw_handler final : public http::response_handler
    {
    public:
        void worker_on_status(const http::response_status_type) override
        {
            throw std::runtime_error("first");
        }

        bool worker_on_data(http::response_chunk_type) override
        {
            throw std::runtime_error("second");
        }

        void main_on_success() override {}
        void main_on_failure(const http::error) override {}
    };

    test_server server("200 OK", "body");

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    const auto pGet = pContext->make_get(server.url(), server_config(),
        std::make_unique<two_throw_handler>());

    REQUIRE(pGet->try_submit());

    REQUIRE_THROWS_WITH(pContext->main_try_handle_completed_request(), "first");
}

TEST_CASE("a header carrying a line break is refused", "[jfc::http::request_config]") {
    const auto pContext = curl_context();

    const auto make_with_header = [&](const std::string &aHeader)
    {
        return pContext->make_get(REFUSED_URL,
            {.timeout_milliseconds = 1000, .headers = {aHeader}},
            [](http::response_data_type) {}, [](http::error) {});
    };

    SECTION("**a CRLF, which is how a second header is spliced in**")
    {
        REQUIRE_THROWS_AS(make_with_header("X-Session: a\r\nX-Injected: b"), http::exception);
    }

    SECTION("**a bare newline, which some servers accept just as readily**")
    {
        REQUIRE_THROWS_AS(make_with_header("X-Session: a\nX-Injected: b"), http::exception);
    }

    SECTION("**a null, which would truncate the header at the C boundary**")
    {
        REQUIRE_THROWS_AS(make_with_header(std::string("X-Session: a\0b", 14)), http::exception);
    }

    SECTION("**and an ordinary header is still accepted**")
    {
        REQUIRE_NOTHROW(make_with_header("X-Session: abc"));
    }
}
