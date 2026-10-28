// © Joseph Cameron - MIT License

#include <jfc/http/impl_fetch_request.h>

#include <jfc/http/exception.h>

#include <algorithm>
#include <cstring>
#include <string_view>
#include <utility>

using namespace jfc;

namespace {
    [[nodiscard]] std::string_view _trim(std::string_view aText) {
        constexpr const char *WHITESPACE = " \t\r\n";

        const auto first = aText.find_first_not_of(WHITESPACE);

        if (first == std::string_view::npos) return {};

        return aText.substr(first, aText.find_last_not_of(WHITESPACE) - first + 1);
    }

    [[nodiscard]] std::shared_ptr<http::fetch_request> *_holder_of(emscripten_fetch_t *const apFetch) {
        return static_cast<std::shared_ptr<http::fetch_request> *>(apFetch->userData);
    }
}

http::fetch_request::fetch_request(std::weak_ptr<http::fetch_context> pContext, std::string_view aURL,
    const http::request_config &aConfig, http::response_handler_ptr_type &&aHandler)
: m_pContext(std::move(pContext))
, m_URL(aURL)
, m_Config(aConfig)
, m_ResponseHandler(std::move(aHandler)) {
    m_bResponseLocked.test_and_set();

    auto headers = m_Config.headers;

    if (m_Config.resume_from_byte > 0) {
        headers.push_back("Range: bytes=" + std::to_string(m_Config.resume_from_byte) + "-");

        if (!m_Config.if_range.empty()) headers.push_back("If-Range: " + m_Config.if_range);
    }

    for (const auto &header : headers) {
        if (header.find_first_of("\r\n") != std::string::npos || header.find('\0') != std::string::npos)
            throw http::exception("jfc::http: request header contains a line break or a null, which "
                "would inject headers the caller did not write: \"" + header + "\"");

        const auto separator = header.find(':');

        if (separator == std::string::npos)
            throw http::exception("jfc::http: request header is not a \"Name: value\" line: \"" + header + "\"");

        m_HeaderStrings.emplace_back(_trim(std::string_view(header).substr(0, separator)));
        m_HeaderStrings.emplace_back(_trim(std::string_view(header).substr(separator + 1)));
    }

    for (const auto &text : m_HeaderStrings) m_HeaderPointers.push_back(text.c_str());

    m_HeaderPointers.push_back(nullptr);
}

http::fetch_request::~fetch_request() {
    close_fetch();
}

void http::fetch_request::configure(emscripten_fetch_attr_t &aAttributes) {
    std::strcpy(aAttributes.requestMethod, "GET");
}

bool http::fetch_request::try_submit() {
    if (m_bInFlight) return false;

    auto pContext = m_pContext.lock();

    if (!pContext) return false;

    m_bCancelled.store(false, std::memory_order_release);
    m_bResponseLocked.test_and_set();
    m_HandlerException = nullptr;
    m_RequestError = http::error::none;
    m_Status = 0;

    m_ResponseHandler->main_on_submit();

    emscripten_fetch_attr_t attributes;
    emscripten_fetch_attr_init(&attributes);

    configure(attributes);

    attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attributes.timeoutMSecs = static_cast<unsigned long>(m_Config.timeout_milliseconds);
    attributes.requestHeaders = m_HeaderPointers.data();
    attributes.onsuccess = &fetch_request::on_success;
    attributes.onerror = &fetch_request::on_error;
    attributes.onprogress = &fetch_request::on_progress;

    attributes.userData = new std::shared_ptr<fetch_request>(shared_from_this());

    m_bInFlight = true;

    m_pFetch = emscripten_fetch(&attributes, m_URL.c_str());

    if (!m_pFetch) {
        delete static_cast<std::shared_ptr<fetch_request> *>(attributes.userData);

        m_bInFlight = false;

        return false;
    }

    pContext->submit(shared_from_this());

    return true;
}

void http::fetch_request::on_progress(emscripten_fetch_t *const apFetch) {
    if (!_holder_of(apFetch)) return;

    const auto pRequest = *_holder_of(apFetch);

    if (pRequest->is_cancelled()) return;

    const std::size_t received = static_cast<std::size_t>(apFetch->dataOffset + apFetch->numBytes);

    const std::optional<std::size_t> total = apFetch->totalBytes > 0
        ? std::optional<std::size_t>(static_cast<std::size_t>(apFetch->totalBytes))
        : std::nullopt;

    try {
        pRequest->m_ResponseHandler->worker_on_progress(received, total);
    }
    catch (...) {
        if (!pRequest->m_HandlerException) pRequest->m_HandlerException = std::current_exception();
    }
}

void http::fetch_request::on_success(emscripten_fetch_t *const apFetch) {
    if (!_holder_of(apFetch)) return;

    const auto pRequest = *_holder_of(apFetch);

    pRequest->main_finish(apFetch, true);
}

void http::fetch_request::on_error(emscripten_fetch_t *const apFetch) {
    if (!_holder_of(apFetch)) return;

    const auto pRequest = *_holder_of(apFetch);

    pRequest->main_finish(apFetch, false);
}

