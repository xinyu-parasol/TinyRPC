#ifndef TINYRPC_CONNECTION_POOL_H
#define TINYRPC_CONNECTION_POOL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

class ConnectionPool {
public:
    explicit ConnectionPool(size_t max_idle_per_instance = 8);
    ~ConnectionPool();

    int Borrow(const std::string &ip, uint16_t port);
    void Return(const std::string &ip, uint16_t port, int sockfd);

private:
    struct Pool {
        std::vector<int> idle;
    };

    size_t m_max_idle;
    std::mutex m_mutex;
    std::unordered_map<std::string, Pool> m_pools;

    int CreateNew(const std::string &ip, uint16_t port);
};

#endif
