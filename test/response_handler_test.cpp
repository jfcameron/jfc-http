// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class recording_handler final : public http::response_handler
    {
    public:
        std::vector<std::string> calls;

        http::error last_error = http::error::none;

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            calls.push_back("worker_on_data:" + std::to_string(aChunk.size()));

            return true;
        }

        void worker_on_complete() override { calls.push_back("worker_on_complete"); }

        void main_on_success() override { calls.push_back("main_on_success"); }

        void main_on_failure(const http::error aError) override
        {
            last_error = aError;

            calls.push_back("main_on_failure");
        }
    };
}

TEST_CASE("a failed request runs only the failure handler", "[jfc::http::response_handler]") {
    const auto pContext = curl_context();

    auto pHandler = std::make_unique<recording_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(REFUSED_URL, config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(handler.calls == std::vector<std::string>{"main_on_failure"});

    REQUIRE(handler.last_error == http::error::network_error);
}

TEST_CASE("the functor overloads and the handler interface agree",
    "[jfc::http::response_handler]") {
    const auto pContext = curl_context();

    http::error viaFunctor = http::error::none;

    const auto pFunctorGet = pContext->make_get(REFUSED_URL, config(),
        [](http::response_data_type) {},
        [&](http::error aError) { viaFunctor = aError; });

    auto pHandler = std::make_unique<recording_handler>();

    auto &handler = *pHandler;

    const auto pHandlerGet = pContext->make_get(REFUSED_URL, config(), std::move(pHandler));

    REQUIRE(pFunctorGet->try_submit());
    REQUIRE(pHandlerGet->try_submit());

    while (pContext->main_try_handle_completed_request()) {}

    REQUIRE(viaFunctor == handler.last_error);
    REQUIRE(viaFunctor == http::error::network_error);
}

TEST_CASE("a handler is destroyed with its request", "[jfc::http::response_handler]") {
    struct counting_handler final : http::response_handler
    {
        std::size_t &liveCount;

        explicit counting_handler(std::size_t &aLiveCount) : liveCount(aLiveCount)
        {
            ++liveCount;
        }

        ~counting_handler() override { --liveCount; }

        bool worker_on_data(http::response_chunk_type) override { return true; }
        void main_on_success() override {}
        void main_on_failure(const http::error) override {}
    };

    std::size_t liveCount = 0;

    {
        const auto pContext = curl_context();

        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            std::make_unique<counting_handler>(liveCount));

        REQUIRE(liveCount == 1);
    }

    REQUIRE(liveCount == 0);
}
