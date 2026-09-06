// © Joseph Cameron - All Rights Reserved

#include <jfc/http/null_context.h>

#include <memory>
#include <utility>

using namespace jfc;

bool http::null_get::try_submit() { return false; }

void http::null_get::cancel() {}

bool http::null_post::try_submit() { return false; }

void http::null_post::cancel() {}

bool http::null_post::try_update_postdata(const std::string &) { return false; }

http::request_shared_ptr_type http::null_context::make_get(const std::string &,
    const http::request_config &,
    http::response_handler_ptr_type &&
) {
    return std::make_shared<http::null_get>();
}

http::post_shared_ptr_type http::null_context::make_post(const std::string &,
    const std::string &,
    const http::request_config &,
    http::response_handler_ptr_type &&
) {
    return std::make_shared<http::null_post>();
}

bool http::null_context::main_try_handle_completed_request() { return false; }

void http::null_context::cancel_all() {}

std::size_t http::null_context::outstanding_request_count() const { return 0; }

http::context_shared_ptr_type http::null_context::make(http::policy aPolicy) {
    return http::context_shared_ptr_type(new http::null_context(std::move(aPolicy)));
}

http::null_context::null_context(http::policy) {}
