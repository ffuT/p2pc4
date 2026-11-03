#pragma once
#include "rendezvous.pb.h"
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <string>

#define printf(fmt, ...) ((void)0)

struct packet {
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

    bool init(const InitInfo& info, bool& start);

    void sendHover(uint8_t pos); // player position + keep alive
    bool poll(uint8_t& opPos);
    bool sendMove(uint8_t move); // reliable important game move info
    bool pollMove(uint8_t& move);
    
    private:
    int createUdpSocket();
    bool registerUdp(const sockaddr_in& addr, const std::string& token);
    sockaddr_in resolveUdp(const char* host, const char* port);
    void peerAddress(const rendezvous::v1::PeerInfo&);
    bool holePunch();

    uint32_t m_time = 0;
    uint32_t m_oppTime = 0;
    std::chrono::time_point<std::chrono::steady_clock> m_oppTimeout;
    int m_fd;
    sockaddr_in peer{};
};