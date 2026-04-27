#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace inputleap {

class ServerDiscoveryResponder {
public:
    static constexpr std::uint16_t kDiscoveryPort = 24801;

    ServerDiscoveryResponder(std::string serverName, std::uint16_t serverPort);
    ~ServerDiscoveryResponder();

    bool start();
    void stop();
    bool isRunning() const;

private:
    void run();
    bool openSocket();
    void closeSocket();
    std::string buildResponse() const;

private:
    std::string server_name_;
    std::uint16_t server_port_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> running_{false};
    std::thread thread_;

#if SYSAPI_WIN32
    using NativeSocket = std::uintptr_t;
    static constexpr NativeSocket kInvalidSocket = static_cast<NativeSocket>(~0ULL);
#else
    using NativeSocket = int;
    static constexpr NativeSocket kInvalidSocket = -1;
#endif

    NativeSocket socket_{kInvalidSocket};
};

} // namespace inputleap
