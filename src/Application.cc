#include "Application.h"
#include <unistd.h>

Config Application::m_config;
Application *Application::m_instance = nullptr;
std::mutex Application::m_mutex;

void Application::Init(int argc, char **argv) {
    const char *config_file = nullptr;
    int opt;
    while ((opt = getopt(argc, argv, "i:")) != -1) {
        switch (opt) {
            case 'i':
                config_file = optarg;
                break;
            default:
                break;
        }
    }
    if (config_file != nullptr) {
        m_config.LoadConfigFile(config_file);
    }
}

Application &Application::GetInstance() {
    if (m_instance == nullptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_instance == nullptr) {
            m_instance = new Application();
        }
    }
    return *m_instance;
}

Config &Application::GetConfig() {
    return m_config;
}
