#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

#include "rendezvous.grpc.pb.h"
#include "rendezvous.pb.h"
#include <grpcpp/support/status.h>
#include <grpcpp/grpcpp.h>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0

int createUdpSocket() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(0);
    int rv = bind(fd, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    if(rv == -1)
        return rv;

    timeval tv(0, 500'000);
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    printf("Created Socket!\n");
    return fd;
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
            if(errno == EAGAIN || errno == EWOULDBLOCK) // eh what? 
                continue;
            return false;
        }
        if(std::string(buf,n) == "OK"){
            printf("OK Received!\n");
            return true;
        }
    }
    return false;
}

sockaddr_in peerAddress(const rendezvous::v1::PeerInfo& info) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(info.port());
    if (!inet_pton(AF_INET, info.ip().c_str(), &addr.sin_addr)){
        throw std::runtime_error("Failed to inet_pton?\n");
    }
    return addr;
}

// LATER STEP: the hole punch. Same socket fd as registerUdp!
// TODO (outline):
//  1. for ~10 s: sendto(peer, "PUNCH") every ~200 ms, and recvfrom() in between.
//  2. received "PUNCH" from the peer -> answer "PUNCH_ACK" and you are connected.
//     received "PUNCH_ACK"          -> you are connected.
//  3. check the packet came from the peer (compare address), ignore anything else.
//  4. timeout without any packet from the peer -> return false (relay fallback, much later).
bool holePunch(int fd, const sockaddr_in& peer) {
    return false;
}

int main() {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    auto stub = rendezvous::v1::Rendezvous::NewStub(channel);

    int fd = createUdpSocket();
    if(fd == -1){
        std::printf("Failure to Create Socket\n");
        return EXIT_FAILURE;
    }
    sockaddr_in server = resolveUdp("localhost", "50052");

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

    if (!registerUdp(fd, server, joinResp.token()))
        std::runtime_error("Failed to Register udp!");

    grpc::ClientContext waitCtx;
    rendezvous::v1::WaitReq waitReq;
    waitReq.set_token(joinResp.token());
    rendezvous::v1::PeerInfo peerInfo;

    grpc::Status waitStatus = stub->WaitPeer(&waitCtx, waitReq, &peerInfo);
    if(!waitStatus.ok())
        throw std::runtime_error(waitStatus.error_message().c_str());
    std::printf("peer ip: %s:%d\n", peerInfo.ip().c_str(), peerInfo.port());

    sockaddr_in peer = peerAddress(peerInfo);
    // TODO UDP 5 (later): holePunch(fd, peer); then moves/acks/keepalives over the same fd
    // TODO UDP 6: close(fd) when the game is over
    return EXIT_SUCCESS;
}