// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/curl_request.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class recording_handler final : public http::response_handler
    {
    public:
        std::vector<std::string> calls;

        std::string received;

        std::size_t refuse_after_bytes = 0; //!< 0 disables refusal

        std::optional<std::size_t> last_total;

        std::size_t last_received = 0;

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            if (refuse_after_bytes && received.size() + aChunk.size() > refuse_after_bytes)
            {
                calls.push_back("refused");

                return false;
            }

            received.append(aChunk.data(), aChunk.size());

            calls.push_back("data:" + std::to_string(aChunk.size()));

            return true;
        }

        void worker_on_complete() override { calls.push_back("complete"); }

        void worker_on_progress(const std::size_t aReceived,
            const std::optional<std::size_t> aTotal) override
        {
            last_received = aReceived;
            last_total = aTotal;

            calls.push_back("progress");
        }

        void main_on_success() override { calls.push_back("main_success"); }

        void main_on_failure(const http::error) override { calls.push_back("main_failure"); }
    };

    [[nodiscard]] http::response_chunk_type chunk_of(const std::string &aText)
    {
        return http::response_chunk_type(aText.data(), aText.size());
    }
}

TEST_CASE("arriving bytes are handed straight to the handler", "[jfc::http::response_handler]") {
    recording_handler handler;

    http::response_sink sink{.pHandler = &handler};

    SECTION("**a chunk reaches the handler intact**")
    {
        std::string content("hello");

        REQUIRE(http::write_response_chunk(content.data(), 1, content.size(), &sink)
            == content.size());

        REQUIRE(handler.received == "hello");
        REQUIRE_FALSE(sink.aborted);
        REQUIRE(sink.received_bytes == 5);
    }

    SECTION("**successive chunks accumulate in the handler, not in the library**")
    {
        std::string first("abc");
        std::string second("de");

        REQUIRE(http::write_response_chunk(first.data(), 1, first.size(), &sink) == 3);
        REQUIRE(http::write_response_chunk(second.data(), 1, second.size(), &sink) == 2);

        REQUIRE(handler.received == "abcde");
        REQUIRE(sink.received_bytes == 5);
    }

    SECTION("**the item size and count are multiplied, as curl's callback contract says**")
    {
        std::string content("abcdefgh");

        REQUIRE(http::write_response_chunk(content.data(), 4, 2, &sink) == 8);

        REQUIRE(handler.received == "abcdefgh");
    }

    SECTION("**a handler that refuses abandons the transfer**")
    {
        handler.refuse_after_bytes = 4;

        std::string content("abcdefgh");

        REQUIRE(http::write_response_chunk(content.data(), 1, content.size(), &sink) == 0);

        REQUIRE(sink.aborted);
        REQUIRE(handler.received.empty());
    }
}

TEST_CASE("the transport imposes no ceiling on a response", "[jfc::http::response_handler]") {
    class counting_handler final : public http::response_handler
    {
    public:
        std::size_t total = 0;

        bool worker_on_data(http::response_chunk_type aChunk) override
        {
            total += aChunk.size();

            return true;
        }

        void main_on_success() override {}
        void main_on_failure(const http::error) override {}
    };

    counting_handler handler;

    http::response_sink sink{.pHandler = &handler};

    std::vector<char> buffer(64u * 1024u, 'x');

    while (handler.total <= http::DEFAULT_MAXIMUM_RESPONSE_BYTES)
    {
        REQUIRE(http::write_response_chunk(buffer.data(), 1, buffer.size(), &sink)
            == buffer.size());
    }

    REQUIRE(handler.total > http::DEFAULT_MAXIMUM_RESPONSE_BYTES);
    REQUIRE_FALSE(sink.aborted);
}

TEST_CASE("progress is reported absolutely, not per-transfer", "[jfc::http::response_handler]") {
    recording_handler handler;

    SECTION("**a fresh transfer reports what curl reports**")
    {
        http::response_sink sink{.pHandler = &handler};

        REQUIRE(http::report_transfer_progress(&sink, 1000, 250, 0, 0) == 0);

        REQUIRE(handler.last_received == 250);
        REQUIRE(handler.last_total.has_value());
        REQUIRE(*handler.last_total == 1000);
    }

    SECTION("**a resumed one adds the offset to both halves**")
    {
        http::response_sink sink{.pHandler = &handler, .resume_offset = 400};

        REQUIRE(http::report_transfer_progress(&sink, 600, 100, 0, 0) == 0);

        REQUIRE(handler.last_received == 500);
        REQUIRE(*handler.last_total == 1000);
    }

    SECTION("**an unknown total is empty rather than zero**")
    {
        http::response_sink sink{.pHandler = &handler};

        REQUIRE(http::report_transfer_progress(&sink, 0, 128, 0, 0) == 0);

        REQUIRE(handler.last_received == 128);
        REQUIRE_FALSE(handler.last_total.has_value());
    }
}

