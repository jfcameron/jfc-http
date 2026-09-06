// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_CURL_REQUEST_H
#define JFC_HTTP_CURL_REQUEST_H

#include <jfc/http/curl_context.h>
#include <jfc/http/curl_share.h>
#include <jfc/http/request.h>
#include <jfc/http/response_handler.h>
#include <jfc/http/types.h>

#include <atomic>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>

#define CURL_STATICLIB
#include <curl/curl.h>

namespace jfc::http {
    /// \brief what the write and progress callbacks are handed
    struct response_sink final {
        http::response_handler *pHandler = nullptr;     ///< brief the handler
        CURL *pHandle = nullptr;                        ///< the handle this transfer is running on
        std::size_t resume_offset = 0;                  ///< where in the resource this transfer began
        std::size_t received_bytes = 0;                 ///< how many bytes have been handed to the handler
        http::response_status_type status = 0;          ///< the status of the response currently being read
        bool status_reported = false;                   ///< whether any status line has been seen
        bool aborted = false;                           ///< set when the handler refused a chunk
        const std::atomic<bool> *pCancelled = nullptr;  ///< the request's cancel flag
        std::exception_ptr pException = nullptr;        ///< the first exception a handler threw, if one did
    };

    /// \brief record a handler's exception on the sink
    void capture_handler_exception(response_sink &aSink);

    /// \brief the header callback
    [[nodiscard]] std::size_t receive_response_header(char *const aContent,
        const std::size_t aItemSize,
        const std::size_t aItemCount,
        void *const aUserPointer);

    /// \brief read the status off a handle and tell the handler
    void report_response_status(response_sink &aSink);

    /// \brief the write callback: hands each arriving chunk to the handler
    [[nodiscard]] std::size_t write_response_chunk(char *const aContent,
        const std::size_t aItemSize,
        const std::size_t aItemCount,
        void *const aUserPointer);

    /// \brief the progress callback, in curl's `CURLOPT_XFERINFOFUNCTION` shape
    [[nodiscard]] int report_transfer_progress(void *const aUserPointer,
        const curl_off_t aDownloadTotal,
        const curl_off_t aDownloadNow,
        const curl_off_t aUploadTotal,
        const curl_off_t aUploadNow);

    /// \brief libcurl implementation of a request
    class curl_request : public std::enable_shared_from_this<curl_request> {
    public:
        /// \brief hand this request to the context's submitter
        [[nodiscard]] bool try_submit();

        /// \brief ask for the transfer to stop; safe to call from any thread at any time
        void cancel();

        /// \brief whether cancellation has been asked for and not yet cleared by a resubmit
        [[nodiscard]] bool is_cancelled() const;

        /// \brief fetch task run by worker threads
        void worker_fetch_task() noexcept;

        /// \brief claim this request for handling
        [[nodiscard]] bool main_try_claim();

        /// \brief run the handlers for a claimed request, on the main thread
        /// \exception ... whatever a worker callback threw, rethrown here
        void main_run_handlers();

        virtual ~curl_request() = default;

    protected:
        /// \brief the transfer itself
        void perform_transfer();

        /// \brief lets child types test whether this is currently in flight
        [[nodiscard]] bool is_in_flight() const;

        /// \brief concrete-request-type specific configuration
        virtual void worker_on_enqueued_perform_extra_configuration(CURL *const apCURL);

        /// \brief inits all work common to every request type
        /// \exception jfc::http::exception a curl handle could not be allocated
        curl_request(std::weak_ptr<http::curl_context> pContext,
            const std::string &aURL,
            const http::request_config &aConfig,
            http::response_handler_ptr_type &&aHandler);

    private:
        using curl_handle_ptr_type = std::shared_ptr<CURL>;

        using curl_linked_list_ptr_type = std::shared_ptr<curl_slist>;

        std::weak_ptr<http::curl_context> m_pContext;

        std::shared_ptr<http::curl_share> m_pShare;

        bool m_bInFlight = false;

        std::atomic<bool> m_bCancelled{false};

        std::atomic_flag m_bResponseLocked;

        curl_handle_ptr_type m_pHandle;

        curl_linked_list_ptr_type m_pHeaders;

        response_sink m_ResponseSink;

        std::size_t m_ResumeFromByte = 0;

        bool m_bFailOnHTTPError = false;

        http::error m_RequestError = http::error::none;

        std::exception_ptr m_HandlerException;

        http::response_handler_ptr_type m_ResponseHandler;
    };
}

#endif
