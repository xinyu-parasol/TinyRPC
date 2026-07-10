#include "Provider.h"
#include "Application.h"
#include "Controller.h"
#include "header.pb.h"
#include <muduo/base/Timestamp.h>
#include <google/protobuf/descriptor.h>
#include <arpa/inet.h>
#include <cstdio>

void Provider::NotifyService(google::protobuf::Service *service) {
    ServiceInfo service_info;
    service_info.service = service;

    const google::protobuf::ServiceDescriptor *desc = service->GetDescriptor();
    const std::string &service_name = desc->name();

    int method_cnt = desc->method_count();
    for (int i = 0; i < method_cnt; ++i) {
        const google::protobuf::MethodDescriptor *method = desc->method(i);
        service_info.method_map[method->name()] = method;
    }

    service_map[service_name] = service_info;
    printf("[TinyRpc] Registered service: %s\n", service_name.c_str());
}

void Provider::Run() {
    std::string ip = Application::GetConfig().Load("rpc_server_ip");
    uint16_t port = static_cast<uint16_t>(
        std::stoi(Application::GetConfig().Load("rpc_server_port"))
    );

    muduo::net::InetAddress address(ip, port);
    muduo::net::TcpServer server(&event_loop, address, "Provider");

    server.setConnectionCallback(
        std::bind(&Provider::OnConnection, this, std::placeholders::_1)
    );
    server.setMessageCallback(
        std::bind(&Provider::OnMessage, this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3)
    );

    setbuf(stdout, NULL);
    server.setThreadNum(4);
    printf("[TinyRpc] RPC server starting on %s:%d\n", ip.c_str(), port);
    server.start();
    event_loop.loop();
}

void Provider::OnConnection(const muduo::net::TcpConnectionPtr &conn) {
    if (!conn->connected()) {
        conn->shutdown();
    }
}

void Provider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                         muduo::net::Buffer *buf,
                         muduo::Timestamp) {
    while (buf->readableBytes() >= 4) {
        const char *data = buf->peek();
        uint32_t header_size = ntohl(*reinterpret_cast<const uint32_t *>(data));

        if (buf->readableBytes() < 4 + header_size) {
            return;
        }

        std::string header_str(data + 4, header_size);
        TinyRpc::RpcHeader header;
        if (!header.ParseFromString(header_str)) {
            fprintf(stderr, "[TinyRpc] Failed to parse RpcHeader\n");
            buf->retrieve(4 + header_size);
            continue;
        }

        uint32_t args_size = header.args_size();
        if (buf->readableBytes() < 4 + header_size + args_size) {
            return;
        }

        std::string service_name = header.service_name();
        std::string method_name = header.method_name();

        auto it = service_map.find(service_name);
        if (it == service_map.end()) {
            fprintf(stderr, "[TinyRpc] Service not found: %s\n", service_name.c_str());
            buf->retrieve(4 + header_size + args_size);
            continue;
        }

        ServiceInfo &service_info = it->second;
        auto method_it = service_info.method_map.find(method_name);
        if (method_it == service_info.method_map.end()) {
            fprintf(stderr, "[TinyRpc] Method not found: %s\n", method_name.c_str());
            buf->retrieve(4 + header_size + args_size);
            continue;
        }

        google::protobuf::Service *service = service_info.service;
        const google::protobuf::MethodDescriptor *method = method_it->second;

        google::protobuf::Message *request = service->GetRequestPrototype(method).New();
        google::protobuf::Message *response = service->GetResponsePrototype(method).New();

        std::string args_str(data + 4 + header_size, args_size);
        if (!request->ParseFromString(args_str)) {
            fprintf(stderr, "[TinyRpc] Failed to parse request args\n");
            delete request;
            delete response;
            buf->retrieve(4 + header_size + args_size);
            continue;
        }

        Controller controller;
        service->CallMethod(method, &controller, request, response, nullptr);

        if (controller.Failed()) {
            fprintf(stderr, "[TinyRpc] RPC call failed: %s\n", controller.ErrorText().c_str());
            delete request;
            delete response;
            buf->retrieve(4 + header_size + args_size);
            continue;
        }

        std::string response_str;
        if (!response->SerializeToString(&response_str)) {
            fprintf(stderr, "[TinyRpc] Failed to serialize response\n");
            delete request;
            delete response;
            buf->retrieve(4 + header_size + args_size);
            continue;
        }

        buf->retrieve(4 + header_size + args_size);

        uint32_t response_size_net = htonl(response_str.size());
        conn->send(&response_size_net, sizeof(response_size_net));
        conn->send(response_str);

        delete request;
        delete response;
    }
}
