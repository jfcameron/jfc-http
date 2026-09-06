// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/null_context.h>

#include <memory>
#include <type_traits>

using namespace jfc;
using namespace jfc::http::test;

TEST_CASE("the null implementation satisfies the whole contract", "[jfc::http::null_context]") {
    const auto pContext = null_context();

    REQUIRE(pContext);

    SECTION("**it hands out a get, and declines to send it**")
    {
        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            [](http::response_data_type) {}, [](http::error) {});

        REQUIRE(pGet);
        REQUIRE_FALSE(pGet->try_submit());
    }

    SECTION("**it hands out a post, and declines that too**")
    {
        const auto pPost = pContext->make_post(REFUSED_URL, "a=1", config(),
            [](http::response_data_type) {}, [](http::error) {});

        REQUIRE(pPost);
        REQUIRE_FALSE(pPost->try_submit());
        REQUIRE_FALSE(pPost->try_update_postdata("a=2"));
    }

    SECTION("**nothing is ever submitted, handled, or performed**")
    {
        REQUIRE(pContext->outstanding_request_count() == 0);
        REQUIRE_FALSE(pContext->main_try_handle_completed_request());
    }

    SECTION("**and no task ever reaches the caller's threads**")
    {
        deferring_submitter submitter;

        const auto pNull = http::null_context::make({.submit = submitter.submitter()});

        const auto pGet = pNull->make_get(REFUSED_URL, config(),
            [](http::response_data_type) {}, [](http::error) {});

        REQUIRE_FALSE(pGet->try_submit());
        REQUIRE(submitter.pending() == 0);
    }

    SECTION("**and the handler overload is accepted, not just the functor one**")
    {
        class counting_handler final : public http::response_handler
        {
        public:
            bool worker_on_data(http::response_chunk_type) override { return true; }
            void main_on_success() override {}
            void main_on_failure(const http::error) override {}
        };

        REQUIRE(pContext->make_get(REFUSED_URL, config(),
            std::make_unique<counting_handler>()));

        REQUIRE(pContext->make_post(REFUSED_URL, "a=1", config(),
            std::make_unique<counting_handler>()));
    }
}

TEST_CASE("the null context is reached through its own factory", "[jfc::http::null_context]") {
    const auto pContext = http::null_context::make();

    REQUIRE(pContext);
    REQUIRE(pContext->outstanding_request_count() == 0);

    static_assert(!std::is_default_constructible_v<http::null_context>,
        "the constructor is private: a context comes from null_context::make");

    http::null_get get;
    http::null_post post;

    REQUIRE_FALSE(get.try_submit());
    REQUIRE_FALSE(post.try_submit());
}
