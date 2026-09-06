// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_RESPONSE_HANDLER_H
#define JFC_HTTP_RESPONSE_HANDLER_H

#include <jfc/http/types.h>

#include <cstddef>
#include <optional>

namespace jfc::http {
    /// \brief interface for user-defined request handler types
    class response_handler {
    public:
        /// \brief called on the worker thread once the server's status line has been read
        ///
        /// \param aStatus the code from the status line -- 200, 206, 404, 503
        virtual void worker_on_status(const response_status_type aStatus) {
            static_cast<void>(aStatus);
        }

        /// \brief called on the worker thread for each response header
        virtual void worker_on_header(const header_view_type aName, const header_view_type aValue) {
            static_cast<void>(aName);
            static_cast<void>(aValue);
        }

        /// \brief called on the worker thread as each piece of the response arrives
        [[nodiscard]] virtual bool worker_on_data(response_chunk_type aChunk) = 0;

        /// \brief called on the worker thread once the whole response has arrived
        virtual void worker_on_complete() {}

        /// \brief called on the worker thread as the transfer proceeds
        virtual void worker_on_progress(const std::size_t aReceivedBytes,
            const std::optional<std::size_t> aTotalBytes)
        {
            static_cast<void>(aReceivedBytes);
            static_cast<void>(aTotalBytes);
        }

        /// \brief called by the main thread if the request succeeded
        /// \note guaranteed to be called after worker_on_complete has returned
        virtual void main_on_success() = 0;

        /// \brief called by the main thread if the request did not succeed
        virtual void main_on_failure(const error aError) = 0;

        virtual ~response_handler() = default;
    };
}

#endif
