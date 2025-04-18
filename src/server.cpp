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

// TODO: Move to common file
#define CHECK(exp, msg) assert((void(msg), !(exp)))

const int kMaxClients = 5;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    int rc;

    // Parse arguments
    const uint16_t port = atoi(argv[1]);
    CHECK(port == 0, "Invalid port");

    // Initialise UDP socket
    const int sockfdUDP = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(sockfdUDP < 0, "socket");

    // Initialise TCP socket
    const int sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockfdTCP < 0, "socket");

    // Initialise serverAddr
    sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    // Bind UDP
    rc = bind(sockfdUDP, (sockaddr *)&serverAddr, sizeof(sockaddr));
    CHECK(rc < 0, "bind");

    // Bind TCP
    rc = bind(sockfdTCP, (sockaddr *)&serverAddr, sizeof(sockaddr));
    CHECK(rc < 0, "bind");

    // Listen on TCP
    rc = listen(sockfdTCP, kMaxClients);
    CHECK(rc < 0, "listen");

    while (true) {

    }

    // Close sockets
    close(sockfdUDP);
    close(sockfdTCP);

    return 0;
}