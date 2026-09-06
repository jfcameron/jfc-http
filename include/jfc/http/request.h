// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_REQUEST_H
#define JFC_HTTP_REQUEST_H

#include <jfc/http/types.h>

#include <cstddef>
#include <string>

namespace jfc::http {
    constexpr const char *LIBRARY_PRODUCT_TOKEN = "jfc-http_request/0.75";

    /// \brief everything about a request that has a sensible default
    struct request_config final {
        std::string user_agent = {};                            ///< the *calling application's* product token, such as `"MyGame/1.2.3"`
        milliseconds_type timeout_milliseconds = 30000;         ///< how long the **whole** transfer may take before it is abandoned
        milliseconds_type connect_timeout_milliseconds = 10000; ///< how long the connection itself may take to establish
        milliseconds_type stall_timeout_milliseconds = 0;       ///< abandon the transfer if no data arrives for this long; 0 disables it
        bool follow_redirects = true;                           ///< follow a `Location` header rather than delivering the redirect itself
        std::size_t maximum_redirects = 5;                      ///< how many hops to follow before giving up
        std::size_t resume_from_byte = 0;                       ///< begin the transfer this many bytes into the resource
        std::string if_range = {};                              ///< the validator a resumed transfer is only valid against, sent as `If-Range`
        bool fail_on_http_error = true;                         ///< treat an http status of 400 or above as a failed request
        header_collection_type headers = {};                    ///< additional headers, each a complete "Name: value" line
    };

    /// \brief the complete `User-Agent` value for a config: the caller's token, then this library's
    ///
    /// Composed here rather than in an implementation so that every implementation sends the same
    /// thing, and so the rule is testable without a network.
    [[nodiscard]] std::string user_agent_header(const request_config &aConfig);

    /// \brief a request that has been built but not necessarily sent
    class request {
    public:
        /// \brief hand the request to the context's task submitter to be performed
        /// \return false if it is already in flight or if the context that made it is gone
        [[nodiscard]] virtual bool try_submit() = 0;

        /// \brief ask for this request to be abandoned
        virtual void cancel() = 0;

        virtual ~request() = default;
    };
}

#endif
