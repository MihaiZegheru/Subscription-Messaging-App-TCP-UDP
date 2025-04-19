#ifndef COMMON_H__
#define COMMON_H__

#define CHECK(exp, msg) assert((void(msg), !(exp)))

#define LOG_INFO(msg) std::cout << msg << std::endl

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

#endif // COMMON_H__