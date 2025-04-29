#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

#include <cstring>

#ifdef DEBUG
    std::ofstream _log_file(LOG_FILE, std::ios::trunc);
#endif

int recv_all(int sockfd, void *buffer, PacketLenParam len) {
  PacketLenParam bytes_received = 0;
  PacketLenParam bytes_remaining = sizeof(PacketLenParam);
  char *buff = (char *)buffer;
  int res;

  // Recv packet size
  while(bytes_remaining) {
    res = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    CHECK(res < 0, "recv");
    bytes_remaining -= res;
    bytes_received += res;
  }

  bytes_remaining = *(PacketLenParam *)buff;
  bytes_received = 0;

  while(bytes_remaining) {
    res = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    CHECK(res < 0, "recv");
    bytes_remaining -= res;
    bytes_received += res;
  }
  return bytes_received;
}

int send_all(int sockfd, void *buffer, PacketLenParam len) {
  PacketLenParam bytes_sent = 0;
  PacketLenParam bytes_remaining = sizeof(PacketLenParam);
  char *buff = (char *)buffer;
  int res;

  // Send packet size
  while(bytes_remaining) {
    res = send(sockfd, &len, bytes_remaining, 0);
    CHECK(res < 0, "recv");
    bytes_remaining -= res;
    bytes_sent += res;
  }

  bytes_remaining = len;
  bytes_sent = 0;

  while(bytes_remaining) {
    res = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    CHECK(res < 0, "send");
    bytes_remaining -= res;
    bytes_sent += res;
  }
  return bytes_sent;
}
