// © Joseph Cameron - All Rights Reserved

#include <jfc/http.h>

#include <jfc/http/curl_context.h>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace jfc;

int main(int, char **)
{
    std::vector<std::thread> workers;

    bool failed = false;

    const http::task_submitter_type submit = [&workers](std::vector<http::task_type> &&aTasks) {
        for (auto &task : aTasks) workers.emplace_back(std::move(task));
    };

    {
        auto pHttp = http::curl_context::make({.submit = submit});

        http::request_shared_ptr_type pGet = pHttp->make_get(
            "https://localhost/get_endpoint",
            {
                .user_agent = "Mozilla/5.0 (Android 4.4; Mobile; rv:41.0) Gecko/41.0 Firefox/41.0",
                .timeout_milliseconds = 300000,
                .headers = {"Connection: close"},
            },
            [](http::response_data_type aData) // response handler
            {
                const std::string response(aData.begin(), aData.end());

                std::cout << "get response: " << response << "\n";
            },
            [](http::error) // failure handler
            {
                std::cout << "get failed\n";
            });

        class counting_handler final : public http::response_handler
        {
            std::size_t m_ReceivedBytes = 0;

            std::optional<std::size_t> m_TotalBytes;

            std::optional<http::response_status_type> m_Status;

        public:
            void worker_on_status(const http::response_status_type aStatus) override
            {
                m_Status = aStatus;
            }

            bool worker_on_data(http::response_chunk_type aChunk) override
            {
                m_ReceivedBytes += aChunk.size();

                return true;
            }

            void worker_on_progress(const std::size_t aReceivedBytes,
                const std::optional<std::size_t> aTotalBytes) override
            {
                m_TotalBytes = aTotalBytes;

                if (aTotalBytes)
                {
                    std::cout << "  " << aReceivedBytes << " / " << *aTotalBytes << " bytes\n";
                }
            }

            void worker_on_complete() override
            {
                std::cout << "worker finished. Bytes received: " << m_ReceivedBytes << "\n";
            }

            void main_on_success() override
            {
                std::cout << "main succeeded, received " << m_ReceivedBytes << " bytes\n";
            }

            void main_on_failure(const http::error) override
            {
                std::cout << "main failed, status: "
                    << (m_Status ? std::to_string(*m_Status) : std::string("no response"))
                    << "\n";
            }

            counting_handler() = default;
        };

        auto pPost = pHttp->make_post(
            "https://localhost/post_endpoint",
            "good=morning",
            {
                .user_agent = "libcurl-agent/1.0",
                .timeout_milliseconds = 300000,
                .headers = {"Content-Type: application/x-www-form-urlencoded"},
            },
            std::make_unique<counting_handler>());

        if (!pGet->try_submit() || !pPost->try_submit())
        {
            std::cerr << "could not submit the requests\n";

            failed = true;
        }

        while (!failed && pHttp->outstanding_request_count())
        {
            if (!pHttp->main_try_handle_completed_request())
                std::this_thread::yield();
        }

        pHttp->cancel_all();
    }

    for (auto &worker : workers) worker.join();

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
