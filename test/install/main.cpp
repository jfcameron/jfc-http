// © Joseph Cameron - MIT License

#include <jfc/http.h>
#include <jfc/http/curl_context.h>
#include <jfc/http/null_context.h>

#if __has_include(<jfc/http/curl_request.h>)
#error "a private header was installed"
#endif

#include <optional>

int main() {
    const auto pContext = jfc::http::curl_context::make();

    std::optional<jfc::http::failure> reported;

    const auto pGet = pContext->make_get("http://127.0.0.1:9/", {.timeout_milliseconds = 2000},
        [](jfc::http::response) {},
        [&](const jfc::http::failure &aFailure) { reported = aFailure; });

    if (!pGet->try_submit() || !pContext->main_try_handle_completed_request()) return 1;

    return reported && reported->error == jfc::http::error::network_error && jfc::http::null_context::make() ? 0 : 1;
}
