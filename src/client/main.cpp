#include <cstdio>
#include <cstdlib>
#include <grpcpp/support/status.h>
#include <stdexcept>

#include "rendezvous.grpc.pb.h"
#include "rendezvous.pb.h"
#include <grpcpp/grpcpp.h>

constexpr uint32_t version = 1;

int main() {
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    auto stub = rendezvous::v1::Rendezvous::NewStub(channel);
    
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

    grpc::ClientContext waitCtx;
    rendezvous::v1::WaitReq waitReq;
    waitReq.set_token(joinResp.token());
    rendezvous::v1::PeerInfo peerInfo;

    grpc::Status waitStatus = stub->WaitPeer(&waitCtx, waitReq, &peerInfo);
    if(!waitStatus.ok())
        throw std::runtime_error(waitStatus.error_message().c_str());
    std::printf("peer ip: %s:%d", peerInfo.ip().c_str(), peerInfo.port());
    return EXIT_SUCCESS;
}