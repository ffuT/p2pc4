#pragma once
#include "rendezvous.pb.h"
#include <cstdint>
#include <netinet/in.h>
#include <string>

class Net {
    public:
    Net();
    ~Net();

    bool init(const std::string& ip,
        const std::string& tcp,
        const std::string& udp,
        uint32_t version);

    void sendHover(); // non-important game state info
    void sendMove(); // reliable important game move info
    void poll();
    
    private:
    int createUdpSocket();
    bool registerUdp(const sockaddr_in& addr, const std::string& token);
    sockaddr_in resolveUdp(const char* host, const char* port);
    void peerAddress(const rendezvous::v1::PeerInfo&);
    bool holePunch();

    int m_fd;
    sockaddr_in peer{};
};