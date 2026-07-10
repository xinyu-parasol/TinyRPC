#ifndef TINYRPC_APPLICATION_H
#define TINYRPC_APPLICATION_H

#include "Config.h"
#include <mutex>

class Application {
public:
    static void Init(int argc, char **argv);
    static Application &GetInstance();
    static Config &GetConfig();
private:
    static Config m_config;
    static Application *m_instance;
    static std::mutex m_mutex;
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application &operator=(const Application&) = delete;
};

#endif
