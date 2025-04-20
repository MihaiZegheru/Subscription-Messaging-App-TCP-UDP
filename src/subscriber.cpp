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
#include <cmath>

#include "common.h"

#ifdef DEBUG
    #include <fstream>
    std::ofstream fout("log/client_debug_log.txt");
    #define LOG_DEBUG(msg) fout << "DEBUG:: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

char buff[kBuffLen];

namespace subscriber {

namespace {

void SendExit(int sockfd) {
    std::string message = "ext";
    strcpy(buff, message.c_str());
    int rc = send_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "send");
}

void SendSubscribe(int sockfd, const std::string topic) {
    std::string message = "sub$" + topic;
    strcpy(buff, message.c_str());
    int rc = send_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "send");
    LOG_INFO("Subscribed to topic " << topic);
}

void SendUnsubscribe(int sockfd, const std::string topic) {
    std::string message = "uns$" + topic;
    strcpy(buff, message.c_str());
    int rc = send_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "send");
    LOG_INFO("Unsubscribed from topic " << topic);
}

void RecvBufferToPublishedMessage(char *src, PublishedMessage &message) {
    size_t offset = 0;
    offset += 3;
    memcpy(&message.ip, src + offset, sizeof(message.ip));
    offset += sizeof(message.ip);
    memcpy(&message.port, src + offset, sizeof(message.port));
    offset += sizeof(message.port);

    message.topic = std::string(src + offset, kTopicLen);
    message.topic.erase(message.topic.find_last_not_of('\0') + 1);
    offset += kTopicLen;
    message.type = src[offset];
    offset += 1;
    memcpy(message.content, src + offset, kMaxContentLen);
}

// TODO: Convert std::string type to std::stringview.
//       Change all const global strings.
void PrettyPrintMessage(const std::string topic,
                        const std::string type,
                        const std::string content) {
    std::stringstream ss;
    ss << topic << " - " << type << " - " << content;
    LOG_INFO(ss.str());
}

void PrettyPrintIntMessage(const PublishedMessage message) {
    uint32_t num;
    memcpy(&num, message.content + 1, sizeof(num));
    std::string content(std::to_string(
            (int64_t)ntohl(num) * (message.content[0] == 1 ? -1 : 1)));
    PrettyPrintMessage(std::move(message.topic),
                       std::move(kIntType),
                       std::move(content));
}

void PrettyPrintShortRealMessage(const PublishedMessage message) {
    uint16_t num;
    memcpy(&num, message.content, sizeof(num));
    std::stringstream ss;
    ss.precision(2);
    ss << std::fixed << (double)ntohs(num) / 100;
    std::string content = ss.str();
    PrettyPrintMessage(std::move(message.topic),
                       std::move(kShortRealType),
                       std::move(content));
}

void PrettyPrintFloatMessage(const PublishedMessage message) {
    uint32_t num;
    memcpy(&num, message.content + 1, sizeof(num));
    uint8_t exp;
    memcpy(&exp, message.content + 1 + sizeof(num), sizeof(exp));
    std::stringstream ss;
    ss.precision(10);
    ss << std::fixed
       << (double)ntohl(num) / std::pow(10, exp) * 
          (message.content[0] == 1 ? -1 : 1);
    std::string content = ss.str();
    PrettyPrintMessage(std::move(message.topic),
                       std::move(kFloatType),
                       std::move(content));
}

void PrettyPrintStringMessage(const PublishedMessage message) {
    std::string content = std::string(message.content, kMaxContentLen);     
    PrettyPrintMessage(std::move(message.topic),
                       std::move(kStringType),
                       std::move(content));
}

} // namespace

bool HandleSTDIN(int sockfd) {
    std::string input;
    std::cin >> input;

    if (input == "exit") {
        SendExit(sockfd);
        return false;
    } else if (input == "subscribe") {
        std::cin >> input;
        SendSubscribe(sockfd, std::move(input));
    } else if (input == "unsubscribe") {
        std::cin >> input;
        SendUnsubscribe(sockfd, std::move(input));
    } else {
        LOG_DEBUG("Command does not exist");
    }
    return true;
}

bool HandleTCP(int sockfd) {
    int rc = recv_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "recv");

    if (std::string(buff, 3) == "ext") {
        return false;
    } else if (std::string(buff, 3) == "msg") {
        PublishedMessage message;
        RecvBufferToPublishedMessage(buff, message);
        switch (message.type) {
            case 0:
                PrettyPrintIntMessage(std::move(message));
                break;
            case 1:
                PrettyPrintShortRealMessage(std::move(message));
                break;
            case 2:
                PrettyPrintFloatMessage(std::move(message));
                break;
            case 3:
                PrettyPrintStringMessage(std::move(message));
                break;
            default:
                LOG_DEBUG("Case " +
                          std::to_string(message.type) +
                          " not handled");
                break;
        }
    }
    return true;
}

} // namespace subscriber

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "Usage: " << argv[0]
                  << " <client_id> <server_ip> <server_port>\n";
        return 1;
    }

    int rc;

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parse arguments
    const char *clientID = argv[1];
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
    strcpy(buff, clientID);
    send_all(sockfd, buff, kBuffLen);

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
                isRunning = subscriber::HandleSTDIN(sockfd);
            } else if (fd == sockfd) {
                isRunning = subscriber::HandleTCP(sockfd);
            }
        }
    }

    close(sockfd);
    close(epollfd);

    return 0;
}
