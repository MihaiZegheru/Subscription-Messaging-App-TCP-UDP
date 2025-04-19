#include <netinet/in.h>
#include <cassert>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/tcp.h>

#define CHECK(exp, msg) assert((void(msg), !(exp)))

#ifdef DEBUG
    #include <fstream>
    std::ofstream fout("client_debug_log.txt");
    #define LOG_DEBUG(msg) fout << "DEBUG:: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) std::cout << msg << std::endl

const int kBuffLen = 256;
const int kMaxEventsNum = 100;

char buff[kBuffLen];

namespace subscriber {

/**
 * May modify reference parameter isRunning.
*/
void HandleSTDIN(int sockfd, bool &isRunning) {
    int rc;

    std::string input;
    std::cin >> input;

    if (input == "exit") {
        isRunning = false;
    } else if (input == "subscribe") {
        std::cin >> input;
        std::string message = "sub$" + input;
        rc = send(sockfd, message.c_str(), message.length(), 0);
        CHECK(rc < 0, "send");

        LOG_INFO("Subscribed to topic " << input);
    } else if (input == "unsubscribe") {
        std::cin >> input;
        std::string message = "uns$" + input;
        rc = send(sockfd, message.c_str(), message.length(), 0);
        CHECK(rc < 0, "send");

        LOG_INFO("Unsubscribed from topic " << input);
    } else {
        LOG_INFO("Command does not exist.\n");
    }
}

/**
 * May modify reference parameter isRunning.
*/
void HandleTCP(int sockfd, bool &isRunning) {
    int rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    LOG_DEBUG(std::string(buff, kBuffLen));

    if ((std::string)buff == "exit") {
        isRunning = false;
    } else {
        
    }
}

} // namespace subscriber

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0] << " <client_id> <server_ip> <server_port>\n";
        return 1;
    }

    int rc;

    // Parse arguments
    const char *clientID = argv[1];
    size_t clientIDLen = strlen(clientID);
    const char *serverIP = argv[2];
    const uint16_t port = atoi(argv[3]);
    CHECK(port == 0, "Invalid port");

    // Initialise TCP socket
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockfd < 0, "socket");

    // Disable Nagle
    int flag = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    CHECK(rc < 0, "setsockopt");

    // Initialise serverAddr
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    rc = inet_aton(serverIP, &serverAddr.sin_addr);
    CHECK(rc < 0, "inet_aton");

    // Connect
    rc = connect(sockfd, (sockaddr *)&serverAddr, sizeof(serverAddr));
    CHECK(rc < 0, "connect");

    // Send clientID to the server
    send(sockfd, clientID, clientIDLen, 0);

    // Initialise epoll
    int epollfd = epoll_create1(0);
    CHECK(epollfd < 0, "epoll_create1");

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    CHECK(rc < 0, "epoll_ctl");

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    CHECK(rc < 0, "epoll_ctl");

    bool isRunning = true;
    epoll_event events[kMaxEventsNum];

    while (isRunning) {
        memset(buff, 0, kBuffLen);

        // Wait for events
        int nfds = epoll_wait(epollfd, events, kMaxEventsNum, -1);
        CHECK(nfds < 0, "epoll_wait");

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == STDIN_FILENO) {
                subscriber::HandleSTDIN(sockfd, isRunning);
            } else if (fd == sockfd) {
                subscriber::HandleTCP(sockfd, isRunning);
            }
        }
    }

    close(sockfd);
    close(epollfd);

    return 0;
}
