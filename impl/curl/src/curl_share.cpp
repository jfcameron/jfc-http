// © Joseph Cameron - All Rights Reserved

#include <jfc/http/curl_share.h>

#include <jfc/http/exception.h>

#include <cstddef>

using namespace jfc;

void http::curl_share::lock(CURL *, const curl_lock_data aData, curl_lock_access, void *apUser) {
    static_cast<http::curl_share *>(apUser)->m_Mutexes[static_cast<std::size_t>(aData)].lock();
}

void http::curl_share::unlock(CURL *, const curl_lock_data aData, void *apUser) {
    static_cast<http::curl_share *>(apUser)->m_Mutexes[static_cast<std::size_t>(aData)].unlock();
}

http::curl_share::curl_share()
: m_pHandle(curl_share_init())
{
    if (!m_pHandle) throw http::exception("jfc::http: could not create a curl share handle");

    curl_share_setopt(m_pHandle, CURLSHOPT_LOCKFUNC, http::curl_share::lock);
    curl_share_setopt(m_pHandle, CURLSHOPT_UNLOCKFUNC, http::curl_share::unlock);
    curl_share_setopt(m_pHandle, CURLSHOPT_USERDATA, this);

    curl_share_setopt(m_pHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(m_pHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    curl_share_setopt(m_pHandle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
}

http::curl_share::~curl_share() {
    if (m_pHandle) curl_share_cleanup(m_pHandle);
}
