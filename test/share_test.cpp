// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"
#include "test_server.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    [[nodiscard]] http::request_config server_config()
    {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 5000};
    }

    [[nodiscard]] std::string fetch(const http::context_shared_ptr_type &aContext,
        const std::string &aURL)
    {
        std::string body;

        const auto pGet = aContext->make_get(aURL, server_config(),
            [&](http::response_data_type aData) { body = to_string(aData); },
            [](http::error) {});

        REQUIRE(pGet->try_submit());
        REQUIRE(aContext->main_try_handle_completed_request());

        return body;
    }
}

TEST_CASE("requests from one context reuse a connection", "[jfc::http::curl_share]") {
    test_server server({{"200 OK", {}, "one"}, {"200 OK", {}, "two"}}, true);

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    REQUIRE(fetch(pContext, server.url()) == "one");
    REQUIRE(fetch(pContext, server.url()) == "two");

    REQUIRE(server.served() == 2);

    REQUIRE(server.connections() == 1);
}

TEST_CASE("a manifest and its chunks cost one connection", "[jfc::http::curl_share]") {
    constexpr std::size_t CHUNK_COUNT = 5;

    std::vector<test_server::response> responses{{"200 OK", {}, "manifest"}};

    for (std::size_t i = 0; i < CHUNK_COUNT; ++i)
        responses.push_back({"200 OK", {}, "chunk" + std::to_string(i)});

    test_server server(responses, true);

    REQUIRE(server.listening());

    const auto pContext = curl_context();

    REQUIRE(fetch(pContext, server.url()) == "manifest");

    for (std::size_t i = 0; i < CHUNK_COUNT; ++i)
        REQUIRE(fetch(pContext, server.url()) == "chunk" + std::to_string(i));

    REQUIRE(server.served() == CHUNK_COUNT + 1);

    REQUIRE(server.connections() == 1);
}

TEST_CASE("two contexts do not share a connection", "[jfc::http::curl_share]") {
    test_server server({{"200 OK", {}, "one"}, {"200 OK", {}, "two"}}, true);

    REQUIRE(server.listening());

    const auto pFirst = curl_context();
    const auto pSecond = curl_context();

    REQUIRE(fetch(pFirst, server.url()) == "one");
    REQUIRE(fetch(pSecond, server.url()) == "two");

    REQUIRE(server.served() == 2);

    REQUIRE(server.connections() == 2);
}

TEST_CASE("a request destroyed after its context cleans up safely", "[jfc::http::curl_share]") {
    test_server server({{"200 OK", {}, "body"}}, true);

    REQUIRE(server.listening());

    http::request_shared_ptr_type pOutlivingRequest;

    {
        const auto pContext = curl_context();

        REQUIRE(fetch(pContext, server.url()) == "body");

        pOutlivingRequest = pContext->make_get(server.url(), server_config(),
            [](http::response_data_type) {}, [](http::error) {});
    }

    REQUIRE(pOutlivingRequest);
    REQUIRE_FALSE(pOutlivingRequest->try_submit());

    pOutlivingRequest.reset();

    SUCCEED("the handle was cleaned up after its context, without reaching freed memory");
}
