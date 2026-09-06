// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_request.h>

#include <jfc/http/exception.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

using namespace jfc;

namespace {
    [[nodiscard]] http::error _curlcode_to_error(const CURLcode aCurlCode) {
        switch (aCurlCode) {
            case CURLE_OK:
                return http::error::none;

            case CURLE_UNSUPPORTED_PROTOCOL:
                return http::error::unsupported_protocol;

            case CURLE_HTTP_RETURNED_ERROR:
                return http::error::http_error;

            case CURLE_RANGE_ERROR:
                return http::error::range_not_satisfied;

            case CURLE_TOO_MANY_REDIRECTS:
                return http::error::too_many_redirects;

            case CURLE_ABORTED_BY_CALLBACK:
            case CURLE_WRITE_ERROR:
                return http::error::aborted_by_handler;

            case CURLE_COULDNT_RESOLVE_PROXY:
            case CURLE_COULDNT_RESOLVE_HOST:
            case CURLE_COULDNT_CONNECT:
            case CURLE_OPERATION_TIMEDOUT:
            case CURLE_SSL_CONNECT_ERROR:
            case CURLE_PEER_FAILED_VERIFICATION:
            case CURLE_GOT_NOTHING:
            case CURLE_SEND_ERROR:
            case CURLE_RECV_ERROR:
                return http::error::network_error;

            default:
                return http::error::unhandled_error;
        }
    }

    [[nodiscard]] long _to_low_speed_seconds(const http::milliseconds_type aMilliseconds) {
        if (aMilliseconds == 0) return 0;

        return static_cast<long>(std::max<http::milliseconds_type>(1, (aMilliseconds + 999) / 1000));
    }
}

void http::capture_handler_exception(http::response_sink &aSink) {
    if (!aSink.pException) aSink.pException = std::current_exception();

    aSink.aborted = true;
}

void http::report_response_status(http::response_sink &aSink) {
    if (aSink.status_reported || !aSink.pHandle) return;

    long responseCode = 0;

    if (curl_easy_getinfo(aSink.pHandle, CURLINFO_RESPONSE_CODE, &responseCode) != CURLE_OK) return;

    if (responseCode <= 0) return;

    aSink.status = static_cast<http::response_status_type>(responseCode);
    aSink.status_reported = true;

    try {
        aSink.pHandler->worker_on_status(aSink.status);
    }
    catch (...) {
        http::capture_handler_exception(aSink);
    }
}

namespace {
    [[nodiscard]] std::string_view _trim(std::string_view aText) {
        constexpr const char *WHITESPACE = " \t\r\n";

        const auto first = aText.find_first_not_of(WHITESPACE);

        if (first == std::string_view::npos) return {};

        return aText.substr(first, aText.find_last_not_of(WHITESPACE) - first + 1);
    }

    [[nodiscard]] std::optional<http::response_status_type> _parse_status_code(const std::string_view aStatusLine) {
        const auto codeStart = aStatusLine.find(' ');

        if (codeStart == std::string_view::npos) return std::nullopt;

        const auto field = _trim(aStatusLine.substr(codeStart));
        http::response_status_type code = 0;
        const auto parsed = std::from_chars(field.data(), field.data() + field.size(), code);
        if (parsed.ec != std::errc{}) return std::nullopt;
        if (code < 100 || code > 999) return std::nullopt;

        return code;
    }
}

std::size_t http::receive_response_header(char *const apContent,
    const std::size_t aItemSize,
    const std::size_t aItemCount,
    void *const apUserPointer
) {
    const std::size_t lineByteCount(aItemSize * aItemCount);
    auto pSink(static_cast<http::response_sink *>(apUserPointer));
    const std::string_view line(apContent, lineByteCount);

    if (pSink->pCancelled && pSink->pCancelled->load(std::memory_order_acquire)) return 0;

    try {
        if (line.starts_with("HTTP/")) {
            if (const auto code = _parse_status_code(line)) {
                pSink->status = *code;
                pSink->status_reported = true;
                pSink->pHandler->worker_on_status(pSink->status);
            }
        }
        else if (const auto separator = line.find(':'); separator != std::string_view::npos) {
            pSink->pHandler->worker_on_header(_trim(line.substr(0, separator)), _trim(line.substr(separator + 1)));
        }
    }
    catch (...) {
        http::capture_handler_exception(*pSink);
        return 0;
    }

    return lineByteCount;
}

