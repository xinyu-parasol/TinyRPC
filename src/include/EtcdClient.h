#ifndef TINYRPC_ETCD_CLIENT_H
#define TINYRPC_ETCD_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

class EtcdClient {
public:
    EtcdClient(const std::string &etcd_ip, uint16_t etcd_port);

    // High-level service registration
    bool RegisterService(const std::string &key, const std::string &value, int ttl_sec = 10);
    bool UnregisterService(const std::string &key);
    std::string DiscoverService(const std::string &key);
    std::vector<std::string> ListInstances(const std::string &service_name);

    // Low-level etcd v3 API
    bool Put(const std::string &key, const std::string &value, int64_t lease_id = 0);
    std::string Get(const std::string &key);
    bool Delete(const std::string &key);
    std::vector<std::string> GetByPrefix(const std::string &prefix);

    int64_t GrantLease(int ttl_sec);
    bool KeepAliveLease(int64_t lease_id);
    bool RevokeLease(int64_t lease_id);

    // Heartbeat: keep all active leases alive
    void KeepAliveAll();

    static std::string Base64Encode(const std::string &data);
    static std::string Base64Decode(const std::string &data);

private:
    std::string base_url_;
    struct LeaseInfo {
        int64_t lease_id;
        int ttl_sec;
    };
    std::unordered_map<std::string, LeaseInfo> active_leases_;

    std::string HttpPost(const std::string &path, const std::string &json_body);
    std::string HttpPut(const std::string &path, const std::string &json_body);

    static std::string Uint64ToString(uint64_t v);
};

#endif
