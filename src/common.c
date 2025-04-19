#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  int res;

  while(bytes_remaining) {
    res = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    CHECK(res < 0, "recv");

    bytes_remaining -= res;
    bytes_received += res;
  }

  return bytes_received;
}

int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  int res;

  while(bytes_remaining) {
    res = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    CHECK(res < 0, "send");

    bytes_remaining -= res;
    bytes_sent += res;
  }

  return bytes_sent;
}