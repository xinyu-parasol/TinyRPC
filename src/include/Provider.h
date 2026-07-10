#ifndef TINYRPC_PROVIDER_H
#define TINYRPC_PROVIDER_H

#include <google/protobuf/service.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <google/protobuf/descriptor.h>
#include <functional>
#include <string>
#include <unordered_map>

class Provider {
public:
    void NotifyService(google::protobuf::Service *service);
    void Run();
private:
    muduo::net::EventLoop event_loop;
    struct ServiceInfo {
        google::protobuf::Service *service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> method_map;
    };
    std::unordered_map<std::string, ServiceInfo> service_map;
    void OnConnection(const muduo::net::TcpConnectionPtr &conn);
    void OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp time);
};

#endif
