#include "ConnectionPool.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>

ConnectionPool::ConnectionPool(size_t max_idle_per_instance)
    : m_max_idle(max_idle_per_instance) {
}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &entry : m_pools) {
        for (int fd : entry.second.idle) {
            ::close(fd);
        }
    }
    m_pools.clear();
}

int ConnectionPool::CreateNew(const std::string &ip, uint16_t port) {
    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) return -1;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        ::close(sockfd);
        return -1;
    }

    if (::connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        ::close(sockfd);
        return -1;
    }

    printf("[Pool] Created new connection to %s:%d (fd=%d)\n",
           ip.c_str(), port, sockfd);
    return sockfd;
}

int ConnectionPool::Borrow(const std::string &ip, uint16_t port) {
    std::string key = ip + ":" + std::to_string(port);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pools.find(key);
    if (it != m_pools.end() && !it->second.idle.empty()) {
        int sockfd = it->second.idle.back();
        it->second.idle.pop_back();
        printf("[Pool] Reuse connection to %s:%d (fd=%d)\n", ip.c_str(), port, sockfd);
        return sockfd;
    }

    return CreateNew(ip, port);
}

void ConnectionPool::Return(const std::string &ip, uint16_t port, int sockfd) {
    std::string key = ip + ":" + std::to_string(port);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto &pool = m_pools[key];
    if (pool.idle.size() < m_max_idle) {
        pool.idle.push_back(sockfd);
        printf("[Pool] Return connection to %s:%d (fd=%d) [idle=%zu]\n",
               ip.c_str(), port, sockfd, pool.idle.size());
    } else {
        ::close(sockfd);
        printf("[Pool] Pool full, close connection to %s:%d (fd=%d)\n",
               ip.c_str(), port, sockfd);
    }
}
