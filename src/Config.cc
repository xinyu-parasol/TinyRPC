#include "Config.h"
#include <fstream>
#include <sstream>

void Config::LoadConfigFile(const char *config_file) {
    std::ifstream infile(config_file);
    if (!infile.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(infile, line)) {
        Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        Trim(key);
        Trim(value);
        config_map[key] = value;
    }
    infile.close();
}

std::string Config::Load(const std::string &key) {
    auto it = config_map.find(key);
    if (it != config_map.end()) {
        return it->second;
    }
    return "";
}

void Config::Trim(std::string &buf) {
    size_t start = 0;
    while (start < buf.size() && isspace(buf[start])) {
        ++start;
    }
    size_t end = buf.size();
    while (end > start && isspace(buf[end - 1])) {
        --end;
    }
    buf = buf.substr(start, end - start);
}