std::size_t http::write_response_chunk(char *const apContent,
    const std::size_t aItemSize,
    const std::size_t aItemCount,
    void *const apUserPointer
) {
    const std::size_t contentByteCount(aItemSize * aItemCount);
    auto pSink(static_cast<http::response_sink *>(apUserPointer));

    if (pSink->pException) return 0;

    if (pSink->pCancelled && pSink->pCancelled->load(std::memory_order_acquire)) return 0;

    try {
        if (!pSink->pHandler->worker_on_data(http::response_chunk_type(apContent, contentByteCount))) {
            pSink->aborted = true;
            return 0;
        }
    }
    catch (...) {
        http::capture_handler_exception(*pSink);
        return 0;
    }

    pSink->received_bytes += contentByteCount;
    return contentByteCount;
}

int http::report_transfer_progress(void *const apUserPointer,
    const curl_off_t aDownloadTotal,
    const curl_off_t aDownloadNow,
    const curl_off_t,
    const curl_off_t
) {
    auto pSink(static_cast<http::response_sink *>(apUserPointer));

    if (pSink->pCancelled && pSink->pCancelled->load(std::memory_order_acquire)) return 1;

    const std::size_t receivedBytes = pSink->resume_offset + static_cast<std::size_t>(aDownloadNow);

    const std::optional<std::size_t> totalBytes = aDownloadTotal > 0
        ? std::optional<std::size_t>(pSink->resume_offset + static_cast<std::size_t>(aDownloadTotal))
        : std::nullopt;

    try {
        pSink->pHandler->worker_on_progress(receivedBytes, totalBytes);
    }
    catch (...) {
        http::capture_handler_exception(*pSink);
        return 1;
    }

    return 0;
}

http::curl_request::curl_request(std::weak_ptr<http::curl_context> pContext,
    const std::string &aURL,
    const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler
) 
: m_pContext(pContext)
, m_pHandle([&aURL]()
    {
        auto handle = curl_easy_init();

        if (!handle) throw http::exception(
            "jfc::http::curl_request: could not initialize a curl handle for \"" + aURL + "\"");

        return handle;
    }()
    , [](CURL *p)
    {
        curl_easy_cleanup(p);
    })
