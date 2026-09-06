// © Joseph Cameron - All Rights Reserved

#ifndef JFC_HTTP_TEST_SERVER_H
#define JFC_HTTP_TEST_SERVER_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <unistd.h>
#endif

namespace jfc::http::test
{
    class test_server final {
    public:
#ifdef _WIN32
        using socket_type = SOCKET;

        static constexpr socket_type INVALID = INVALID_SOCKET;

        static void close_socket(const socket_type aSocket) { ::closesocket(aSocket); }
#else
        using socket_type = int;

        static constexpr socket_type INVALID = -1;

        static void close_socket(const socket_type aSocket) { ::close(aSocket); }
#endif

        [[nodiscard]] std::string url() const {
            return "http://127.0.0.1:" + std::to_string(m_Port) + "/";
        }

        [[nodiscard]] bool listening() const { return m_Listener != INVALID; }

        struct response final {
            std::string status_line = "200 OK";
            std::vector<std::string> headers = {};
            std::string body = {};
            std::size_t body_delay_milliseconds = 0;
        };

        [[nodiscard]] std::size_t served() const { return m_Served.load(); }

        [[nodiscard]] std::size_t connections() const { return m_Connections.load(); }

        explicit test_server(std::vector<response> aResponses, const bool aKeepAlive = false)
        : m_Responses(std::move(aResponses))
        , m_bKeepAlive(aKeepAlive)
        {
            start();
        }

        test_server(const std::string &aStatusLine, const std::string &aBody)
        : m_Responses{response{aStatusLine, {}, aBody}}
        {
            start();
        }

    private:
        void start()
        {
#ifdef _WIN32
            WSADATA data;

            WSAStartup(MAKEWORD(2, 2), &data);
#endif
            m_Listener = ::socket(AF_INET, SOCK_STREAM, 0);

            if (m_Listener == INVALID) return;

            sockaddr_in address{};

            address.sin_family = AF_INET;
            address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
            address.sin_port = 0; 

            if (::bind(m_Listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
                || ::listen(m_Listener, 1) != 0)
            {
                close_socket(m_Listener);

                m_Listener = INVALID;

                return;
            }

            socklen_t length = sizeof(address);

            ::getsockname(m_Listener, reinterpret_cast<sockaddr *>(&address), &length);

            m_Port = ::ntohs(address.sin_port);

            m_Thread = std::thread([this]() { serve(); });
        }

    public:

        ~test_server() {
            m_bStopping = true;

            if (m_Thread.joinable()) m_Thread.join();

            if (m_Listener != INVALID) close_socket(m_Listener);
#ifdef _WIN32
            WSACleanup();
#endif
        }

        test_server(const test_server &) = delete;
        test_server &operator=(const test_server &) = delete;

    private:
        [[nodiscard]] std::string encode_headers(const response &aResponse) const
        {
            std::string encoded = "HTTP/1.1 " + aResponse.status_line + "\r\n";

            for (const auto &header : aResponse.headers) encoded += header + "\r\n";

            encoded += "Content-Length: " + std::to_string(aResponse.body.size()) + "\r\n";

            if (!m_bKeepAlive) encoded += "Connection: close\r\n";

            return encoded + "\r\n";
        }

        static constexpr long IDLE_TIMEOUT_MICROSECONDS = 400000;

        [[nodiscard]] bool read_request(const socket_type aConnection)
        {
            std::string request;

            char buffer[1024];

            while (request.find("\r\n\r\n") == std::string::npos && !m_bStopping)
            {
                if (!wait_readable(aConnection, IDLE_TIMEOUT_MICROSECONDS)) return false;

                const auto received = ::recv(aConnection, buffer, sizeof(buffer), 0);

                if (received <= 0) return false;

                request.append(buffer, static_cast<std::size_t>(received));
            }

            m_LastRequest = request;

            return !m_bStopping;
        }

#ifdef MSG_NOSIGNAL
        static constexpr int SEND_FLAGS = MSG_NOSIGNAL;
#else
        static constexpr int SEND_FLAGS = 0;
#endif

        static void suppress_sigpipe(const socket_type aConnection)
        {
#if defined(SO_NOSIGPIPE)
            const int on = 1;

            ::setsockopt(aConnection, SOL_SOCKET, SO_NOSIGPIPE,
                reinterpret_cast<const char *>(&on), sizeof(on));
#else
            static_cast<void>(aConnection);
#endif
        }

        [[nodiscard]] bool send_all(const socket_type aConnection, const std::string &aBytes)
        {
            std::size_t sent = 0;

            while (sent < aBytes.size())
            {
                const auto wrote = ::send(aConnection, aBytes.data() + sent,
                    static_cast<int>(aBytes.size() - sent), SEND_FLAGS);

                if (wrote <= 0) return false;

                sent += static_cast<std::size_t>(wrote);
            }

            return true;
        }

        [[nodiscard]] static bool wait_readable(const socket_type aSocket,
            const long aMicroseconds)
        {
            fd_set readable;

            FD_ZERO(&readable);
            FD_SET(aSocket, &readable);

            timeval timeout{};

            timeout.tv_usec = aMicroseconds % 1000000;
            timeout.tv_sec = aMicroseconds / 1000000;

            return ::select(static_cast<int>(aSocket) + 1, &readable, nullptr, nullptr, &timeout)
                > 0;
        }

        [[nodiscard]] socket_type wait_for_connection()
        {
            while (!m_bStopping)
            {
                if (wait_readable(m_Listener, 100000))
                    return ::accept(m_Listener, nullptr, nullptr);
            }

            return INVALID;
        }

        void serve()
        {
            std::size_t responseIndex = 0;

            while (responseIndex < m_Responses.size() && !m_bStopping)
            {
                const socket_type connection = wait_for_connection();

                if (connection == INVALID) return;

                ++m_Connections;

                suppress_sigpipe(connection);

                do
                {
                    if (!read_request(connection)) break;

                    const auto &next = m_Responses[responseIndex++];

                    if (!send_all(connection, encode_headers(next))) break;

                    for (std::size_t waited = 0;
                        waited < next.body_delay_milliseconds && !m_bStopping;
                        waited += 50)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }

                    if (!next.body.empty() && !send_all(connection, next.body)) break;

                    ++m_Served;
                }
                while (m_bKeepAlive && responseIndex < m_Responses.size() && !m_bStopping);

                close_socket(connection);
            }
        }

    public:
        [[nodiscard]] const std::string &last_request() const { return m_LastRequest; }

    private:
        std::vector<response> m_Responses;

        std::string m_LastRequest;

        bool m_bKeepAlive = false;

        std::atomic<std::size_t> m_Served{0};

        std::atomic<std::size_t> m_Connections{0};

        socket_type m_Listener = INVALID;

        unsigned short m_Port = 0;

        std::atomic<bool> m_bStopping{false};

        std::thread m_Thread;
    };
}

#endif
