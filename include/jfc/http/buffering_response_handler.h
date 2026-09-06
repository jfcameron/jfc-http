// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_BUFFERING_RESPONSE_HANDLER_H
#define JFC_HTTP_BUFFERING_RESPONSE_HANDLER_H

#include <jfc/http/response_handler.h>
#include <jfc/http/types.h>

#include <cstddef>
#include <optional>

namespace jfc::http {
    inline constexpr std::size_t DEFAULT_MAXIMUM_RESPONSE_BYTES = 16u * 1024u * 1024u; ///< default max is 16mb

    /// \brief instance configuration for \ref buffering_response_handler
    struct buffering_config final {
        std::size_t maximum_bytes = DEFAULT_MAXIMUM_RESPONSE_BYTES; ///< refuse the transfer once the body would exceed this many bytes
    };

    /// \brief a handler that collects the whole response
    class buffering_response_handler : public http::response_handler { 
    public:
        void worker_on_status(const response_status_type aStatus) override;

        [[nodiscard]] bool worker_on_data(response_chunk_type aChunk) override;

        void main_on_success() override;

        void main_on_failure(const error aError) override;

        /// \brief the bytes collected so far
        [[nodiscard]] const response_data_type &response() const;

        /// \brief whether the limit is what stopped the transfer
        [[nodiscard]] bool exceeded_limit() const;

        /// \brief the status the server sent, empty if it never answered
        [[nodiscard]] std::optional<response_status_type> status() const;

        buffering_response_handler(response_functor_type aOnSuccess,
            failure_functor_type aOnFailure,
            const buffering_config &aConfig = {});

        ~buffering_response_handler() override = default;

    private:
        response_data_type m_Buffer;

        response_functor_type m_SuccessFunctor;
        failure_functor_type m_FailureFunctor;

        std::size_t m_MaximumBytes;

        std::optional<response_status_type> m_Status;

        bool m_bExceededLimit = false;
    };
}

#endif
