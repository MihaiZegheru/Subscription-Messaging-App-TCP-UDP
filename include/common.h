#ifndef COMMON_H__
#define COMMON_H__

#define CHECK(exp, msg) assert((void(msg), !(exp)))

#ifdef DEBUG
    #include <fstream>
    std::ofstream fout("log/client_debug_log.txt");
    #define LOG_DEBUG(msg) fout << "DEBUG:: " << msg << std::endl
#else
    #define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) std::cout << msg << std::endl

const int kBuffLen = 2000;
const int kMaxEventsNum = 100;

#endif // COMMON_H__