, m_ResumeFromByte(aConfig.resume_from_byte)
, m_bFailOnHTTPError(aConfig.fail_on_http_error)
, m_ResponseHandler(std::move(aHandler)) {
    if (const auto pContext = m_pContext.lock()) {
        m_pShare = pContext->share();
        curl_easy_setopt(m_pHandle.get(), CURLOPT_SHARE, m_pShare->handle());
    }

    m_bResponseLocked.test_and_set();

    curl_easy_setopt(m_pHandle.get(), CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(m_pHandle.get(), CURLOPT_WRITEFUNCTION, http::write_response_chunk);
    curl_easy_setopt(m_pHandle.get(), CURLOPT_HEADERFUNCTION, http::receive_response_header);
    curl_easy_setopt(m_pHandle.get(), CURLOPT_XFERINFOFUNCTION, http::report_transfer_progress);
    curl_easy_setopt(m_pHandle.get(), CURLOPT_NOPROGRESS, 0L);

    curl_easy_setopt(m_pHandle.get(), CURLOPT_URL, aURL.c_str());
    const std::string userAgent = http::user_agent_header(aConfig);

    curl_easy_setopt(m_pHandle.get(), CURLOPT_USERAGENT, userAgent.c_str());

    http::header_collection_type headers = aConfig.headers;

    if (m_ResumeFromByte > 0 && !aConfig.if_range.empty()) headers.push_back("If-Range: " + aConfig.if_range);

    if (!headers.empty()) {
        m_pHeaders = {[this, &headers]() {
            curl_slist *headerlist = nullptr;

            for (const auto &header : headers) {
                if (header.find_first_of("\r\n") != std::string::npos || header.find('\0') != std::string::npos) {
                    curl_slist_free_all(headerlist);

                    throw http::exception("jfc::http: request header contains a line break or "
                        "a null, which would inject headers the caller did not write: \""
                        + header + "\"");
                }

                headerlist = curl_slist_append(headerlist, header.c_str());
            }

            curl_easy_setopt(m_pHandle.get(), CURLOPT_HTTPHEADER, headerlist);

            return headerlist;
        }()
        , [](curl_slist *p) {
            curl_slist_free_all(p);
        }};
    }

    curl_easy_setopt(m_pHandle.get(), CURLOPT_TIMEOUT_MS, static_cast<long>(aConfig.timeout_milliseconds));
    curl_easy_setopt(m_pHandle.get(), CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(aConfig.connect_timeout_milliseconds));

    if (const long stallSeconds = _to_low_speed_seconds(aConfig.stall_timeout_milliseconds)) {
        curl_easy_setopt(m_pHandle.get(), CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(m_pHandle.get(), CURLOPT_LOW_SPEED_TIME, stallSeconds);
    }

    if (aConfig.follow_redirects) {
        curl_easy_setopt(m_pHandle.get(), CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt(m_pHandle.get(), CURLOPT_MAXREDIRS,
            static_cast<long>(aConfig.maximum_redirects));

        curl_easy_setopt(m_pHandle.get(), CURLOPT_UNRESTRICTED_AUTH, 0L);
    }

    if (m_ResumeFromByte > 0) {
        curl_easy_setopt(m_pHandle.get(), CURLOPT_RESUME_FROM_LARGE,
            static_cast<curl_off_t>(m_ResumeFromByte));
    }

    if (m_bFailOnHTTPError) curl_easy_setopt(m_pHandle.get(), CURLOPT_FAILONERROR, 1L);

    curl_easy_setopt(m_pHandle.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_pHandle.get(), CURLOPT_SSL_VERIFYHOST, 2L);

#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(m_pHandle.get(), CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(m_pHandle.get(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(m_pHandle.get(), CURLOPT_PROTOCOLS,
        static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(m_pHandle.get(), CURLOPT_REDIR_PROTOCOLS,
        static_cast<long>(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
}

void http::curl_request::worker_on_enqueued_perform_extra_configuration(CURL *const) {}

void http::curl_request::worker_fetch_task() noexcept {
    m_bResponseLocked.test_and_set();
    m_HandlerException = nullptr;

    try {
        perform_transfer();
    }
    catch (...) {
        if (!m_HandlerException) m_HandlerException = std::current_exception();
        m_RequestError = http::error::handler_threw;
    }

    m_bResponseLocked.clear();
}

void http::curl_request::perform_transfer() {
    m_ResponseSink = {
        .pHandler = m_ResponseHandler.get(),
        .pHandle = m_pHandle.get(),
        .resume_offset = m_ResumeFromByte,
        .pCancelled = &m_bCancelled
    };

    if (m_bCancelled.load(std::memory_order_acquire)) {
        m_RequestError = http::error::cancelled;
        return;
    }

    curl_easy_setopt(m_pHandle.get(), CURLOPT_WRITEDATA, static_cast<void *>(&m_ResponseSink));
    curl_easy_setopt(m_pHandle.get(), CURLOPT_HEADERDATA, static_cast<void *>(&m_ResponseSink));
    curl_easy_setopt(m_pHandle.get(), CURLOPT_XFERINFODATA, static_cast<void *>(&m_ResponseSink));

    worker_on_enqueued_perform_extra_configuration(m_pHandle.get());

    const auto error = curl_easy_perform(m_pHandle.get());

    http::report_response_status(m_ResponseSink);

    if (m_ResponseSink.pException) {
        m_HandlerException = m_ResponseSink.pException;
        m_RequestError = http::error::handler_threw;
        return;
    }

    if (m_bCancelled.load(std::memory_order_acquire)) {
        m_RequestError = http::error::cancelled;
        return;
    }

    m_RequestError = m_ResponseSink.aborted
        ? http::error::aborted_by_handler
        : _curlcode_to_error(error);

    if (m_RequestError == http::error::none && m_ResumeFromByte > 0 && m_ResponseSink.status != 206) {
        m_RequestError = http::error::range_not_satisfied;
    }

    if (m_RequestError == http::error::none) m_ResponseHandler->worker_on_complete();
}

bool http::curl_request::try_submit() {
    if (m_bInFlight) return false;

    auto pContext = m_pContext.lock();

    if (!pContext) return false;

    m_bCancelled.store(false, std::memory_order_release);

    m_bInFlight = true;

    try {
        pContext->submit(shared_from_this());
    }
    catch (...) {
        m_bInFlight = false;
        throw;
    }

    return true;
}

bool http::curl_request::main_try_claim() {
    return !m_bResponseLocked.test_and_set();
}

void http::curl_request::main_run_handlers() {
    m_bInFlight = false;

    if (m_HandlerException) {
        const auto pException = m_HandlerException;
        m_HandlerException = nullptr;
        std::rethrow_exception(pException);
    }

    if (m_RequestError == http::error::none) m_ResponseHandler->main_on_success();
    else m_ResponseHandler->main_on_failure(m_RequestError);
}

void http::curl_request::cancel() {
    m_bCancelled.store(true, std::memory_order_release);
}

bool http::curl_request::is_cancelled() const {
    return m_bCancelled.load(std::memory_order_acquire);
}

bool http::curl_request::is_in_flight() const {
    return m_bInFlight;
}
