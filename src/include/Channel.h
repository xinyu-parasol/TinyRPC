#ifndef TINYRPC_CHANNEL_H
#define TINYRPC_CHANNEL_H

#include <google/protobuf/service.h>

class Channel : public google::protobuf::RpcChannel {
public:
    Channel(const std::string &ip, uint16_t port);
    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    google::protobuf::RpcController *controller,
                    const google::protobuf::Message *request,
                    google::protobuf::Message *response,
                    google::protobuf::Closure *done) override;
private:
    int connect();
    std::string m_ip;
    uint16_t m_port;
};

#endif
