#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common.h"

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
    std::string output = ss.str();
    output.pop_back();
    LOG_DEBUG(std::move(output));
}

std::vector<std::string> SplitPattern(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, delim)) {
        parts.push_back(part);
    }
    return parts;
}

bool ArePatternsOverlapping(const std::string& a, const std::string& b) {
    auto A = SplitPattern(a, '/');
    auto B = SplitPattern(b, '/');

    size_t i = 0, j = 0;
    while (i < A.size() && j < B.size()) {
        if (A[i] == "*" || B[j] == "*") {
            return true;
        }
        if (A[i] == "+" || B[j] == "+" || A[i] == B[j]) {
            ++i; ++j;
            continue;
        }
        return false;
    }
    if ((i == A.size() && j == B.size()) ||
        (i < A.size() && A[i] == "*") ||
        (j < B.size() && B[j] == "*")) {
        return true;
    }
    return false;
}

std::string PatternToRegex(const std::string& pattern) {
    std::string regex = "^";
    for (size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '*') {
            regex += ".*";
        } else if (pattern[i] == '+') {
            regex += "[^/]+";
        } else {
            if (std::string(".^$|()[]{}\\").find(pattern[i]) != std::string::npos) {
                regex += '\\';
            }
            regex += pattern[i];
        }
    }
    regex += "$";
    return regex;
}

bool MatchesPattern(const std::string& s, const std::string& pattern) {
    std::string p = PatternToRegex(pattern);
    std::regex regx(p);
    return std::regex_match(s, regx);
}

std::string BuildMessageBuffer(char *src, char *dest, sockaddr_in addr,
                               PacketLenParam &outLen) {
    size_t offset = 0;
    strcpy(dest + offset, "msg");
    offset += 3;
    memcpy(dest + offset, &addr.sin_addr, sizeof(addr.sin_addr));
    offset += sizeof(addr.sin_addr);
    memcpy(dest + offset, &addr.sin_port, sizeof(addr.sin_port));
    offset += sizeof(addr.sin_port);
    memcpy(dest + offset, src, kTopicLen + 1 + kMaxContentLen);
    offset += kTopicLen + 1 + kMaxContentLen;
    outLen = offset;

    std::string topic = std::string(src, kTopicLen);
    topic.erase(topic.find_last_not_of('\0') + 1);
    return topic;
}

void HandleNewClient(std::string clientID, int sockfd, int epollfd,
                     sockaddr_in clientAddr) {
    clients.insert({clientID, true});
    socketToClient.insert({sockfd, clientID});
    clientToSocket.insert({clientID, sockfd});

    // Add new socket to epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    CHECK(rc < 0, "epoll_ctl");

    LOG_INFO("New client " << clientID << " connected from "
                << inet_ntoa(clientAddr.sin_addr) << ":"
                << htons(clientAddr.sin_port));
    LOG_DEBUG("New client " << clientID << " connected from "
                << inet_ntoa(clientAddr.sin_addr) << ":"
                << htons(clientAddr.sin_port));
}

void HandleReturningClient(std::string clientID, int sockfd, int epollfd,
                           sockaddr_in clientAddr) {
    clients[clientID] = true;
    socketToClient.insert({sockfd, clientID});
    clientToSocket.insert({clientID, sockfd});

    // Add new socket to epoll
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    int rc = epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);
    CHECK(rc < 0, "epoll_ctl");

    LOG_INFO("New client " << clientID << " connected from "
                << inet_ntoa(clientAddr.sin_addr) << ":"
                << htons(clientAddr.sin_port));
    LOG_DEBUG("Welcome back " << clientID << " connected from "
                << inet_ntoa(clientAddr.sin_addr) << ":"
                << htons(clientAddr.sin_port));
}

void HandleConnectionRefused(std::string clientID, int sockfd) {
    LOG_INFO("Client " << clientID << " already connected.");
    LOG_DEBUG("Client " << clientID << " already connected.");
    
    std::string message = "ext";
    strcpy(buff, message.c_str());
    int rc = send_all(sockfd, buff, message.length());
    CHECK(rc < 0, "send");
}

