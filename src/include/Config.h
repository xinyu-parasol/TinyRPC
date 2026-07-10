#ifndef TINYRPC_CONFIG_H
#define TINYRPC_CONFIG_H

#include <unordered_map>
#include <string>

class Config {
public:
    void LoadConfigFile(const char *config_file);
    std::string Load(const std::string &key);
private:
    std::unordered_map<std::string, std::string> config_map;
    void Trim(std::string &buf);
};

#endif
