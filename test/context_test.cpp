// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/null_context.h>

#include <memory>
#include <string>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    void no_op_success(http::response_data_type) {}
    void no_op_failure(http::error) {}
}

TEST_CASE("the factory returns the implementation it was asked for", "[jfc::http::context]") {
    SECTION("**both implementations are constructible**") {
        REQUIRE(null_context());
        REQUIRE(curl_context());
    }

    SECTION("**and they are actually distinct**") {
        deferring_submitter nullSubmitter;
        deferring_submitter curlSubmitter;

        const auto pNull = http::null_context::make({.submit = nullSubmitter.submitter()});

        const auto pCurl = curl_context(curlSubmitter.submitter());

        const auto pNullGet = pNull->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);
        const auto pCurlGet = pCurl->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);

        REQUIRE_FALSE(pNullGet->try_submit());
        REQUIRE(pCurlGet->try_submit());

        REQUIRE(nullSubmitter.pending() == 0);
        REQUIRE(curlSubmitter.pending() == 1);

        curlSubmitter.run_all();
    }
}

TEST_CASE("a context hands out requests without sending them", "[jfc::http::context]") {
    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    SECTION("**a fresh context owes nothing**") {
        REQUIRE(pContext->outstanding_request_count() == 0);
    }

    SECTION("**and has nothing to hand back to main**") {
        REQUIRE_FALSE(pContext->main_try_handle_completed_request());
    }

    SECTION("**making a get does not submit it**") {
        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);

        REQUIRE(pGet);
        REQUIRE(submitter.pending() == 0);
        REQUIRE(pContext->outstanding_request_count() == 0);
    }

    SECTION("**and making a post does not submit it either**") {
        const auto pPost = pContext->make_post(REFUSED_URL, "a=1", config(),
            no_op_success, no_op_failure);

        REQUIRE(pPost);
        REQUIRE(submitter.pending() == 0);
        REQUIRE(pContext->outstanding_request_count() == 0);
    }
}

TEST_CASE("a request refuses to be submitted twice", "[jfc::http::request]") {
    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    const auto pGet = pContext->make_get(REFUSED_URL, config(), no_op_success, no_op_failure);

    REQUIRE(pGet->try_submit());
    REQUIRE_FALSE(pGet->try_submit());

    REQUIRE(submitter.pending() == 1);
    REQUIRE(pContext->outstanding_request_count() == 1);

    submitter.run_all();
}

TEST_CASE("a request outliving its context declines to submit", "[jfc::http::request]") {
    http::request_shared_ptr_type pGet;

    {
        const auto pContext = curl_context();

        pGet = pContext->make_get(REFUSED_URL, config(), no_op_success, no_op_failure);
    }

    REQUIRE(pGet);
    REQUIRE_FALSE(pGet->try_submit());
}

TEST_CASE("a request config is usable with no fields set", "[jfc::http::request_config]") {
    const http::request_config defaults;

    REQUIRE(defaults.user_agent.empty());
    REQUIRE_FALSE(http::user_agent_header(defaults).empty());

    REQUIRE(defaults.timeout_milliseconds > 0);
    REQUIRE(defaults.headers.empty());

    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    REQUIRE(pContext->make_get(REFUSED_URL, {}, no_op_success, no_op_failure));
}

TEST_CASE("a post's body can be replaced while it is idle", "[jfc::http::post]") {
    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    const auto pPost = pContext->make_post(REFUSED_URL, "a=1", config(),
        no_op_success, no_op_failure);

    SECTION("**an idle post accepts a new body**") {
        REQUIRE(pPost->try_update_postdata("a=2"));
    }

    SECTION("**one in flight refuses, rather than changing under the worker**") {
        REQUIRE(pPost->try_submit());
        REQUIRE_FALSE(pPost->try_update_postdata("a=2"));

        submitter.run_all();
    }
}
