#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <vector>
#include <string_view>

using namespace geode::prelude;

namespace {
#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
inline void closeSocket(socket_t s) {
    closesocket(s);
}

bool initSockets() {
    static bool s_initialized = false;
    if (!s_initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            s_initialized = true;
        } else {
            log::error("WSAStartup failed");
            return false;
        }
    }
    return s_initialized;
}
#else
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
inline void closeSocket(socket_t s) {
    close(s);
}

bool initSockets() {
    return true;
}
#endif

// Minimal OSC message builder:
// Constructs standard 4-byte null-padded OSC strings:
// Address string null-padded to 4 bytes, followed by typetag ',' null-padded to 4 bytes (",\0\0\0").
std::vector<uint8_t> makeOsc(std::string_view address) {
    std::vector<uint8_t> buf(address.begin(), address.end());
    buf.push_back('\0');
    while (buf.size() % 4 != 0) {
        buf.push_back('\0');
    }
    // OSC typetag ',' null-padded to 4 bytes
    buf.push_back(',');
    buf.push_back('\0');
    buf.push_back('\0');
    buf.push_back('\0');
    return buf;
}

// Lightweight non-blocking UDP OSC sender function targeting 127.0.0.1
void sendOscMessages(const std::vector<std::string_view>& addresses, int port) {
    if (!initSockets()) return;

    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCK) {
        log::error("Failed to create UDP socket");
        return;
    }

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(static_cast<uint16_t>(port));
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    for (const auto& address : addresses) {
        auto pkt = makeOsc(address);
        sendto(sock, reinterpret_cast<const char*>(pkt.data()), static_cast<int>(pkt.size()), 0,
               reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    }

    closeSocket(sock);
}

void sendReaperSync(int port) {
    // Packet 1: "/action/1016"  (REAPER action: Transport: Stop)
    // Packet 2: "/action/40042" (REAPER action: Transport: Go to start of project)
    // Packet 3: "/action/1007"  (REAPER action: Transport: Play)
    sendOscMessages({ "/action/1016", "/action/40042", "/action/1007" }, port);
}
} // namespace

class $modify(SyncPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();

        if (Mod::get()->getSettingValue<bool>("enabled")) {
            auto port = static_cast<int>(Mod::get()->getSettingValue<int64_t>("port"));
            sendReaperSync(port);
        }
    }
};

$on_mod(Loaded) {
    log::info("gmdsync loaded (v1.0.0)");
}
