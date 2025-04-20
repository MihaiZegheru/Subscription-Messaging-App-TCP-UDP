#ifndef COMMON_H__
#define COMMON_H__

#include <string>

#define CHECK(exp, msg) assert((void(msg), !(exp)))

#define LOG_INFO(msg) std::cout << msg << std::endl

#ifdef DEBUG
    #include <fstream>
    #ifndef LOG_FILE
        #define LOG_FILE "default.log"
    #endif
    std::ofstream _log_file(LOG_FILE, std::ios::app);
    #define LOG_DEBUG(msg) _log_file << "DEBUG :: " << __FILE__ << ":" << __LINE__ << " :: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

const int kBuffLen = 2000;
const int kMaxEventsNum = 100;

const int kTopicLen = 50;
const int kMaxContentLen = 1500;
const int kIntTypeLen = 40;
const int kShortRealLen = 16;
const int kFloatLen = 40;
const std::string kIntType = "INT";
const std::string kShortRealType = "SHORT_REAL";
const std::string kFloatType = "FLOAT";
const std::string kStringType = "STRING";

struct PublishedMessage {
    in_addr ip;
    uint16_t port;
    std::string topic;
    int type;
    char content[kMaxContentLen];
};

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;
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
  char *buff = (char *)buffer;
  int res;

  while(bytes_remaining) {
    res = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    CHECK(res < 0, "send");

    bytes_remaining -= res;
    bytes_sent += res;
  }

  return bytes_sent;
}
#endif // COMMON_H__