TEST_CASE("the buffering handler collects a body through the public interface",
    "[jfc::http::buffering_response_handler]") {
    http::response_data_type delivered;

    http::error reported = http::error::none;

    bool succeeded = false;

    const auto on_success = [&](http::response_data_type aData)
    {
        delivered = std::move(aData);

        succeeded = true;
    };

    const auto on_failure = [&](http::error aError) { reported = aError; };

    SECTION("**it assembles the chunks and hands over one body**")
    {
        http::buffering_response_handler handler(on_success, on_failure);

        REQUIRE(handler.worker_on_data(chunk_of("abc")));
        REQUIRE(handler.worker_on_data(chunk_of("de")));

        handler.main_on_success();

        REQUIRE(succeeded);
        REQUIRE(to_string(delivered) == "abcde");
        REQUIRE_FALSE(handler.exceeded_limit());
    }

    SECTION("**the default limit is a real bound, not a nominal one**")
    {
        REQUIRE(http::DEFAULT_MAXIMUM_RESPONSE_BYTES > 1024u * 1024u);
        REQUIRE(http::DEFAULT_MAXIMUM_RESPONSE_BYTES <= 64u * 1024u * 1024u);
    }

    SECTION("**it refuses the chunk that would cross its limit**")
    {
        http::buffering_response_handler handler(on_success, on_failure, {.maximum_bytes = 4});

        REQUIRE_FALSE(handler.worker_on_data(chunk_of("abcde")));

        REQUIRE(handler.exceeded_limit());
        REQUIRE(handler.response().empty());
    }

    SECTION("**the limit counts what is already buffered, not just the chunk in hand**")
    {
        http::buffering_response_handler handler(on_success, on_failure, {.maximum_bytes = 4});

        REQUIRE(handler.worker_on_data(chunk_of("abc")));
        REQUIRE_FALSE(handler.worker_on_data(chunk_of("de")));

        REQUIRE(handler.exceeded_limit());
        REQUIRE(handler.response().size() == 3);
    }

    SECTION("**and translates the library's abort into something it can explain**")
    {
        http::buffering_response_handler handler(on_success, on_failure, {.maximum_bytes = 4});

        REQUIRE_FALSE(handler.worker_on_data(chunk_of("abcde")));

        handler.main_on_failure(http::error::aborted_by_handler);

        REQUIRE(reported == http::error::response_too_large);
    }

    SECTION("**while a failure it did not cause passes through unchanged**")
    {
        http::buffering_response_handler handler(on_success, on_failure);

        handler.main_on_failure(http::error::network_error);

        REQUIRE(reported == http::error::network_error);
    }
}

TEST_CASE("a failed transfer never reports completion", "[jfc::http::response_handler]") {
    const auto pContext = curl_context();

    auto pHandler = std::make_unique<recording_handler>();

    auto &handler = *pHandler;

    const auto pGet = pContext->make_get(REFUSED_URL, config(), std::move(pHandler));

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "complete")
        == handler.calls.end());

    REQUIRE(std::find(handler.calls.begin(), handler.calls.end(), "main_success")
        == handler.calls.end());

    REQUIRE(handler.calls.back() == "main_failure");
}

TEST_CASE("a resumed request is configured with its offset", "[jfc::http::request_config]") {
    const http::request_config defaults;

    REQUIRE(defaults.resume_from_byte == 0);

    const auto pContext = curl_context();

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(REFUSED_URL,
        {.timeout_milliseconds = 1000, .resume_from_byte = 4096},
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::network_error);
}

TEST_CASE("timeouts can be configured for a large body", "[jfc::http::request_config]") {
    const http::request_config defaults;

    REQUIRE(defaults.timeout_milliseconds == 30000);
    REQUIRE(defaults.connect_timeout_milliseconds > 0);
    REQUIRE(defaults.stall_timeout_milliseconds == 0);

    REQUIRE(defaults.fail_on_http_error);

    const auto pContext = curl_context();

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(REFUSED_URL,
        {
            .timeout_milliseconds = 0,
            .connect_timeout_milliseconds = 1000,
            .stall_timeout_milliseconds = 2000,
        },
        [](http::response_data_type) {},
        [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::network_error);
}
