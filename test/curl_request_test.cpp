// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_post.h>
#include <jfc/http/curl_request.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class discarding_handler final : public http::response_handler
    {
    public:
        bool worker_on_data(http::response_chunk_type) override { return true; }
        void main_on_success() override {}
        void main_on_failure(const http::error) override {}
    };
}

TEST_CASE("a refused connection travels the whole path and reports a failure",
    "[jfc::http::curl_request]") {
    const auto pContext = curl_context();

    http::error reported = http::error::none;

    bool succeeded = false;

    const auto pGet = pContext->make_get(REFUSED_URL, config(),
        [&](http::response_data_type) { succeeded = true; },
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->outstanding_request_count() == 1);

    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE_FALSE(succeeded);

    REQUIRE(reported == http::error::network_error);

    SECTION("**and the request is drained, not left for a second handling**")
    {
        REQUIRE(pContext->outstanding_request_count() == 0);
        REQUIRE_FALSE(pContext->main_try_handle_completed_request());
    }
}

TEST_CASE("a handled request can be sent again", "[jfc::http::curl_request]") {
    const auto pContext = curl_context();

    std::size_t failures = 0;

    const auto pGet = pContext->make_get(REFUSED_URL, config(),
        [](http::response_data_type) {},
        [&](http::error) { ++failures; });

    for (std::size_t i = 0; i < 2; ++i)
    {
        REQUIRE(pGet->try_submit());
        REQUIRE(pContext->main_try_handle_completed_request());
    }

    REQUIRE(failures == 2);
}

TEST_CASE("a post keeps the body it was made with", "[jfc::http::curl_post]") {
    const auto pContext = curl_context();

    const auto pPost = std::make_shared<http::curl_post>(
        std::weak_ptr<http::curl_context>(),
        REFUSED_URL,
        "field=value",
        config(),
        std::make_unique<discarding_handler>());

    REQUIRE(pPost->postdata() == "field=value");

    SECTION("**and an accepted update replaces it**")
    {
        REQUIRE(pPost->try_update_postdata("field=other"));
        REQUIRE(pPost->postdata() == "field=other");
    }

    SECTION("**a body containing a zero byte survives being stored**")
    {
        const std::string binary("a\0b", 3);

        REQUIRE(pPost->try_update_postdata(binary));
        REQUIRE(pPost->postdata().size() == 3);
    }
}

TEST_CASE("a post with a body travels the whole path", "[jfc::http::curl_post]") {
    const auto pContext = curl_context();

    http::error reported = http::error::none;

    const auto pPost = pContext->make_post(REFUSED_URL, "field=value", config(),
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pPost->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::network_error);
}
