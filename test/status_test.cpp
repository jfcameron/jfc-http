// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <jfc/http/curl_request.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class status_handler final : public http::response_handler
    {
    public:
        std::optional<http::response_status_type> status;

        std::vector<std::string> calls;

        std::string body;

        void worker_on_status(const http::response_status_type aStatus) override
        {
            status = aStatus;

            calls.push_back("status");
        }

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            body.append(aChunk.data(), aChunk.size());

            calls.push_back("data");

            return true;
        }

        void worker_on_complete() override { calls.push_back("complete"); }

        void main_on_success() override { calls.push_back("main_success"); }

        void main_on_failure(const http::error) override { calls.push_back("main_failure"); }
    };

    [[nodiscard]] status_handler run_against(const std::string &aStatusLine,
        const std::string &aBody,
        const http::request_config &aConfig)
    {
        test_server server(aStatusLine, aBody);

        REQUIRE(server.listening());

        const auto pContext = curl_context();

        auto pHandler = std::make_unique<status_handler>();

        auto &handler = *pHandler;

        const auto pGet = pContext->make_get(server.url(), aConfig, std::move(pHandler));

        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());

        return handler;
    }

    [[nodiscard]] http::request_config server_config()
    {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 5000};
    }
}

TEST_CASE("the handler is told the status the server sent", "[jfc::http::response_handler]") {
    SECTION("**a 200 is reported, before any of the body**")
    {
        const auto handler = run_against("200 OK", "hello world", server_config());

        REQUIRE(handler.status.has_value());
        REQUIRE(*handler.status == 200);
        REQUIRE(handler.body == "hello world");

        REQUIRE(handler.calls.front() == "status");
        REQUIRE(handler.calls[1] == "data");
    }

    SECTION("**it is reported exactly once, however many chunks arrive**")
    {
        const auto handler = run_against("200 OK", std::string(64u * 1024u, 'x'), server_config());

        REQUIRE(*handler.status == 200);

        REQUIRE(std::count(handler.calls.begin(), handler.calls.end(), "status") == 1);
    }

    SECTION("**a response with no body still reports its status**")
    {
        const auto handler = run_against("204 No Content", "", server_config());

        REQUIRE(handler.status.has_value());
        REQUIRE(*handler.status == 204);
        REQUIRE(handler.body.empty());
    }
}

TEST_CASE("a failing status reaches the handler alongside the failure",
    "[jfc::http::response_handler]") {
    SECTION("**a 404 with fail_on_http_error on fails, and says which status**")
    {
        const auto handler = run_against("404 Not Found", "no such thing", server_config());

        REQUIRE(handler.status.has_value());
        REQUIRE(*handler.status == 404);

        REQUIRE(handler.calls.back() == "main_failure");

        REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "complete")
            == handler.calls.end());
    }

    SECTION("**a 503 is distinguishable from a 404, which is the whole point**")
    {
        const auto handler = run_against("503 Service Unavailable", "later", server_config());

        REQUIRE(*handler.status == 503);
    }

    SECTION("**with fail_on_http_error off, the same 404 succeeds and delivers its body**")
    {
        auto config = server_config();

        config.fail_on_http_error = false;

        const auto handler = run_against("404 Not Found", "no such thing", config);

        REQUIRE(*handler.status == 404);
        REQUIRE(handler.body == "no such thing");
        REQUIRE(handler.calls.back() == "main_success");
    }
}

TEST_CASE("a request that never reached a server reports no status",
    "[jfc::http::response_handler]") {
    const auto pContext = curl_context();

    auto pHandler = std::make_unique<status_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(REFUSED_URL, config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE_FALSE(handler.status.has_value());

    REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "status")
        == handler.calls.end());
}

TEST_CASE("a server that ignores a range is caught", "[jfc::http::request_config]") {
    test_server server("200 OK", "the whole thing, not the part that was asked for");

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    http::error reported = http::error::none;

    auto config = server_config();

    config.resume_from_byte = 8;

    const auto pGet = pContext->make_get(server.url(), config,
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::range_not_satisfied);
}

TEST_CASE("the buffering handler exposes the status it saw",
    "[jfc::http::buffering_response_handler]") {
    test_server server("404 Not Found", "gone");

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    http::error reported = http::error::none;

    auto pHandler = std::make_unique<http::buffering_response_handler>(
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(server.url(), server_config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::http_error);

    REQUIRE(handler.status().has_value());
    REQUIRE(*handler.status() == 404);
}

TEST_CASE("reporting a status without a handle does nothing", "[jfc::http::curl_request]") {
    status_handler handler;

    http::response_sink sink{.pHandler = &handler};

    http::report_response_status(sink);

    REQUIRE_FALSE(handler.status.has_value());
    REQUIRE_FALSE(sink.status_reported);
}