void http::fetch_request::main_finish(emscripten_fetch_t *const apFetch, const bool aSucceeded) {
    const auto pContext = m_pContext.lock();

    const auto pSelf = shared_from_this();

    const http::response_status_type status = apFetch->status;

    std::string headers;

    http::response_data_type body;

    if (pContext && !is_cancelled()) {
        headers.resize(emscripten_fetch_get_response_headers_length(apFetch) + 1, '\0');

        emscripten_fetch_get_response_headers(apFetch, headers.data(), headers.size());

        body.assign(apFetch->data, apFetch->data + apFetch->numBytes);
    }

    close_fetch();

    if (!pContext) return;

    struct task_state final {
        std::shared_ptr<http::fetch_request> request;
        bool ran = false;

        explicit task_state(std::shared_ptr<http::fetch_request> aRequest) : request(std::move(aRequest)) {}

        task_state(const task_state &) = delete;
        task_state &operator=(const task_state &) = delete;

        ~task_state() {
            if (ran) return;

            request->m_RequestError = http::error::cancelled;
            request->m_bResponseLocked.clear();
        }
    };

    const auto pState = std::make_shared<task_state>(pSelf);

    const auto task = [pState, status, aSucceeded, headers = std::move(headers), body = std::move(body)]() {
        pState->ran = true;

        auto &self = *pState->request;

        if (self.is_cancelled()) {
            self.m_RequestError = http::error::cancelled;
            self.m_bResponseLocked.clear();

            return;
        }

        self.m_Status = status;

        try {
            if (self.m_Status) self.m_ResponseHandler->worker_on_status(self.m_Status);

            std::string_view remaining(headers.c_str());

            while (!remaining.empty()) {
                const auto lineEnd = remaining.find('\n');
                const auto line = remaining.substr(0, lineEnd);

                remaining = lineEnd == std::string_view::npos ? std::string_view() : remaining.substr(lineEnd + 1);

                if (const auto separator = line.find(':'); separator != std::string_view::npos)
                    self.m_ResponseHandler->worker_on_header(_trim(line.substr(0, separator)),
                        _trim(line.substr(separator + 1)));
            }

            const bool httpError = self.m_Status >= 400;

            if (!aSucceeded && (self.m_Status == 0 || (httpError && self.m_Config.fail_on_http_error))) {
                self.m_RequestError = self.m_Status == 0 ? http::error::network_error : http::error::http_error;
            }
            else if (self.m_Config.resume_from_byte > 0 && self.m_Status != 206) {
                self.m_RequestError = http::error::range_not_satisfied;
            }
            else {
                if (!body.empty() && !self.m_ResponseHandler->worker_on_data(body))
                    self.m_RequestError = http::error::aborted_by_handler;
                else
                    self.m_ResponseHandler->worker_on_complete();
            }
        }
        catch (...) {
            if (!self.m_HandlerException) self.m_HandlerException = std::current_exception();

            self.m_RequestError = http::error::handler_threw;
        }

        if (self.m_HandlerException) self.m_RequestError = http::error::handler_threw;

        self.m_bResponseLocked.clear();
    };

    try {
        pContext->run_worker_half(task);
    }
    catch (...) {
        m_HandlerException = std::current_exception();
    }
}

bool http::fetch_request::main_try_claim() {
    return !m_bResponseLocked.test_and_set();
}

void http::fetch_request::main_abort_if_cancelled() {
    if (!m_bInFlight || !is_cancelled() || !m_pFetch) return;

    close_fetch();

    m_RequestError = http::error::cancelled;
    m_bResponseLocked.clear();
}

void http::fetch_request::main_abandon() {
    cancel();

    close_fetch();
}

void http::fetch_request::close_fetch() {
    if (!m_pFetch) return;

    auto *const pFetch = std::exchange(m_pFetch, nullptr);
    auto *const pHolder = _holder_of(pFetch);

    pFetch->userData = nullptr;

    emscripten_fetch_close(pFetch);

    delete pHolder;
}

void http::fetch_request::main_run_handlers() {
    m_bInFlight = false;

    close_fetch();

    if (m_HandlerException) {
        const auto pException = m_HandlerException;
        m_HandlerException = nullptr;
        std::rethrow_exception(pException);
    }

    if (m_RequestError == http::error::none) m_ResponseHandler->main_on_success();
    else m_ResponseHandler->main_on_failure(m_RequestError);
}

void http::fetch_request::cancel() {
    m_bCancelled.store(true, std::memory_order_release);
}

bool http::fetch_request::is_cancelled() const {
    return m_bCancelled.load(std::memory_order_acquire);
}

bool http::fetch_request::is_in_flight() const {
    return m_bInFlight;
}

http::fetch_get::fetch_get(std::weak_ptr<http::fetch_context> pContext, std::string_view aURL,
    const http::request_config &aConfig, http::response_handler_ptr_type &&aHandler)
: http::fetch_request(std::move(pContext), aURL, aConfig, std::move(aHandler))
{}

bool http::fetch_get::try_submit() { return fetch_request::try_submit(); }

void http::fetch_get::cancel() { fetch_request::cancel(); }

http::fetch_post::fetch_post(std::weak_ptr<http::fetch_context> pContext, std::string_view aURL,
    std::string_view aPostData, const http::request_config &aConfig,
    http::response_handler_ptr_type &&aHandler)
: http::fetch_request(std::move(pContext), aURL, aConfig, std::move(aHandler))
, m_PostData(aPostData)
{}

void http::fetch_post::configure(emscripten_fetch_attr_t &aAttributes) {
    std::strcpy(aAttributes.requestMethod, "POST");

    aAttributes.requestData = m_PostData.data();
    aAttributes.requestDataSize = m_PostData.size();
}

bool http::fetch_post::try_update_postdata(const std::string &aPostData) {
    if (is_in_flight()) return false;

    m_PostData = aPostData;

    return true;
}

bool http::fetch_post::try_submit() { return fetch_request::try_submit(); }

void http::fetch_post::cancel() { fetch_request::cancel(); }
