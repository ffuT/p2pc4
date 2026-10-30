#include "net.h"
#include <cstdlib>

constexpr uint32_t version = (1 << 16) | (0 << 8) | 0; // 1.0.0

int main(int argc, char* argv[]) {
    std::string ip = "localhost";
    std::string tcp = "50051";
    std::string udp = "50052";
    for(int i = 1; i < argc; i+= 2) {
        auto arg = std::string(argv[i]);
        if(argc <= 2) arg = "-h";
        if (arg == "--ip") ip = argv[i+1];
        else if (arg == "--tcp") tcp = argv[i+1];
        else if (arg == "--udp") udp = argv[i+1];
        else {
            printf("    --ip string \n        ip to connect (default localhost)\n");
            printf("    --tcp int \n        tcp port (default 50051)\n");
            printf("    --upd int \n        ucp port (default 50052)\n");
            return EXIT_SUCCESS;
        }
    }

    Net net = Net();
    if(!net.init(ip, tcp, udp, version))
        return EXIT_FAILURE;
   
    // TODO UDP 5 (later): holePunch(fd, peer); then moves/acks/keepalives over the same fd
    // TODO UDP 6: close(fd) when the game is over
    return EXIT_SUCCESS;
}