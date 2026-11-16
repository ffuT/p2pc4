#pragma once
#include "rendezvous.pb.h"
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <stop_token>
#include <string>

#define printf(fmt, ...) ((void)0)

enum class Poll : int8_t {
    oppMoved,
    empty,
    update,
    ack,
    err, // error in networking
    NONE, // opp left
};

enum class PacketType : int8_t {
    pos,
    move,
    ack,
};

struct packet {
    PacketType type;
    uint32_t time;
    uint8_t pos;
};

class Net {
    public:
    struct InitInfo {
        const std::string& ip;
        const std::string& tcp;
        const std::string& udp;
        const std::string& room;
        uint32_t version;
    };

    Net();
    ~Net();

    bool init(const InitInfo& info, bool& start, std::stop_token& st);

    void sendHover(uint8_t pos); // player position + keep alive
    void sendMove(uint8_t move, uint8_t seq); // reliable important game move info
    Poll poll(uint8_t& oppPos, uint8_t seq);
    
    private:
    // initialization
    int createUdpSocket();
    bool registerUdp(const sockaddr_in& addr, const std::string& token);
    sockaddr_in resolveUdp(const char* host, const char* port);
    void peerAddress(const rendezvous::v1::PeerInfo&);
    bool holePunch();

    // runtime
    bool verifyPacket(ssize_t n, sockaddr from);

    // moves
    bool m_moving = false;
    const uint16_t m_moveTimeOutDelayMs = 200;
    std::chrono::time_point<std::chrono::steady_clock> m_moveTimeout;

    uint32_t m_keepAliveClock = 0;
    uint32_t m_oppTime = 0;
    std::chrono::time_point<std::chrono::steady_clock> m_oppTimeout;
    const uint8_t m_timeOutDelaySeconds = 5;
    int m_fd;
    sockaddr_in peer{};
};