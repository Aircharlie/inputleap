#include "inputleap/ServerDiscoveryResponder.h"

#include "base/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>

#if SYSAPI_WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace inputleap {
namespace {

constexpr const char* kDiscoveryRequest = "INPUTLEAP_DISCOVER_V1";
constexpr const char* kDiscoveryReplyPrefix = "INPUTLEAP_SERVER_V1";

std::string trim_ascii_whitespace(std::string value)
{
    const auto is_ws = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
        [&](unsigned char c) { return !is_ws(c); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
        [&](unsigned char c) { return !is_ws(c); }).base(), value.end());
    return value;
}

std::string sanitize_server_name(std::string value)
{
    std::replace_if(value.begin(), value.end(),
        [](char c) { return c == '\r' || c == '\n' || c == '\t'; }, ' ');
    return value;
}

} // namespace

ServerDiscoveryResponder::ServerDiscoveryResponder(std::string serverName, std::uint16_t serverPort) :
    server_name_(sanitize_server_name(std::move(serverName))),
    server_port_(serverPort)
{
}

ServerDiscoveryResponder::~ServerDiscoveryResponder()
{
    stop();
}

bool ServerDiscoveryResponder::start()
{
    if (running_) {
        return true;
    }

    stop_requested_ = false;
    if (!openSocket()) {
        return false;
    }

    thread_ = std::thread([this]() { run(); });
    running_ = true;

    LOG_DEBUG1("server discovery responder listening on UDP port %u for \"%s\"",
               static_cast<unsigned>(kDiscoveryPort), server_name_.c_str());
    return true;
}

void ServerDiscoveryResponder::stop()
{
    stop_requested_ = true;
    closeSocket();

    if (thread_.joinable()) {
        thread_.join();
    }

    running_ = false;
}

bool ServerDiscoveryResponder::isRunning() const
{
    return running_;
}

bool ServerDiscoveryResponder::openSocket()
{
    socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == kInvalidSocket) {
        LOG_WARN("failed to create UDP discovery socket");
        return false;
    }

    int reuse = 1;
    setsockopt(static_cast<decltype(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))>(socket_),
               SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

#if SYSAPI_WIN32
    DWORD timeout_ms = 500;
    setsockopt(static_cast<SOCKET>(socket_), SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(kDiscoveryPort);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    const int bind_result = ::bind(
        static_cast<decltype(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))>(socket_),
        reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr));
    if (bind_result != 0) {
        LOG_WARN("failed to bind UDP discovery socket on port %u",
                 static_cast<unsigned>(kDiscoveryPort));
        closeSocket();
        return false;
    }

    return true;
}

void ServerDiscoveryResponder::closeSocket()
{
    if (socket_ == kInvalidSocket) {
        return;
    }

#if SYSAPI_WIN32
    closesocket(static_cast<SOCKET>(socket_));
#else
    close(socket_);
#endif
    socket_ = kInvalidSocket;
}

std::string ServerDiscoveryResponder::buildResponse() const
{
    std::ostringstream stream;
    stream << kDiscoveryReplyPrefix << '\t' << server_name_ << '\t' << server_port_ << '\n';
    return stream.str();
}

void ServerDiscoveryResponder::run()
{
    const std::string response = buildResponse();
    std::array<char, 512> buffer{};

    while (!stop_requested_) {
        sockaddr_in remote_addr{};
#if SYSAPI_WIN32
        int remote_len = sizeof(remote_addr);
        const int bytes_received = recvfrom(static_cast<SOCKET>(socket_), buffer.data(),
                                            static_cast<int>(buffer.size()), 0,
                                            reinterpret_cast<sockaddr*>(&remote_addr),
                                            &remote_len);
        if (bytes_received == SOCKET_ERROR) {
            const auto error = WSAGetLastError();
            if (stop_requested_ || error == WSAETIMEDOUT || error == WSAEINTR || error == WSAENOTSOCK) {
                continue;
            }
            LOG_DEBUG("UDP discovery recvfrom failed: %d", static_cast<int>(error));
            continue;
        }
#else
        socklen_t remote_len = sizeof(remote_addr);
        const int bytes_received = static_cast<int>(recvfrom(socket_, buffer.data(), buffer.size(),
                                                             0, reinterpret_cast<sockaddr*>(&remote_addr),
                                                             &remote_len));
        if (bytes_received < 0) {
            if (stop_requested_ || errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == EBADF) {
                continue;
            }
            LOG_DEBUG("UDP discovery recvfrom failed: %d", errno);
            continue;
        }
#endif

        const std::string request = trim_ascii_whitespace(std::string(buffer.data(), bytes_received));
        if (request != kDiscoveryRequest) {
            continue;
        }

        char address_text[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &remote_addr.sin_addr, address_text, sizeof(address_text));
        LOG_DEBUG1("replying to UDP discovery request from %s", address_text);

        sendto(static_cast<decltype(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))>(socket_),
               response.data(), static_cast<int>(response.size()), 0,
               reinterpret_cast<const sockaddr*>(&remote_addr), remote_len);
    }

    running_ = false;
}

} // namespace inputleap
