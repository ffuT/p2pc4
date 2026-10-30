#include "net.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "rendezvous.grpc.pb.h"
#include "rendezvous.pb.h"
#include <grpcpp/support/status.h>
#include <grpcpp/grpcpp.h>

void Net::createUdpSocket() {
    m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    int rv = bind(m_fd, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    if(rv == -1)
        throw std::runtime_error("failed to create udp socket");

    timeval tv(0, 500'000);
    setsockopt(m_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    printf("Created Socket!\n");
}

sockaddr_in resolveUdp(const char* host, const char* port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    int rc = getaddrinfo(host, port, &hints, &res);
    if(rc != 0){
        throw std::runtime_error(gai_strerror(rc));
    }
    sockaddr_in addr;
    std::memcpy(&addr, res->ai_addr, sizeof(addr));
    freeaddrinfo(res);
    printf("Resolved udp!\n");
    return addr;
}

bool registerUdp(int fd, const sockaddr_in& server, const std::string& token) {
    for(int i = 0; i < 10; i++){
        sendto(fd, token.data(), token.size(), 0, (sockaddr*)&server, sizeof(server));
        char buf[64];
        sockaddr from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if( n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            return false;
        }
        if(std::string(buf, n) == "OK"){
            printf("OK Received!\n");
            return true;
        }
    }
    return false;
}

void Net::peerAddress(const rendezvous::v1::PeerInfo& info) {
    peer.sin_family = AF_INET;
    peer.sin_port = htons(info.port());
    if (!inet_pton(AF_INET, info.ip().c_str(), &peer.sin_addr)){
        throw std::runtime_error("Failed to inet_pton?\n");
    }
}

bool holePunch(int fd, const sockaddr_in& peer) {
    auto wait = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::string punch = "PUNCH";
    bool pa = false;
    uint quiet = 0;
    while (wait > std::chrono::steady_clock::now()) {
        if(!pa){
            sendto(fd, punch.data(), punch.size(), 0, (sockaddr*)&peer, sizeof(peer));
            quiet = 0;
        }
        char buf[64]{};
        sockaddr from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if( n < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK){ // didnt receive any packet
                if(quiet++ >= 5)
                    break;
                continue;
            }
            return false;
        }
        if(from.sa_family != AF_INET)
            continue;
        if(((sockaddr_in*)&from)->sin_addr.s_addr == peer.sin_addr.s_addr){
            auto data = std::string(buf, n); 
            if(data == "PUNCH"){
                auto punchack = std::string("PUNCHACK");
                sendto(fd, punchack.data(), punchack.size(), 0, (sockaddr*)&peer, sizeof(peer));
                quiet = 0;
            } else if(data == "PUNCHACK"){
                pa = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    pa ? printf("Hole Punch Success!\n") : printf("Session Timed Out\n");
    return pa;
}

bool Net::init(const std::string& ip, const std::string& tcp, const std::string& udp, uint32_t version){
printf("trying tcp on %s:%s\n",ip.c_str(), tcp.c_str());
    auto channel = grpc::CreateChannel((ip + ":" + tcp), grpc::InsecureChannelCredentials());
    auto stub = rendezvous::v1::Rendezvous::NewStub(channel);

    createUdpSocket();

    printf("trying udp on %s:%s\n",ip.c_str(), udp.c_str());
    sockaddr_in server = resolveUdp(ip.c_str(), udp.c_str());

    grpc::ClientContext joinCtx;
    rendezvous::v1::JoinReq joinReq;
    joinReq.set_version(version);
    joinReq.set_room("test");
    rendezvous::v1::JoinAck joinResp;

    grpc::Status JoinStatus = stub->Join(&joinCtx, joinReq, &joinResp);
    if(!JoinStatus.ok())
        throw std::runtime_error(JoinStatus.error_message().c_str());
    std::printf("token: ");
    for(unsigned char c : joinResp.token())
        std::printf("%02x", c);
    std::printf("\n");

    if (!registerUdp(m_fd, server, joinResp.token()))
        throw std::runtime_error("Failed to Register udp!");

    grpc::ClientContext waitCtx;
    rendezvous::v1::WaitReq waitReq;
    waitReq.set_token(joinResp.token());
    rendezvous::v1::PeerInfo peerInfo;

    grpc::Status waitStatus = stub->WaitPeer(&waitCtx, waitReq, &peerInfo);
    if(!waitStatus.ok())
        throw std::runtime_error(waitStatus.error_message().c_str());
    std::printf("peer ip: %s:%d\n", peerInfo.ip().c_str(), peerInfo.port());

    peerAddress(peerInfo);
    holePunch(m_fd, peer);
    return true;
}

void Net::sendHover() {
    
}

void Net::sendMove() {

}

void Net::poll() {

}
