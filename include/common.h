#ifndef COMMON_H__
#define COMMON_H__

#include <cassert>
#include <iostream>
#include <netdb.h>
#include <string>

#define CHECK(exp, msg) assert((void(msg), !(exp)))

#define LOG_INFO(msg) std::cout << msg << std::endl

#ifdef DEBUG
    #include <fstream>
    #ifndef LOG_FILE
        #define LOG_FILE "default.log"
    #endif
    extern std::ofstream _log_file;
    #define LOG_DEBUG(msg) _log_file << "DEBUG :: " << __FILE__ << ":" \
                                     << __LINE__ << " :: " << msg << std::endl
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

struct TopicMessage {
    in_addr ip;
    uint16_t port;
    std::string topic;
    int type;
    char content[kMaxContentLen];
};

int recv_all(int sockfd, void *buffer, size_t len);
int send_all(int sockfd, void *buffer, size_t len);

#endif // COMMON_H__
