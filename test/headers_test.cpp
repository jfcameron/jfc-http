// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <jfc/http/curl_request.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class collecting_handler final : public http::response_handler
    {
    public:
        std::vector<std::pair<std::string, std::string>> headers;

        std::vector<http::response_status_type> statuses;

        std::string body;

        void worker_on_status(const http::response_status_type aStatus) override
        {
            statuses.push_back(aStatus);

            headers.clear();
        }

        void worker_on_header(const http::header_view_type aName,
            const http::header_view_type aValue) override
        {
            headers.emplace_back(std::string(aName), std::string(aValue));
        }

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            body.append(aChunk.data(), aChunk.size());

            return true;
        }

        void main_on_success() override {}
        void main_on_failure(const http::error) override {}

        [[nodiscard]] std::optional<std::string> find(const std::string &aName) const
        {
            for (const auto &[name, value] : headers)
                if (http::header_name_equals(name, aName)) return value;

            return std::nullopt;
        }
    };

    [[nodiscard]] http::request_config server_config()
    {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 5000};
    }

    template <typename Server>
    [[nodiscard]] collecting_handler run(Server &aServer, const http::request_config &aConfig)
    {
        const auto pContext = curl_context();

        auto pHandler = std::make_unique<collecting_handler>();

        auto &handler = *pHandler;

        const auto pGet = pContext->make_get(aServer.url(), aConfig, std::move(pHandler));

        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());

        return handler;
    }
}

TEST_CASE("header names compare case-insensitively", "[jfc::http::types]") {
    REQUIRE(http::header_name_equals("ETag", "etag"));
    REQUIRE(http::header_name_equals("CONTENT-LENGTH", "content-length"));
    REQUIRE(http::header_name_equals("", ""));

    REQUIRE_FALSE(http::header_name_equals("ETag", "ETagg"));
    REQUIRE_FALSE(http::header_name_equals("ETag", ""));
    REQUIRE_FALSE(http::header_name_equals("Retry-After", "Retry_After"));
}

TEST_CASE("header lines are parsed into names and values", "[jfc::http::curl_request]") {
    collecting_handler handler;

    http::response_sink sink{.pHandler = &handler};

    const auto feed = [&](std::string aLine)
    {
        return http::receive_response_header(aLine.data(), 1, aLine.size(), &sink);
    };

    SECTION("**a status line is a status, not a header**")
    {
        REQUIRE(feed("HTTP/1.1 404 Not Found\r\n") > 0);

        REQUIRE(handler.statuses == std::vector<http::response_status_type>{404});
        REQUIRE(handler.headers.empty());
        REQUIRE(sink.status == 404);
    }

    SECTION("**a header is split on the first colon, and trimmed**")
    {
        REQUIRE(feed("Content-Type:  text/plain \r\n") > 0);

        REQUIRE(handler.headers.size() == 1);
        REQUIRE(handler.headers[0].first == "Content-Type");
        REQUIRE(handler.headers[0].second == "text/plain");
    }

    SECTION("**a value containing a colon keeps it**")
    {
        REQUIRE(feed("Location: https://example.com:8443/thing\r\n") > 0);

        REQUIRE(handler.headers[0].second == "https://example.com:8443/thing");
    }

    SECTION("**the blank line ending the headers is not a header**")
    {
        REQUIRE(feed("\r\n") > 0);

        REQUIRE(handler.headers.empty());
    }

    SECTION("**a status line the server made up is ignored, not guessed at**")
    {
        const auto hostile = GENERATE(
            "HTTP/1.1 99999999999999999999 Very Large\r\n", // past every integer we have
            "HTTP/1.1 -200 Negative\r\n",                   // a sign is not part of a status
            "HTTP/1.1 abc Not A Number\r\n",
            "HTTP/1.1 \r\n",                                // nothing at all where a code goes
            "HTTP/1.1 12 Too Short\r\n",                    // a status is three digits
            "HTTP/1.1 1000 Too Long\r\n",
            "HTTP/1.1\r\n");                                // no space, so no field to read

        REQUIRE(feed(hostile) > 0);

        REQUIRE(handler.statuses.empty());
        REQUIRE(sink.status == 0);
        REQUIRE_FALSE(sink.status_reported);
    }

    SECTION("**a three-digit status is read, including one nobody registered**")
    {
        const auto accepted = GENERATE(100, 200, 206, 404, 503, 599, 999);

        std::string line = "HTTP/1.1 " + std::to_string(accepted) + " Something\r\n";

        REQUIRE(feed(line) > 0);

        REQUIRE(handler.statuses == std::vector<http::response_status_type>{accepted});
    }

    SECTION("**and the callback reports the whole line as consumed**")
    {
        std::string line("X-Thing: value\r\n");

        REQUIRE(feed(line) == line.size());
    }
}

TEST_CASE("a response's headers reach the handler", "[jfc::http::response_handler]") {
    test_server server({{"200 OK", {"ETag: \"abc123\"", "X-Custom: hello"}, "body"}});

    REQUIRE(server.listening());

    const auto handler = run(server, server_config());

    REQUIRE(handler.find("etag").has_value());
    REQUIRE(*handler.find("etag") == "\"abc123\"");

    REQUIRE(*handler.find("X-CUSTOM") == "hello");

    REQUIRE(handler.body == "body");
}

