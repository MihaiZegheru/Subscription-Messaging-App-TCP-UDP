#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <iterator>
#include <map>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <sys/types.h>
#include <sys/socket.h>

#define CHECK(exp, msg) assert((void(msg), !(exp)))

const int kMaxClients = 5;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int rc;

    // Parse the port
    const uint16_t port = atoi(argv[1]);
    CHECK(port == 0, "Invalid port");

    // Initialise UDP socket
    const int sockUDP = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(sockUDP < 0, "sockUDP");

    // Initialise TCP socket
    const int sockTCP = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockTCP < 0, "sockTCP");

    sockaddr_in serverAddr;
    
    // Bind UDP
    rc = bind(sockUDP, (sockaddr *)&serverAddr, sizeof(sockaddr));
    CHECK(rc < 0, "bind UDP");

    // Bind TCP
    rc = bind(sockTCP, (sockaddr *)&serverAddr, sizeof(sockaddr));
    CHECK(rc < 0, "bind TCP");

    // Listen over TCP
    rc = listen(sockTCP, kMaxClients);
    CHECK(rc < 0, "listen");

    while (true) {
        
    }

    // Close sockets
    close(sockUDP);
    close(sockTCP);

    return 0;
}