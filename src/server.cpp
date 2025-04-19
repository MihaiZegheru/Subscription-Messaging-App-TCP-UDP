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
#include <regex>
#include <set>

#include "common.h"

#ifdef DEBUG
    #include <fstream>
#include <set>
    std::ofstream fout("log/server_debug_log.txt");
    #define LOG_DEBUG(msg) fout << "DEBUG:: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

const int kMaxClients = 5;

char buff[kBuffLen];
char auxBuff[kBuffLen];

namespace server {

const int kTopicLen = 50;

std::unordered_map<std::string, bool> clients;
std::unordered_map<int, std::string> socketToClient;
std::unordered_map<std::string, int> clientToSocket;
std::multimap<std::string, std::string> topicToClients;

namespace {

void PrettyPrintTopicToClients() {
    std::stringstream ss;
    std::string currentKey;
    ss << "\n";

    for (auto it = topicToClients.begin(); it != topicToClients.end(); ++it) {
        if (it->first != currentKey) {
            currentKey = it->first;
            ss << "+ " << currentKey << "\n";
        }
        ss << "  + " << it->second << "\n";
    }
    LOG_DEBUG(ss.str());
}

std::vector<std::string> split(const std::string& s, char delim = '/') {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, delim)) {
        parts.push_back(part);
    }
    return parts;
}

bool wildcardOverlap(const std::string& a, const std::string& b) {
    auto A = split(a);
    auto B = split(b);

    size_t i = 0, j = 0;
    while (i < A.size() && j < B.size()) {
        if (A[i] == "*" || B[j] == "*") {
            // '*' can consume one or more segments — optimistic overlap
            return true;
        }
        if (A[i] == "+" || B[j] == "+" || A[i] == B[j]) {
            ++i; ++j;
            continue;
        }
        return false;
    }

    // handle tail segments
    if ((i == A.size() && j == B.size()) ||
        (i < A.size() && A[i] == "*") ||
        (j < B.size() && B[j] == "*")) {
        return true;
    }

    return false;
}

std::string wildcardToRegex(const std::string& wildcard) {
    std::string regex = "^";
    for (size_t i = 0; i < wildcard.size(); ++i) {
        if (wildcard[i] == '*') {
            regex += ".*";
        } else if (wildcard[i] == '+') {
            regex += "[^/]+";
        } else {
            // escape regex special characters
            if (std::string(".^$|()[]{}\\").find(wildcard[i]) != std::string::npos) {
                regex += '\\';
            }
            regex += wildcard[i];
        }
    }
    regex += "$";
    return regex;
}

// Check if a topic matches any of the subscriptions
bool matchesTopic(const std::string& topic, const std::string& subscription) {
    std::string pattern = wildcardToRegex(subscription);
    std::regex re(pattern);
    return std::regex_match(topic, re);
}

} // namespace

/**
 * May modify reference parameter isRunning.
*/
void HandleSTDIN(bool &isRunning) {
    std::string input;
    std::cin >> input;

    if (input == "exit") {
        isRunning = false;

        std::string message = "ext";
        
        for (auto it = clientToSocket.begin(); it != clientToSocket.end(); it++) {
            if (clients[(*it).first] == false) {
                continue;
            }
            strcpy(buff, message.c_str());
            int rc = send_all((*it).second, buff, kBuffLen);
            CHECK(rc < 0, "send");

            LOG_INFO("Client " << (*it).first << " disconnected.");
        }
    } else {
        LOG_DEBUG("Command does not exist.\n");
    }
}