TEST_CASE("a redirect is followed to the resource", "[jfc::http::request_config]") {
    SECTION("**the final body is delivered, and the hop's is not**")
    {
        test_server server({
            {"301 Moved Permanently", {"Location: /final", "ETag: \"hop\""}, "REDIRECT-PAGE"},
            {"200 OK", {"ETag: \"real\""}, "THE-REAL-CONTENT"},
        });

        REQUIRE(server.listening());

        const auto handler = run(server, server_config());

        REQUIRE(handler.body == "THE-REAL-CONTENT");

        REQUIRE(handler.statuses == std::vector<http::response_status_type>{301, 200});

        REQUIRE(server.served() == 2);
    }

    SECTION("**and the hop's headers are not mistaken for the resource's**")
    {
        test_server server({
            {"301 Moved Permanently", {"Location: /final", "ETag: \"hop\""}, ""},
            {"200 OK", {"ETag: \"real\""}, "content"},
        });

        REQUIRE(server.listening());

        const auto handler = run(server, server_config());

        REQUIRE(*handler.find("ETag") == "\"real\"");
    }

    SECTION("**not following delivers the redirect itself**")
    {
        test_server server({{"301 Moved Permanently", {"Location: /final"}, "REDIRECT-PAGE"}});

        REQUIRE(server.listening());

        auto config = server_config();

        config.follow_redirects = false;
        config.fail_on_http_error = false;

        const auto handler = run(server, config);

        REQUIRE(handler.statuses == std::vector<http::response_status_type>{301});
        REQUIRE(handler.body == "REDIRECT-PAGE");
        REQUIRE(server.served() == 1);
    }
}

TEST_CASE("a redirect chain is bounded", "[jfc::http::request_config]") {
    test_server server({
        {"301 Moved Permanently", {"Location: /a"}, ""},
        {"301 Moved Permanently", {"Location: /b"}, ""},
        {"301 Moved Permanently", {"Location: /c"}, ""},
        {"301 Moved Permanently", {"Location: /d"}, ""},
    });

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    http::error reported = http::error::none;

    auto config = server_config();

    config.maximum_redirects = 2;

    const auto pGet = pContext->make_get(server.url(), config,
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::too_many_redirects);
}

TEST_CASE("a resumed request carries the validator it was given", "[jfc::http::request_config]") {
    SECTION("**the first attempt learns the ETag**")
    {
        test_server server({{"200 OK", {"ETag: \"v1\""}, "0123456789"}});

        REQUIRE(server.listening());

        const auto handler = run(server, server_config());

        REQUIRE(*handler.find("ETag") == "\"v1\"");
    }

    SECTION("**and the second sends it as If-Range beside the range**")
    {
        test_server server({{"206 Partial Content",
            {"ETag: \"v1\"", "Content-Range: bytes 6-9/10"}, "6789"}});

        REQUIRE(server.listening());

        auto config = server_config();

        config.resume_from_byte = 6;
        config.if_range = "\"v1\"";

        const auto handler = run(server, config);

        REQUIRE(handler.body == "6789");

        REQUIRE(server.last_request().find("If-Range: \"v1\"") != std::string::npos);
        REQUIRE(server.last_request().find("Range: bytes=6-") != std::string::npos);
    }

    SECTION("**a server whose file changed answers 200, and that is refused**")
    {
        test_server server({{"200 OK", {"ETag: \"v2\""}, "a completely different build"}});

        REQUIRE(server.listening());

        const auto pContext = curl_context();

        http::error reported = http::error::none;

        auto config = server_config();

        config.resume_from_byte = 6;
        config.if_range = "\"v1\"";

        const auto pGet = pContext->make_get(server.url(), config,
            [](http::response_data_type) {},
            [&](http::error aError) { reported = aError; });

        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());

        REQUIRE(reported == http::error::range_not_satisfied);
    }

    SECTION("**a 206 that does not say which range it sent is refused**")
    {
        test_server server({{"206 Partial Content", {"ETag: \"v1\""}, "6789"}});

        REQUIRE(server.listening());

        const auto pContext = curl_context();

        http::error reported = http::error::none;

        auto config = server_config();

        config.resume_from_byte = 6;
        config.if_range = "\"v1\"";

        const auto pGet = pContext->make_get(server.url(), config,
            [](http::response_data_type) {},
            [&](http::error aError) { reported = aError; });

        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());

        REQUIRE(reported == http::error::range_not_satisfied);
    }

    SECTION("**an If-Range without a range is not sent, having nothing to condition**")
    {
        test_server server({{"200 OK", {}, "whole"}});

        REQUIRE(server.listening());

        auto config = server_config();

        config.if_range = "\"v1\"";

        const auto handler = run(server, config);

        REQUIRE(handler.body == "whole");
        REQUIRE(server.last_request().find("If-Range") == std::string::npos);
    }
}
