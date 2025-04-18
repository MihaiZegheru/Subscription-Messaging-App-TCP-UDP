#include <netinet/in.h>
#include <cassert>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
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

const int kBuffLen = 256;

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
        std::string message = "sub/" + input;

        rc = send(sockfd, message.c_str(), message.length(), 0);
        CHECK(rc < 0, "send");

        LOG_INFO("Subscribed to topic " << input);
    } else if (input == "unsubscribe") {
        std::cin >> input;
        std::string message = "uns/" + input;

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
    int rc;

    rc = recv(sockfd, buff, kBuffLen, 0);
    CHECK(rc < 0, "recv");

    if ((std::string)buff == "exit") {
        isRunning = false;
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
    char *clientID = argv[1];
    size_t clientIDLen = strlen(argv[1]);
    char *serverIP = argv[2];
    const uint16_t port = atoi(argv[3]);
    CHECK(port == 0, "Invalid port");

    // Initialise TCP socket
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(sockfd < 0, "socket");

    // Initialise serverAddr
    sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    rc = inet_aton(serverIP, &serverAddr.sin_addr);
    CHECK(rc < 0, "inet_aton");

    // Connect
    rc = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    CHECK(rc < 0, "connect");

    // Send Client ID to the server
    send(sockfd, clientID, clientIDLen, 0);

    // Initialise fd_set
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(sockfd, &fds);

    int fdmax = sockfd;

    bool isRunning = true;

    while (isRunning) {
        memset(buff, 0, kBuffLen);

        // Wait until a fd is used
        rc = select(fdmax + 1, &fds, NULL, NULL, NULL);
        CHECK(rc < 0, "select");

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            subscriber::HandleSTDIN(sockfd, isRunning);
        } else if (FD_ISSET(sockfd, &fds)) {
            subscriber::HandleTCP(sockfd, isRunning);
        }
    }

    // Close socket
    close(sockfd);

    return 0;
}