void AuthClient(int sockfd, int epollfd, sockaddr_in clientAddr) {
    std::string clientID = (std::string)buff;
    LOG_DEBUG("Received connection request.");

    if (clients.find(clientID) == clients.end()) {
        HandleNewClient(std::move(clientID), sockfd, epollfd, clientAddr);
    } else if (clients[clientID] == false) {
        HandleReturningClient(std::move(clientID), sockfd, epollfd, clientAddr);
    } else {
        HandleConnectionRefused(std::move(clientID), sockfd);
    }
}

void HandleRecvExit(std::string clientID, int sockfd, int epollfd) {
    clients[clientID] = false;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
    close(sockfd);
    LOG_INFO("Client " << clientID << " disconnected.");
    LOG_DEBUG("Client " << clientID << " disconnected.");
}

void HandleRecvSubscribe(std::string message, std::string clientID) {
    message.erase(0, 4);
    topicToClients.insert({message, clientID});
    LOG_DEBUG("Client " << clientID << " subscribing to " << message);
    PrettyPrintTopicToClients();
}

void HandleRecvUnsubscribe(std::string message, std::string clientID) {
    message.erase(0, 4);
    for (auto candidate = topicToClients.begin();
            candidate != topicToClients.end(); ) {
        if (!ArePatternsOverlapping(message, candidate->first)) {
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
}

void SendTopicMessage(char *src, PacketLenParam len, std::string topic) {
    std::set<std::string> alreadySent;
    for (auto const& candidate : topicToClients) {
        if (!MatchesPattern(topic, candidate.first)) {
            continue;
        }
        auto range = topicToClients.equal_range(candidate.first);
        for (auto it = range.first; it != range.second; it++) {
            if (clients[(*it).second] == false ||
                alreadySent.find((*it).second) != alreadySent.end()) {
                continue;
            }
            int rc = send_all(clientToSocket[(*it).second], src, len);
            alreadySent.insert((*it).second);
            CHECK(rc < 0, "send");
        }
    }
}

void CloseServer() {
    std::string message = "ext";
    for (auto it = clientToSocket.begin(); it != clientToSocket.end(); it++) {
        if (clients[(*it).first] == false) {
            continue;
        }
        strcpy(buff, message.c_str());
        int rc = send_all((*it).second, buff, message.length());
        CHECK(rc < 0, "send");
        LOG_INFO("Client " << (*it).first << " disconnected.");
    }
}

} // namespace

bool HandleSTDIN() {
    std::string input;
    std::cin >> input;

    if (input == "exit") {
        CloseServer();
        return false;
    } else {
        LOG_DEBUG("Command does not exist.\n");
    }
    return true;
}

void HandleUDP(int sockfd) {
    int rc;
    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    rc = recvfrom(sockfd, auxBuff, kBuffLen, 0, (struct sockaddr *)&clientAddr,
                  &clientLen);
    CHECK(rc < 0, "recvfrom");

    PacketLenParam len;
    std::string topic = BuildMessageBuffer(auxBuff, buff, clientAddr, len);
    SendTopicMessage(buff, len, std::move(topic));
}

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

    AuthClient(sockfd, epollfd, clientAddr);
}

void HandleTCP(int sockfd, int epollfd) {
    int rc;
    rc = recv_all(sockfd, buff, kBuffLen);
    CHECK(rc < 0, "recv");

    std::string message = buff;
    std::string clientID = socketToClient[sockfd];

    if (message == "ext") {
        HandleRecvExit(std::move(clientID), sockfd, epollfd);
    } else if (message.substr(0, 3) == "sub") {
        HandleRecvSubscribe(std::move(message), std::move(clientID));
    } else if (message.substr(0, 3) == "uns") {
        HandleRecvUnsubscribe(std::move(message), std::move(clientID));
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
                isRunning = server::HandleSTDIN();
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
