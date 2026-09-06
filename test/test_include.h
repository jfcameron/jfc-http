// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_TEST_INCLUDE_H
#define JFC_HTTP_TEST_INCLUDE_H

#include <jfc/http.h>

#include <jfc/http/curl_context.h>
#include <jfc/http/null_context.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace jfc::http::test {
    [[nodiscard]] inline http::request_config config() {
        return {.user_agent = "jfc-http_request test", .timeout_milliseconds = 1000};
    }

    constexpr const char *REFUSED_URL = "http://127.0.0.1:9/";

    class deferring_submitter final {
    public:
        std::vector<http::task_type> tasks;

        [[nodiscard]] http::task_submitter_type submitter() {
            return [this](std::vector<http::task_type> &&aTasks)
            {
                for (auto &task : aTasks) tasks.push_back(std::move(task));
            };
        }

        std::size_t run_all() {
            const auto count = tasks.size();

            for (auto &task : tasks) task();

            tasks.clear();

            return count;
        }

        [[nodiscard]] std::size_t pending() const { return tasks.size(); }
    };

    [[nodiscard]] inline http::context_shared_ptr_type null_context()
    {
        return http::null_context::make();
    }

    [[nodiscard]] inline http::context_shared_ptr_type curl_context()
    {
        return http::curl_context::make();
    }

    [[nodiscard]] inline http::context_shared_ptr_type curl_context(http::task_submitter_type aSubmit)
    {
        return http::curl_context::make({.submit = std::move(aSubmit)});
    }

    [[nodiscard]] inline std::string to_string(const http::response_data_type &aData)
    {
        return std::string(aData.begin(), aData.end());
    }
}

#endif
