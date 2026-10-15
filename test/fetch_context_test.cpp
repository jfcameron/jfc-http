// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/fetch_context.h>

#include <memory>
#include <type_traits>

using namespace jfc;
using namespace jfc::http::test;

TEST_CASE("the fetch implementation builds and hands out requests", "[jfc::http::fetch_context]") {
    const auto pContext = http::fetch_context::make();

    REQUIRE(pContext);
    REQUIRE(pContext->outstanding_request_count() == 0);

    static_assert(!std::is_default_constructible_v<http::fetch_context>,
        "the constructor is private: a context comes from fetch_context::make");

    SECTION("**a get, and a post whose body may be changed before it is sent**") {
        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            [](http::response_data_type) {}, [](http::error) {});

        REQUIRE(pGet);

        const auto pPost = pContext->make_post(REFUSED_URL, "a=1", config(),
            [](http::response_data_type) {}, [](http::error) {});

        REQUIRE(pPost);
        REQUIRE(pPost->try_update_postdata("a=2"));
    }

    SECTION("**a request header is checked as curl's are**") {
        auto bad = config();

        bad.headers.push_back("X-Injected: a\r\nX-Second: b");

        REQUIRE_THROWS_AS(pContext->make_get(REFUSED_URL, bad,
            [](http::response_data_type) {}, [](http::error) {}), http::exception);

        bad.headers = {"not a header line"};

        REQUIRE_THROWS_AS(pContext->make_get(REFUSED_URL, bad,
            [](http::response_data_type) {}, [](http::error) {}), http::exception);
    }

    SECTION("**nothing is owed to main until something is submitted**") {
        REQUIRE_FALSE(pContext->main_try_handle_completed_request());

        pContext->cancel_all();

        REQUIRE(pContext->outstanding_request_count() == 0);
    }

    SECTION("**a request whose context is gone declines to send**") {
        http::request_shared_ptr_type pOrphan;

        {
            const auto pShortLived = http::fetch_context::make();

            pOrphan = pShortLived->make_get(REFUSED_URL, config(),
                [](http::response_data_type) {}, [](http::error) {});
        }

        REQUIRE_FALSE(pOrphan->try_submit());
    }
}
