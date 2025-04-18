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

#ifdef DEBUG
    #define LOG_DEBUG(msg) std::cout << "DEBUG:: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) std::cout << msg << std::endl

const int kMaxClients = 5;
const int kBuffLen = 256;

char buff[kBuffLen];

namespace server {

std::unordered_map<std::string, bool> clients;
std::unordered_map<int, std::string> socketToClient;
std::multimap<std::string, std::string> topicToClients;

/**
 * May modify reference parameter isRunning.
*/
void HandleSTDIN(bool &isRunning) {
    std::string input;
    std::cin >> input;

    if (input == "exit") {
        isRunning = false;

        // TODO: Send exit message to clients

    } else {
        LOG_INFO("Command does not exist.\n");
    }
}

/**
 * May modify reference parameters fds and fdmax.
*/
void HandleAcceptTCP(int sockfdTCP, fd_set &fds, int &fdmax) {
    int rc;

    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    // Accept new connection
    int sockfd = accept(sockfdTCP, (sockaddr *)&clientAddr,
                                &clientLen);
    CHECK(sockfd < 0, "accept");

    // Receive message
    rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    // Update fd_set
    FD_SET(sockfd, &fds);
    fdmax = std::max(fdmax, sockfd);

    std::string clientID = (std::string)buff;

    if (clients.find(clientID) == clients.end()) {
        clients.insert({clientID, true});
        socketToClient.insert({sockfd, clientID});

        LOG_INFO("New client " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
    } else {
        
    }

    // TODO: Check if client is new or not.
}

void HandleTCP(int sockfd) {
    int rc;

    rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    std::string message = buff;
    std::string &clientID = socketToClient[sockfd];

    if (message == "exit") {
        LOG_INFO("Client " << clientID << " disconnected");
    } else if (message.substr(0, 3) == "sub") {
        message.erase(0, 4);
        topicToClients.insert({message, clientID});
        LOG_DEBUG("Client " << clientID << " subscribing to " << message);
    } else if (message.substr(0, 3) == "uns") {
        message.erase(0, 4);
        auto range = topicToClients.equal_range(message);
        for (auto it = range.first; it != range.second; it++) {
            if (it->second == clientID) {
                topicToClients.erase(it);
                break;
            }
        }
        LOG_DEBUG("Client " << clientID << " unsubscribing from " << message);
    } else {
        LOG_DEBUG("Server command unrecognised: " << message);
    }
}

} // namespace server

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

    // Initialise fd_set
    fd_set fds, tmpfds;

    FD_ZERO(&tmpfds);
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(sockfdUDP, &fds);
    FD_SET(sockfdTCP, &fds);

    int fdmax = std::max(sockfdUDP, sockfdTCP);

    bool isRunning = true;

    while (isRunning) {
        memset(buff, 0, kBuffLen);

        tmpfds = fds;

        // Wait until a fd is used
        rc = select(fdmax + 1, &tmpfds, NULL, NULL, NULL);
        CHECK(rc < 0, "select");

        LOG_DEBUG("Unblocked fd.");
        
        for (int fd = 0; fd <= fdmax; fd++) {
            if (!FD_ISSET(fd, &tmpfds)) {
                continue;
            }

            if (fd == STDIN_FILENO) {
                server::HandleSTDIN(isRunning);
            } else if (fd == sockfdUDP) {

            } else if (fd == sockfdTCP) {
                server::HandleAcceptTCP(sockfdTCP, fds, fdmax);
            } else {
                server::HandleTCP(fd);
            }
        }
    }

    // Close sockets
    close(sockfdUDP);
    close(sockfdTCP);

    return 0;
}