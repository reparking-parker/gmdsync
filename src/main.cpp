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
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/StartPosObject.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/PlayerCheckpoint.hpp>

#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

using namespace geode::prelude;

namespace {
#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
inline void closeSocket(socket_t s) {
    closesocket(s);
}
#else
using socket_t = int;
constexpr socket_t INVALID_SOCK = -1;
inline void closeSocket(socket_t s) {
    close(s);
}
#endif

class OscClient {
public:
    static OscClient& get() {
        static OscClient instance;
        return instance;
    }

    ~OscClient() {
        close();
    }

    void ensureSocket(int port) {
        if (m_sock != INVALID_SOCK && m_currentPort == port) {
            return;
        }

        if (m_sock != INVALID_SOCK) {
            closeSocket(m_sock);
            m_sock = INVALID_SOCK;
        }

#ifdef _WIN32
        static bool s_wsa = false;
        if (!s_wsa) {
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
                s_wsa = true;
            } else {
                log::error("WSAStartup failed");
                return;
            }
        }
#endif

        m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (m_sock == INVALID_SOCK) {
            log::error("Failed to create UDP socket");
            return;
        }

#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(m_sock, FIONBIO, &mode);
#else
        int flags = fcntl(m_sock, F_GETFL, 0);
        if (flags != -1) {
            fcntl(m_sock, F_SETFL, flags | O_NONBLOCK);
        }
#endif

        m_currentPort = port;
        std::memset(&m_dest, 0, sizeof(m_dest));
        m_dest.sin_family = AF_INET;
        m_dest.sin_port = htons(static_cast<uint16_t>(port));
        m_dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }

    void sendPacket(const std::vector<uint8_t>& packet) {
        if (m_sock == INVALID_SOCK) return;
        sendto(m_sock, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const sockaddr*>(&m_dest), sizeof(m_dest));
    }

    void close() {
        if (m_sock != INVALID_SOCK) {
            closeSocket(m_sock);
            m_sock = INVALID_SOCK;
            m_currentPort = 0;
        }
    }

private:
    socket_t m_sock = INVALID_SOCK;
    int m_currentPort = 0;
    sockaddr_in m_dest{};
};

// String OSC message without arguments:
// Address null-padded to 4-byte boundary, followed by typetag ",\0\0\0" (4 bytes).
std::vector<uint8_t> makeOsc(std::string_view address) {
    std::vector<uint8_t> buf(address.begin(), address.end());
    buf.push_back('\0');
    while (buf.size() % 4 != 0) {
        buf.push_back('\0');
    }
    // Typetag "," null-padded to 4 bytes
    buf.push_back(',');
    buf.push_back('\0');
    buf.push_back('\0');
    buf.push_back('\0');
    return buf;
}

// Float OSC message with 1 float argument:
// Address null-padded to 4-byte boundary, typetag ",f\0\0", followed by big-endian 32-bit float.
std::vector<uint8_t> makeOscFloat(std::string_view address, float val) {
    std::vector<uint8_t> buf(address.begin(), address.end());
    buf.push_back('\0');
    while (buf.size() % 4 != 0) {
        buf.push_back('\0');
    }
    // Typetag ",f" null-padded to 4 bytes
    buf.push_back(',');
    buf.push_back('f');
    buf.push_back('\0');
    buf.push_back('\0');

    // Float to big-endian 32-bit
    uint32_t raw = 0;
    std::memcpy(&raw, &val, sizeof(float));
    uint32_t netVal = htonl(raw);
    const auto* p = reinterpret_cast<const uint8_t*>(&netVal);
    buf.insert(buf.end(), p, p + 4);
    return buf;
}

void sendOscCommand(std::string_view address) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    int port = static_cast<int>(Mod::get()->getSettingValue<int64_t>("port"));
    auto& client = OscClient::get();
    client.ensureSocket(port);
    client.sendPacket(makeOsc(address));
}

void sendOscFloatCommand(std::string_view address, float val) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    int port = static_cast<int>(Mod::get()->getSettingValue<int64_t>("port"));
    auto& client = OscClient::get();
    client.ensureSocket(port);
    client.sendPacket(makeOscFloat(address, val));
}

// Sets REAPER playrate multiplier
void applyPlayrate(std::optional<float> customSpeed = std::nullopt) {
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    float speed = customSpeed.value_or(static_cast<float>(Mod::get()->getSettingValue<double>("speed")));
    if (speed <= 0.01f) speed = 1.0f;
    sendOscFloatCommand("/playrate/raw", speed);
}

