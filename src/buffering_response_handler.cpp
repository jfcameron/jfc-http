// © Joseph Cameron - All Rights Reserved

#include <jfc/http/buffering_response_handler.h>

#include <utility>

using namespace jfc;

http::buffering_response_handler::buffering_response_handler(http::response_functor_type aOnSuccess,
    http::failure_functor_type aOnFailure,
    const http::buffering_config &aConfig)
: m_SuccessFunctor(std::move(aOnSuccess))
, m_FailureFunctor(std::move(aOnFailure))
, m_MaximumBytes(aConfig.maximum_bytes)
{}

void http::buffering_response_handler::worker_on_status(const http::response_status_type aStatus) {
    m_Status = aStatus;
}

bool http::buffering_response_handler::worker_on_data(http::response_chunk_type aChunk) {
    if (m_Buffer.size() + aChunk.size() > m_MaximumBytes) {
        m_bExceededLimit = true;
        return false;
    }

    m_Buffer.insert(m_Buffer.end(), aChunk.begin(), aChunk.end());

    return true;
}

void http::buffering_response_handler::main_on_success() {
    m_SuccessFunctor(m_Buffer);
}

void http::buffering_response_handler::main_on_failure(const http::error aError) {
    m_FailureFunctor(m_bExceededLimit ? http::error::response_too_large : aError);
}

const http::response_data_type &http::buffering_response_handler::response() const {
    return m_Buffer;
}

bool http::buffering_response_handler::exceeded_limit() const {
    return m_bExceededLimit;
}

std::optional<http::response_status_type> http::buffering_response_handler::status() const {
    return m_Status;
}
