#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "rendezvous.grpc.pb.h"
#include <grpcpp/grpcpp.h>

int main(){
    grpc::ClientContext context;
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    auto stub = rendezvous::v1::Rendezvous::NewStub(channel);
    rendezvous::v1::JoinReq req;

    req.set_version(1);
    req.set_room("test");

    rendezvous::v1::JoinAck ack;

    grpc::Status status = stub->Join(&context, req, &ack);
    if(!status.ok())
        throw std::runtime_error(status.error_message().c_str());

    return EXIT_SUCCESS;
}