#ifndef TINYRPC_LOGGER_H
#define TINYRPC_LOGGER_H

#include <cstdio>
#include <string>

class Logger {
public:
    static void Info(const std::string &msg) {
        printf("[TinyRpc] %s\n", msg.c_str());
    }
    static void Warning(const std::string &msg) {
        fprintf(stderr, "[TinyRpc WARNING] %s\n", msg.c_str());
    }
    static void Error(const std::string &msg) {
        fprintf(stderr, "[TinyRpc ERROR] %s\n", msg.c_str());
    }
};

#endif