void HandleUDP(int sockfd) {
    int rc;

    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    rc = recvfrom(sockfd, auxBuff, kBuffLen, 0, (struct sockaddr *)&clientAddr,
                  &clientLen);
    CHECK(rc < 0, "recvfrom");

    size_t offset = 0;
    strcpy(buff + offset, "msg");
    offset += 3;
    memcpy(buff + offset, &clientAddr.sin_addr, sizeof(clientAddr.sin_addr));
    offset += sizeof(clientAddr.sin_addr);
    memcpy(buff + offset, &clientAddr.sin_port, sizeof(clientAddr.sin_port));
    offset += sizeof(clientAddr.sin_port);
    memcpy(buff + offset, auxBuff, kTopicLen + 1 + kMaxContentLen);
    offset += kTopicLen + 1 + kMaxContentLen;

    std::string topic = std::string(auxBuff, kTopicLen);
    topic.erase(topic.find_last_not_of('\0') + 1);

    std::set<std::string> alreadySent;

    LOG_DEBUG("Search for: " + topic);
    for (auto const& candidate : topicToClients) {
        if (!matchesTopic(topic, candidate.first)) {
            continue;
        }
        LOG_DEBUG("Matched with: " + candidate.first);
        auto range = topicToClients.equal_range(candidate.first);
        for (auto it = range.first; it != range.second; it++) {
            if (clients[(*it).second] == false ||
                alreadySent.find((*it).second) != alreadySent.end()) {
                continue;
            }
            rc = send_all(clientToSocket[(*it).second], buff, kBuffLen);
            alreadySent.insert((*it).second);
            CHECK(rc < 0, "send");
        }
    }
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

    // Disable Nagle
    int flag = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    CHECK(rc < 0, "setsockopt");

    // Receive message
    rc = recv_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "recv");

    std::string clientID = (std::string)buff;
    LOG_DEBUG("Received connection request.");

    if (clients.find(clientID) == clients.end()) {
        clients.insert({clientID, true});
        socketToClient.insert({sockfd, clientID});
        clientToSocket.insert({clientID, sockfd});

        // Add new socket to epoll
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
        CHECK(rc < 0, "epoll_ctl");

        LOG_INFO("New client " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
        LOG_DEBUG("New client " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
    } else if (clients[clientID] == false) {
        clients[clientID] = true;
        socketToClient.insert({sockfd, clientID});
        clientToSocket.insert({clientID, sockfd});

        // Add new socket to epoll
        epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
        CHECK(rc < 0, "epoll_ctl");

        LOG_INFO("New client " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
        LOG_DEBUG("Welcome back " << clientID << " connected from "
                 << inet_ntoa(clientAddr.sin_addr) << ":"
                 << htons(clientAddr.sin_port));
    } else {
        LOG_INFO("Client " << clientID << " already connected.");
        
        std::string message = "ext";
        strcpy(buff, message.c_str());
        int rc = send_all(sockfd, buff, kBuffLen);
        CHECK(rc < 0, "send");

        LOG_DEBUG("Client " << clientID << " already connected.");
    }
}

void HandleTCP(int sockfd, int epollfd) {
    int rc;

    rc = recv_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "recv");

    std::string message = buff;
    std::string &clientID = socketToClient[sockfd];

    if (message == "ext") {
        clients[clientID] = false;
        epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
        close(sockfd);

        LOG_INFO("Client " << clientID << " disconnected.");
        LOG_DEBUG("Client " << clientID << " disconnected.");
    } else if (message.substr(0, 3) == "sub") {
        message.erase(0, 4);
        topicToClients.insert({message, clientID});
        LOG_DEBUG("Client " << clientID << " subscribing to " << message);
        PrettyPrintTopicToClients();
    } else if (message.substr(0, 3) == "uns") {
        message.erase(0, 4);
        for (auto candidate = topicToClients.begin(); candidate != topicToClients.end(); ) {
            if (!wildcardOverlap(message, candidate->first)) {
                ++candidate;
                continue;
            }
            auto range = topicToClients.equal_range(candidate->first);
            for (auto it = range.first; it != range.second; ) {
                if (it->second == clientID) {
                    it = topicToClients.erase(it);
                } else {
                    ++it;
                }
            }
            candidate = range.second;
        }
        LOG_DEBUG("Client " << clientID << " unsubscribing from " << message);
        PrettyPrintTopicToClients();
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

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parse arguments
    const uint16_t port = atoi(argv[1]);
    CHECK(port == 0, "Invalid port");

    // Initialise UDP socket
    const int sockfdUDP = socket(PF_INET, SOCK_DGRAM, 0);
    CHECK(sockfdUDP < 0, "socket");

    // Initialise TCP socket
    const int sockfdTCP = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockfdTCP < 0, "socket");

    // Disable Nagle
    int flag = 1;
    rc = setsockopt(sockfdTCP, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
    CHECK(rc < 0, "setsockopt");

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
                server::HandleUDP(fd);
            } else if (fd == sockfdTCP) {
                server::HandleAcceptTCP(sockfdTCP, epollfd);
            } else {
                server::HandleTCP(fd, epollfd);
            }
        }
    }

    // Close sockets
    close(sockfdUDP);
    close(sockfdTCP);
    close(epollfd);

    return 0;
}
