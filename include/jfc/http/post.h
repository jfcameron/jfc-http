// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_POST_H
#define JFC_HTTP_POST_H

#include <jfc/http/request.h>
#include <jfc/http/types.h>

#include <string>

namespace jfc::http {
    class post : public request {
    public:
        /// \brief replace the body sent by the next send
        /// \return false if called while the post is enqueued, in which case the body is unchanged
        [[nodiscard]] virtual bool try_update_postdata(const std::string &aPostData) = 0;

        virtual ~post() override = default;
    };
}

#endif
