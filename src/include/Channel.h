#ifndef TINYRPC_CHANNEL_H
#define TINYRPC_CHANNEL_H

#include <google/protobuf/service.h>
#include <string>

class EtcdClient;

class Channel : public google::protobuf::RpcChannel {
public:
    // Direct connection
    Channel(const std::string &ip, uint16_t port);
    // Service discovery via etcd
    Channel(const std::string &service_name, EtcdClient *etcd_client);
    ~Channel();

    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;
private:
    bool ResolveService();
    int connect();

    std::string m_ip;
    uint16_t m_port = 0;

    // Service discovery mode
    bool m_use_discovery = false;
    std::string m_service_name;
    EtcdClient *m_etcd_client = nullptr;
};

#endif
