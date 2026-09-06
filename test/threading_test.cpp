// © Joseph Cameron - All Rights Reserved

#include <jfc/catch.hpp>
#include <jfc/types.h>

#include "test_include.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;
using namespace jfc::http::test;

namespace {
    class test_pool final
    {
    public:
        void add(std::vector<http::task_type> &&aTasks)
        {
            {
                const std::lock_guard<std::mutex> lock(m_Mutex);

                for (auto &task : aTasks) m_Tasks.push_back(std::move(task));
            }

            m_Condition.notify_all();
        }

        [[nodiscard]] http::task_submitter_type submitter()
        {
            return [this](std::vector<http::task_type> &&aTasks) { add(std::move(aTasks)); };
        }

        explicit test_pool(const std::size_t aThreadCount)
        {
            for (std::size_t i = 0; i < aThreadCount; ++i)
                m_Threads.emplace_back([this]() { work(); });
        }

        ~test_pool()
        {
            {
                const std::lock_guard<std::mutex> lock(m_Mutex);

                m_bStopping = true;
            }

            m_Condition.notify_all();

            for (auto &thread : m_Threads) thread.join();
        }

    private:
        void work()
        {
            for (;;)
            {
                http::task_type task;

                {
                    std::unique_lock<std::mutex> lock(m_Mutex);

                    m_Condition.wait(lock, [this]() { return m_bStopping || !m_Tasks.empty(); });

                    if (m_Tasks.empty()) return;

                    task = std::move(m_Tasks.back());

                    m_Tasks.pop_back();
                }

                task();
            }
        }

        std::mutex m_Mutex;
        std::condition_variable m_Condition;

        std::vector<http::task_type> m_Tasks;

        bool m_bStopping = false;

        std::vector<std::thread> m_Threads;
    };
}

TEST_CASE("a real pool performs the fetches while main drains completions",
    "[jfc::http::context]") {
    constexpr std::size_t REQUEST_COUNT = 16;
    constexpr std::size_t WORKER_COUNT = 4;

    test_pool pool(WORKER_COUNT);

    const auto pContext = curl_context(pool.submitter());

    std::atomic<std::size_t> failures{0};

    std::vector<http::request_shared_ptr_type> requests;

    for (std::size_t i = 0; i < REQUEST_COUNT; ++i)
    {
        requests.push_back(pContext->make_get(REFUSED_URL, config(),
            [](http::response_data_type) {},
            [&failures](http::error) { ++failures; }));
    }

    for (const auto &pRequest : requests) REQUIRE(pRequest->try_submit());

    REQUIRE(pContext->outstanding_request_count() == REQUEST_COUNT);

    std::size_t handled = 0;

    while (handled < REQUEST_COUNT)
    {
        if (pContext->main_try_handle_completed_request()) ++handled;
        else std::this_thread::yield();
    }

    REQUIRE(handled == REQUEST_COUNT);
    REQUIRE(failures.load() == REQUEST_COUNT);
    REQUIRE(pContext->outstanding_request_count() == 0);
}

TEST_CASE("two contexts can share one pool", "[jfc::http::context]") {
    constexpr std::size_t REQUESTS_EACH = 4;

    test_pool pool(3);

    const auto pFirst = curl_context(pool.submitter());
    const auto pSecond = curl_context(pool.submitter());

    std::atomic<std::size_t> failures{0};

    std::vector<http::request_shared_ptr_type> requests;

    for (const auto &pContext : {pFirst, pSecond})
    {
        for (std::size_t i = 0; i < REQUESTS_EACH; ++i)
        {
            requests.push_back(pContext->make_get(REFUSED_URL, config(),
                [](http::response_data_type) {},
                [&failures](http::error) { ++failures; }));
        }
    }

    for (const auto &pRequest : requests) REQUIRE(pRequest->try_submit());

    std::size_t handled = 0;

    while (handled < REQUESTS_EACH * 2)
    {
        if (pFirst->main_try_handle_completed_request()) ++handled;
        else if (pSecond->main_try_handle_completed_request()) ++handled;
        else std::this_thread::yield();
    }

    REQUIRE(failures.load() == REQUESTS_EACH * 2);
}

TEST_CASE("contexts can be created and destroyed concurrently", "[jfc::http::context]") {
    constexpr std::size_t THREAD_COUNT = 4;

    const auto pHeld = curl_context();

    std::atomic<std::size_t> constructed{0};

    std::vector<std::thread> threads;

    for (std::size_t i = 0; i < THREAD_COUNT; ++i)
    {
        threads.emplace_back([&constructed]()
        {
            for (std::size_t j = 0; j < 16; ++j)
            {
                const auto pContext = curl_context();

                if (pContext->outstanding_request_count() == 0) ++constructed;
            }
        });
    }

    for (auto &thread : threads) thread.join();

    REQUIRE(constructed.load() == THREAD_COUNT * 16);

    REQUIRE(pHeld->outstanding_request_count() == 0);
}
