#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <map>
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
#include <sys/epoll.h>

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
const int kMaxEventsNum = 100;

char buff[kBuffLen];

namespace server {

std::unordered_set<std::string> clients;
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

void HandleUDP() {

}

/**
 * May modify reference parameters epollfd and fdmax.
*/
void HandleAcceptTCP(int sockfdTCP, int epollfd) {
    int rc;

    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    // Accept new connection
    int sockfd = accept(sockfdTCP, (sockaddr *)&clientAddr, &clientLen);
    CHECK(sockfd < 0, "accept");

    // Receive message
    rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    // Add new socket to epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    CHECK(rc < 0, "epoll_ctl");

    std::string clientID = (std::string)buff;

    if (clients.find(clientID) == clients.end()) {
        clients.insert(clientID);
        socketToClient.insert({sockfd, clientID});

        LOG_INFO("New client " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
    } else {
        // Handle already existing client
    }
}

void HandleTCP(int sockfd) {
    int rc;

    rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    std::string message = buff;
    std::string &clientID = socketToClient[sockfd];

    if (message == "exit") {
        clients.erase(clientID);
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

    // Initialise epoll
    int epollfd = epoll_create1(0);
    CHECK(epollfd < 0, "epoll_create1");

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfdUDP;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfdUDP, &ev);
    CHECK(rc < 0, "epoll_ctl (UDP)");

    ev.data.fd = sockfdTCP;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfdTCP, &ev);
    CHECK(rc < 0, "epoll_ctl (TCP)");

    ev.data.fd = STDIN_FILENO;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    CHECK(rc < 0, "epoll_ctl (STDIN)");

    bool isRunning = true;
    epoll_event events[kMaxEventsNum];

    while (isRunning) {
        memset(buff, 0, kBuffLen);

        // Wait for events
        int nfds = epoll_wait(epollfd, events, kMaxEventsNum, -1);
        CHECK(nfds == -1, "epoll_wait");

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == STDIN_FILENO) {
                server::HandleSTDIN(isRunning);
            } else if (fd == sockfdUDP) {
                server::HandleUDP();
            } else if (fd == sockfdTCP) {
                server::HandleAcceptTCP(sockfdTCP, epollfd);
            } else {
                server::HandleTCP(fd);
            }
        }
    }

    // Close sockets
    close(sockfdUDP);
    close(sockfdTCP);
    close(epollfd);

    return 0;
}
