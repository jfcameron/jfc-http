// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_request.h>
#include <jfc/http/response_handler.h>
#include <jfc/http/types.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {
    class _reading_handler final : public jfc::http::response_handler
    {
    public:
        std::size_t observed = 0;

        void worker_on_status(const jfc::http::response_status_type aStatus) override
        {
            observed += static_cast<std::size_t>(aStatus);
        }

        void worker_on_header(const jfc::http::header_view_type aName,
            const jfc::http::header_view_type aValue) override
        {
            for (const char character : aName) observed += static_cast<unsigned char>(character);

            for (const char character : aValue) observed += static_cast<unsigned char>(character);
        }

        bool worker_on_data(jfc::http::response_chunk_type) override { return true; }

        void main_on_success() override {}

        void main_on_failure(const jfc::http::error) override {}
    };
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *aData, const std::size_t aSize)
{
    std::string content(reinterpret_cast<const char *>(aData), aSize);

    _reading_handler handler;

    jfc::http::response_sink sink{.pHandler = &handler};

    std::string_view remaining(content);

    while (!remaining.empty())
    {
        const auto lineEnd = remaining.find('\n');

        const auto line = lineEnd == std::string_view::npos
            ? remaining
            : remaining.substr(0, lineEnd + 1);

        static_cast<void>(jfc::http::receive_response_header(
            content.data() + (line.data() - content.data()), 1, line.size(), &sink));

        if (lineEnd == std::string_view::npos) break;

        remaining.remove_prefix(lineEnd + 1);
    }

    if (sink.status_reported && (sink.status < 100 || sink.status > 999)) __builtin_trap();

    static volatile std::size_t observed = 0;

    observed = observed + handler.observed;

    return 0;
}
