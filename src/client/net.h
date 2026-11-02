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
    Net();
    ~Net();

    bool init(const std::string& ip,
        const std::string& tcp,
        const std::string& udp,
        uint32_t version);

    void sendHover(uint8_t pos); // non-important game state info
    bool poll(uint8_t& opPos);
    bool sendMove(); // reliable important game move info
    bool pollMove();
    
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