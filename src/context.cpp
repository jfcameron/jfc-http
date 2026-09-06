// © Joseph Cameron - All Rights Reserved

#include <jfc/http/buffering_response_handler.h>
#include <jfc/http/context.h>

#include <memory>
#include <utility>

using namespace jfc;

http::request_shared_ptr_type http::context::make_get(const std::string &aURL,
    const http::request_config &aConfig,
    http::response_functor_type aOnSuccess,
    http::failure_functor_type aOnFailure
) {
    return make_get(aURL,
        aConfig,
        std::make_unique<http::buffering_response_handler>(std::move(aOnSuccess), std::move(aOnFailure)));
}

http::post_shared_ptr_type http::context::make_post(const std::string &aURL,
    const std::string &aPostData,
    const http::request_config &aConfig,
    http::response_functor_type aOnSuccess,
    http::failure_functor_type aOnFailure
) {
    return make_post(aURL,
        aPostData,
        aConfig,
        std::make_unique<http::buffering_response_handler>(std::move(aOnSuccess), std::move(aOnFailure)));
}
