// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CONTEXT_H
#define JFC_HTTP_CONTEXT_H

#include <jfc/http/post.h>
#include <jfc/http/request.h>
#include <jfc/http/response_handler.h>
#include <jfc/http/types.h>

#include <cstddef>
#include <string>

namespace jfc::http
{
    /// \brief library entry point: makes requests and moves them between main and the workers
    class context {
    public:
        /// \brief create a GET whose response the worker processes before returning to main
        [[nodiscard]] virtual request_shared_ptr_type make_get(const std::string &aURL,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) = 0;

        /// \brief create a GET whose raw response is returned directly to main
        [[nodiscard]] request_shared_ptr_type make_get(const std::string &aURL,
            const request_config &aConfig,
            response_functor_type aOnSuccess,
            failure_functor_type aOnFailure);

        /// \brief create a POST whose response the worker processes before returning to main
        [[nodiscard]] virtual post_shared_ptr_type make_post(const std::string &aURL,
            const std::string &aPostData,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) = 0;

        /// \brief create a POST whose raw response is returned directly to main
        [[nodiscard]] post_shared_ptr_type make_post(const std::string &aURL,
            const std::string &aPostData,
            const request_config &aConfig,
            response_functor_type aOnSuccess,
            failure_functor_type aOnFailure);

        /// \brief run the handler for one completed request
        /// \warn must be called by the single "main" thread
        /// \return true if a request was handled
        virtual bool main_try_handle_completed_request() = 0;

        /// \brief ask for every outstanding request to be abandoned
        ///
        /// Main-thread only, unlike `request::cancel`: this walks the outstanding list, which
        /// belongs to main. Requests already finished are unaffected.
        virtual void cancel_all() = 0;

        /// \brief the number of requests submitted and not yet handled by main
        ///
        /// Counts requests whose handlers main still owes, which includes those a worker has
        /// already finished fetching. It is not a count of what is running.
        [[nodiscard]] virtual std::size_t outstanding_request_count() const = 0;

        virtual ~context() = default;
    };
}

#endif
