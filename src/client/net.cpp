#include "net.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "rendezvous.grpc.pb.h"
#include "rendezvous.pb.h"
#include <grpcpp/support/status.h>
#include <grpcpp/grpcpp.h>

int Net::createUdpSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    int rv = bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    if(rv == -1)
        return -1;    

    timeval tv(0, 500'000);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    printf("Created Socket!\n");
    return fd;
}

sockaddr_in Net::resolveUdp(const char* host, const char* port) {
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

bool Net::registerUdp(const sockaddr_in& server, const std::string& token) {
    for(int i = 0; i < 10; i++){
        sendto(m_fd, token.data(), token.size(), 0, (sockaddr*)&server, sizeof(server));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        char buf[64];
        sockaddr from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(m_fd, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
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

bool Net::holePunch() {
    auto wait = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::string punch = "PUNCH";
    bool pa = false;
    uint quiet = 0;
    while (wait > std::chrono::steady_clock::now()) {
        if(!pa){
            sendto(m_fd, punch.data(), punch.size(), 0, (sockaddr*)&peer, sizeof(peer));
            quiet = 0;
        }
        char buf[64]{};
        sockaddr from{};
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(m_fd, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
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
                sendto(m_fd, punchack.data(), punchack.size(), 0, (sockaddr*)&peer, sizeof(peer));
                quiet = 0;
            } else if(data == "PUNCHACK"){
                pa = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    pa ? printf("Hole Punch Success!\n") : printf("Session Timed Out\n");
    m_oppTimeout = std::chrono::steady_clock().now();
    return pa;
}

bool Net::init(const Net::InitInfo& info, bool& start) {
    std::string ip = info.ip == "" ? "127.0.0.1" : info.ip;
    std::string tcp = info.tcp == "" ? "50051" : info.tcp;
    std::string udp = info.udp == "" ? "50052" : info.udp;

    printf("trying tcp on %s:%s\n",ip.c_str(), tcp.c_str());
    auto channel = grpc::CreateChannel((ip + ":" + tcp), grpc::InsecureChannelCredentials());
    auto stub = rendezvous::v1::Rendezvous::NewStub(channel);

    m_fd = createUdpSocket();
    if(m_fd == -1)
        throw std::runtime_error("failed to create udp socket");

    printf("trying udp on %s:%s\n",ip.c_str(), udp.c_str());
    sockaddr_in server = resolveUdp(ip.c_str(), udp.c_str());

    grpc::ClientContext joinCtx;
    rendezvous::v1::JoinReq joinReq;
    joinReq.set_version(info.version);
    joinReq.set_room(info.room.c_str());
    rendezvous::v1::JoinAck joinResp;

    grpc::Status JoinStatus = stub->Join(&joinCtx, joinReq, &joinResp);
    if(!JoinStatus.ok())
        throw std::runtime_error(JoinStatus.error_message().c_str());
    printf("token: ");
    for(unsigned char c : joinResp.token())
        printf("%02x", c);
    printf("\n");

    if (!registerUdp(server, joinResp.token()))
        throw std::runtime_error("Failed to Register udp!");

    grpc::ClientContext waitCtx;
    //std::stop_callback onStop(st, [&]{waitCtx.TryCancel();});
    rendezvous::v1::WaitReq waitReq;
    waitReq.set_token(joinResp.token());
    rendezvous::v1::PeerInfo peerInfo;

    grpc::Status waitStatus = stub->WaitPeer(&waitCtx, waitReq, &peerInfo);
    if(!waitStatus.ok())
        throw std::runtime_error(waitStatus.error_message().c_str());
    printf("peer ip: %s:%d\n", peerInfo.ip().c_str(), peerInfo.port());
    
    start = peerInfo.start();
    
    peerAddress(peerInfo);
    return holePunch();
}

void Net::sendHover(uint8_t pos) {
    m_keepAliveClock++;
    packet p{PacketType::pos, m_keepAliveClock, pos};
    sendto(m_fd, &p, sizeof(p), 0, (sockaddr*)&peer, sizeof(peer));
}

void Net::sendMove(uint8_t move, uint8_t seq) {
    packet p{PacketType::move, (uint32_t)seq, move};
    sendto(m_fd, &p, sizeof(p), 0, (sockaddr*)&peer, sizeof(peer));
}

Poll Net::poll(uint8_t& oppPos, uint8_t seq) {
    if (m_oppTimeout + std::chrono::seconds(m_timeOutDelaySeconds) < std::chrono::steady_clock().now())
        return Poll::NONE; // stopped receiving
    if (m_moving && (m_moveTimeout + std::chrono::milliseconds(m_moveTimeOutDelayMs)) < std::chrono::steady_clock().now()) {
        m_moving = false;
        return Poll::oppMoved;
    }

    char buf[64]{};
    sockaddr from{};
    socklen_t fromLen = sizeof(from);
    ssize_t n = recvfrom(m_fd, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);

    if(n < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK)
            return Poll::empty;
        printf("some error idk!");
        return Poll::err; // got an error
    }

    if(!verifyPacket(n, from))
        return Poll::empty; // not from peer
    
    m_oppTimeout = std::chrono::steady_clock().now();

    packet p;
    packet ack;
    memcpy(&p, buf, sizeof(p));

    switch (p.type) {
        case PacketType::pos :
            oppPos = p.pos;
            return Poll::empty;
        case PacketType::move :
            oppPos = p.pos;
            m_moving = true;
            m_moveTimeout = std::chrono::steady_clock().now();
            ack.type = PacketType::ack;
            ack.time = p.time;
            sendto(m_fd, &ack, sizeof(ack), 0, (sockaddr*)&peer, sizeof(peer));
            return Poll::empty;
        case PacketType::ack:
            if(p.time == (uint32_t)seq)
               return Poll::ack;
            else return Poll::empty;
    }
    return Poll::NONE; // cant happen
}

// checks size == sizeof(packet) and sender == peer
bool Net::verifyPacket(ssize_t size, sockaddr from) {
    if(size != sizeof(packet)) // check size
        return false;

    if(from.sa_family != AF_INET) // check sender
        return false;
    else if (((sockaddr_in*)&from)->sin_addr.s_addr != peer.sin_addr.s_addr)
        return false;

    return true;
}

Net::Net(){

}

Net::~Net(){
    close(m_fd);
}