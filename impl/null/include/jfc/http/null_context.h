// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_NULL_CONTEXT_H
#define JFC_HTTP_NULL_CONTEXT_H

#include <jfc/http/context.h>
#include <jfc/http/policy.h>
#include <jfc/http/post.h>
#include <jfc/http/request.h>
#include <jfc/http/types.h>

#include <cstddef>
#include <string>

namespace jfc::http {
    class null_get final : public http::request {
    public:
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        null_get() = default;

        ~null_get() override = default;
    };

    class null_post final : public http::post {
    public:
        [[nodiscard]] bool try_submit() override;

        void cancel() override;

        [[nodiscard]] bool try_update_postdata(const std::string &aPostData) override;

        null_post() = default;

        ~null_post() override = default;
    };

    /// \brief does nothing; for tests, and for porting work
    class null_context final : public http::context {
    public:
        /// \brief create a context that performs no transfers
        [[nodiscard]] static context_shared_ptr_type make(http::policy aPolicy = {});

        using http::context::make_get;
        using http::context::make_post;

        [[nodiscard]] request_shared_ptr_type make_get(const std::string &aURL,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) override;

        [[nodiscard]] post_shared_ptr_type make_post(const std::string &aURL,
            const std::string &aPostData,
            const request_config &aConfig,
            response_handler_ptr_type &&aHandler) override;

        bool main_try_handle_completed_request() override;

        void cancel_all() override;

        [[nodiscard]] std::size_t outstanding_request_count() const override;

        ~null_context() override = default;

    private:
        explicit null_context(http::policy aPolicy = {});
    };
}

#endif
