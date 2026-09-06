// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <jfc/http/curl_context.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    void no_op_success(http::response_data_type) {}
    void no_op_failure(http::error) {}
}

TEST_CASE("the serial submitter runs every task, in order, before returning",
    "[jfc::http::policy]") {
    const auto submit = http::make_serial_task_submitter();

    REQUIRE(submit);

    std::vector<int> order;

    std::vector<http::task_type> tasks;

    for (int i = 0; i < 4; ++i) tasks.emplace_back([&order, i]() { order.push_back(i); });

    submit(std::move(tasks));

    REQUIRE(order == std::vector<int>{0, 1, 2, 3});
}

TEST_CASE("a context with no policy performs work inline", "[jfc::http::policy]") {
    const auto pContext = curl_context();

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(REFUSED_URL, config(),
        no_op_success, [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());

    REQUIRE(pContext->outstanding_request_count() == 1);
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::network_error);
}

TEST_CASE("an empty submitter degrades to serial rather than crashing", "[jfc::http::policy]") {
    const auto pContext = http::curl_context::make({.submit = nullptr});

    http::error reported = http::error::none;

    const auto pGet = pContext->make_get(REFUSED_URL, config(),
        no_op_success, [&](http::error aError) { reported = aError; });

    REQUIRE(pGet->try_submit());
    REQUIRE(pContext->main_try_handle_completed_request());

    REQUIRE(reported == http::error::network_error);
}

TEST_CASE("submission does not wait for the work", "[jfc::http::policy]") {
    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    const auto pGet = pContext->make_get(REFUSED_URL, config(), no_op_success, no_op_failure);

    REQUIRE(pGet->try_submit());

    REQUIRE(submitter.pending() == 1);

    REQUIRE_FALSE(pContext->main_try_handle_completed_request());
    REQUIRE(pContext->outstanding_request_count() == 1);

    REQUIRE(submitter.run_all() == 1);

    REQUIRE(pContext->main_try_handle_completed_request());
    REQUIRE(pContext->outstanding_request_count() == 0);
}

TEST_CASE("each submitted request produces exactly one task", "[jfc::http::policy]") {
    constexpr std::size_t REQUEST_COUNT = 5;

    deferring_submitter submitter;

    const auto pContext = curl_context(submitter.submitter());

    std::vector<http::request_shared_ptr_type> requests;

    for (std::size_t i = 0; i < REQUEST_COUNT; ++i)
    {
        requests.push_back(pContext->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure));
    }

    for (const auto &pRequest : requests) REQUIRE(pRequest->try_submit());

    REQUIRE(submitter.pending() == REQUEST_COUNT);
    REQUIRE(pContext->outstanding_request_count() == REQUEST_COUNT);

    REQUIRE(submitter.run_all() == REQUEST_COUNT);

    std::size_t handled = 0;

    while (pContext->main_try_handle_completed_request()) ++handled;

    REQUIRE(handled == REQUEST_COUNT);
}

TEST_CASE("a context waits for work still running when it is destroyed", "[jfc::http::context]") {
    std::atomic<bool> bTaskFinished{false};

    std::vector<std::thread> threads;

    bool bFinishedBeforeDestructorReturned = false;

    {
        const auto pContext = curl_context(
            [&threads, &bTaskFinished](std::vector<http::task_type> &&aTasks)
            {
                for (auto &task : aTasks)
                {
                    threads.emplace_back([task = std::move(task), &bTaskFinished]()
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        task();

                        bTaskFinished.store(true);
                    });
                }
            });

        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);

        REQUIRE(pGet->try_submit());
    }

    bFinishedBeforeDestructorReturned = bTaskFinished.load();

    for (auto &thread : threads) thread.join();

    REQUIRE(bFinishedBeforeDestructorReturned);
}

TEST_CASE("a task the submitter discards does not hang the destructor",
    "[jfc::http::context]") {
    std::size_t discarded = 0;

    {
        const auto pContext = curl_context(
            [&discarded](std::vector<http::task_type> &&aTasks)
            {
                discarded += aTasks.size();
            });

        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);

        REQUIRE(pGet->try_submit());
    }

    REQUIRE(discarded == 1);
}

TEST_CASE("a throwing submitter does not strand the count", "[jfc::http::context]") {
    {
        const auto pContext = curl_context(
            [](std::vector<http::task_type> &&) { throw http::exception("submitter refused"); });

        const auto pGet = pContext->make_get(REFUSED_URL, config(),
            no_op_success, no_op_failure);

        REQUIRE_THROWS_AS(pGet->try_submit(), http::exception);
    }

    SUCCEED("the context was destroyed without waiting on a task that never ran");
}
