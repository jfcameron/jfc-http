// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <jfc/http/curl_context.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    [[nodiscard]] std::string sent_user_agent(const std::string &aRequest)
    {
        auto lower = aRequest;

        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const auto start = lower.find("user-agent:");

        if (start == std::string::npos) return {};

        const auto valueStart = aRequest.find_first_not_of(' ', start + sizeof("user-agent:") - 1);
        const auto end = aRequest.find("\r\n", start);

        if (valueStart == std::string::npos || end == std::string::npos) return {};

        return aRequest.substr(valueStart, end - valueStart);
    }

    [[nodiscard]] std::string fetch_and_report_user_agent(const http::request_config &aConfig)
    {
        test_server server({{"200 OK", {}, "body"}});

        REQUIRE(server.listening());

        const auto pContext = curl_context();

        const auto pGet = pContext->make_get(server.url(), aConfig,
            [](const http::response_data_type &) {},
            [](const http::error) {});

        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());

        return sent_user_agent(server.last_request());
    }
}

TEST_CASE("the user agent stacks the caller's product token before the library's",
    "[jfc::http::request_config]") {

    SECTION("**an unset field sends the library alone**")
    {
        REQUIRE(http::user_agent_header({}) == http::LIBRARY_PRODUCT_TOKEN);
    }

    SECTION("**a set field is placed first, and the library's follows**")
    {
        const auto composed = http::user_agent_header({.user_agent = "MyGame/1.2.3"});

        REQUIRE(composed == std::string("MyGame/1.2.3 ") + http::LIBRARY_PRODUCT_TOKEN);

        REQUIRE(composed.find(' ') != std::string::npos);
    }

    SECTION("**the library's token carries a version**")
    {
        const std::string token = http::LIBRARY_PRODUCT_TOKEN;

        const auto slash = token.find('/');

        INFO("token \"" << token << "\"");

        REQUIRE(slash != std::string::npos);
        REQUIRE(slash + 1 < token.size());

        REQUIRE(std::isdigit(static_cast<unsigned char>(token[slash + 1])));
    }
}

TEST_CASE("the composed user agent is what reaches the server",
    "[jfc::http::curl_request]") {

    SECTION("**with an application token**")
    {
        const auto received = fetch_and_report_user_agent(
            {.user_agent = "MyGame/1.2.3", .timeout_milliseconds = 5000});

        REQUIRE(received == std::string("MyGame/1.2.3 ") + http::LIBRARY_PRODUCT_TOKEN);
    }

    SECTION("**and without one**")
    {
        const auto received = fetch_and_report_user_agent({.timeout_milliseconds = 5000});

        REQUIRE(received == http::LIBRARY_PRODUCT_TOKEN);
    }
}
