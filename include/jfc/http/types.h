// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_TYPES_H
#define JFC_HTTP_TYPES_H

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace jfc::http
{
    class context;
    class exception;
    class post;
    class request;
    class buffering_response_handler;
    class response_handler;
    struct buffering_config;
    struct policy;
    struct request_config;

    using response_data_type = std::vector<char>;

    using response_chunk_type = std::span<const char>;

    using milliseconds_type = std::size_t;

    using response_status_type = int;

    using header_collection_type = std::vector<std::string>;

    using header_view_type = std::string_view;

    /// \brief compare two header names the way http defines them: case-insensitively
    [[nodiscard]] bool header_name_equals(const header_view_type aLeft,
        const header_view_type aRight);

    /// \brief how a request failed
    enum class error {
        none,
        unsupported_protocol,
        network_error,
        cancelled, ///< the caller asked for this request to be abandoned
        handler_threw, ///< a handler threw, and the exception is waiting to be rethrown on the main thread
        aborted_by_handler, ///< the handler stopped the transfer by returning false from `worker_on_data`
        too_many_redirects, ///< the redirect chain was longer than \ref request_config::maximum_redirects
        range_not_satisfied, ///< the server did not honour the range that was asked for
        http_error, ///< the server answered, and its status was >= 400
        response_too_large, ///< a buffering handler was asked to hold more than its limit
        unhandled_error //!< used when no other information is available
    };

    using response_functor_type = std::function<void(response_data_type)>;

    using failure_functor_type = std::function<void(error)>;

    using task_type = std::function<void()>;

    using task_submitter_type = std::function<void(std::vector<task_type> &&aTasks)>;

    using context_shared_ptr_type = std::shared_ptr<context>;
    using request_shared_ptr_type = std::shared_ptr<request>;
    using post_shared_ptr_type = std::shared_ptr<post>;
    using response_handler_ptr_type = std::unique_ptr<response_handler>;
}

#endif