// Calculates section start time in seconds (for StartPos / Practice Checkpoints)
float getCalculatedStartTime(PlayLayer* pl) {
    if (!pl) return 0.0f;

    // A run is ONLY mid-level if using a StartPos or in practice mode with a checkpoint
    bool isMidLevel = (pl->m_startPosObject != nullptr) ||
                      (pl->m_isPracticeMode && pl->m_currentCheckpoint != nullptr);

    if (!isMidLevel) {
        return 0.0f;
    }

    // 1. For StartPos: calculate directly from the object's fixed level position
    if (pl->m_startPosObject) {
        float posTime = pl->timeForPos(pl->m_startPosObject->getPosition(), 0, 0, false, 0);
        if (posTime > 0.05f) {
            return posTime;
        }
        if (pl->m_startPosObject->m_startSettings && pl->m_startPosObject->m_startSettings->m_songOffset > 0.05f) {
            return pl->m_startPosObject->m_startSettings->m_songOffset;
        }
    }

    // 2. For Practice Mode checkpoint: calculate from checkpoint's player position or player1
    if (pl->m_currentCheckpoint && pl->m_currentCheckpoint->m_player1Checkpoint) {
        float posTime = pl->timeForPos(pl->m_currentCheckpoint->m_player1Checkpoint->m_position, 0, 0, false, 0);
        if (posTime > 0.05f) {
            return posTime;
        }
    }
    if (pl->m_player1) {
        float posTime = pl->timeForPos(pl->m_player1->getPosition(), 0, 0, false, 0);
        if (posTime > 0.05f) {
            return posTime;
        }
    }

    // 3. Fallback: FMOD music time
    if (auto fmod = FMODAudioEngine::sharedEngine()) {
        float fmodSec = fmod->getMusicTime(0);
        if (fmodSec > 0.05f) {
            return fmodSec;
        }
        float fmodMsSec = static_cast<float>(fmod->getMusicTimeMS(0)) / 1000.0f;
        if (fmodMsSec > 0.05f) {
            return fmodMsSec;
        }
    }

    return 0.0f;
}
} // namespace

class $modify(SyncPlayLayer, PlayLayer) {
    void syncPlayback() {
        if (!Mod::get()->getSettingValue<bool>("enabled")) return;

        applyPlayrate();

        float startSec = getCalculatedStartTime(this);
        double offsetMs = Mod::get()->getSettingValue<double>("audio-offset-ms");
        float targetSec = startSec + static_cast<float>(offsetMs / 1000.0);
        if (targetSec < 0.0f) {
            targetSec = 0.0f;
        }

        log::info("syncPlayback: seeking to {:.3f}s and playing", targetSec);
        sendOscFloatCommand("/time", targetSec);
        sendOscCommand("/action/1007"); // Transport: Play
    }

    void resetLevel() {
        PlayLayer::resetLevel();

        syncPlayback();
    }

    void resume() {
        PlayLayer::resume();

        if (Mod::get()->getSettingValue<bool>("enabled")) {
            log::info("resume: continuing playback");
            applyPlayrate();
            sendOscCommand("/action/1007"); // Transport: Play (resumes from current paused position)
        }
    }

    void onQuit() {
        if (Mod::get()->getSettingValue<bool>("enabled")) {
            log::info("onQuit: stopping playback and rewinding");
            sendOscCommand("/action/1016");  // Transport: Stop
            sendOscCommand("/action/40042"); // Transport: Go to start of project
        }

        PlayLayer::onQuit();
    }
};

class $modify(SyncPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        // 1. Pause playback without moving cursor
        if (Mod::get()->getSettingValue<bool>("enabled")) {
            log::info("PauseLayer: pausing playback");
            sendOscCommand("/action/1008"); // REAPER action: Transport: Pause
        }

        // 2. Settings button
        auto spr = CircleButtonSprite::create(
            nullptr, CircleBaseColor::Green, CircleBaseSize::Small
        );
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(SyncPauseLayer::onGmdSyncSettings)
        );
        btn->setID("gmdsync-settings-button"_spr);

        // Locate yellow options button in PauseLayer
        CCMenuItemSpriteExtra* optionsBtn = nullptr;
        if (auto b = this->getChildByID("settings-button")) {
            optionsBtn = typeinfo_cast<CCMenuItemSpriteExtra*>(b);
        }
        if (!optionsBtn) {
            if (auto rightMenu = this->getChildByID("right-button-menu")) {
                if (auto b = rightMenu->getChildByID("settings-button")) {
                    optionsBtn = typeinfo_cast<CCMenuItemSpriteExtra*>(b);
                }
            }
        }
        if (!optionsBtn) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            for (auto child : CCArrayExt<CCNode*>(this->getChildren())) {
                if (auto menu = typeinfo_cast<CCMenu*>(child)) {
                    for (auto item : CCArrayExt<CCNode*>(menu->getChildren())) {
                        if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(item)) {
                            auto worldPos = menu->convertToWorldSpace(b->getPosition());
                            if (worldPos.x > winSize.width - 80.f && worldPos.y > winSize.height - 80.f) {
                                optionsBtn = b;
                                break;
                            }
                        }
                    }
                    if (optionsBtn) break;
                }
            }
        }

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint targetWorldPos = (optionsBtn && optionsBtn->getParent())
            ? optionsBtn->getParent()->convertToWorldSpace(optionsBtn->getPosition()) - CCPoint{ 40.f, 0.f }
            : CCPoint{ winSize.width - 70.f, winSize.height - 25.f };

        auto myMenu = CCMenu::create();
        myMenu->setID("gmdsync-settings-menu"_spr);
        myMenu->setPosition(this->convertToNodeSpace(targetWorldPos));
        myMenu->addChild(btn);
        btn->setPosition({ 0.f, 0.f });
        this->addChild(myMenu);
    }

    void onGmdSyncSettings(CCObject*) {
        geode::openSettingsPopup(Mod::get());
    }
};

$on_mod(Loaded) {
    log::info("gmdsync loaded (v1.0.0)");
    listenForSettingChanges<double>("speed", [](double val) {
        applyPlayrate(static_cast<float>(val));
    });
}
