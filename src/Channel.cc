#include "Channel.h"
#include "Controller.h"
#include "header.pb.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

Channel::Channel(const std::string &ip, uint16_t port)
    : m_ip(ip), m_port(port) {
}

void Channel::CallMethod(const google::protobuf::MethodDescriptor *method,
                         google::protobuf::RpcController *controller,
                         const google::protobuf::Message *request,
                         google::protobuf::Message *response,
                         google::protobuf::Closure *done) {

    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        controller->SetFailed("socket() error");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_ip.c_str(), &addr.sin_addr) <= 0) {
        controller->SetFailed("inet_pton() error");
        ::close(sockfd);
        return;
    }

    if (::connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        controller->SetFailed("connect() error");
        ::close(sockfd);
        return;
    }

    TinyRpc::RpcHeader header;
    header.set_service_name(method->service()->name());
    header.set_method_name(method->name());

    std::string args_str;
    if (!request->SerializeToString(&args_str)) {
        controller->SetFailed("serialize request error");
        ::close(sockfd);
        return;
    }
    header.set_args_size(args_str.size());

    std::string header_str;
    if (!header.SerializeToString(&header_str)) {
        controller->SetFailed("serialize header error");
        ::close(sockfd);
        return;
    }

    uint32_t header_size_net = htonl(header_str.size());
    if (::send(sockfd, &header_size_net, sizeof(header_size_net), 0) == -1) {
        controller->SetFailed("send header size error");
        ::close(sockfd);
        return;
    }
    if (::send(sockfd, header_str.data(), header_str.size(), 0) == -1) {
        controller->SetFailed("send header error");
        ::close(sockfd);
        return;
    }
    if (::send(sockfd, args_str.data(), args_str.size(), 0) == -1) {
        controller->SetFailed("send args error");
        ::close(sockfd);
        return;
    }

    uint32_t response_size_net;
    ssize_t n = ::recv(sockfd, &response_size_net, sizeof(response_size_net), MSG_WAITALL);
    if (n != sizeof(response_size_net)) {
        controller->SetFailed("recv response size error");
        ::close(sockfd);
        return;
    }
    uint32_t response_size = ntohl(response_size_net);

    std::string response_str(response_size, '\0');
    n = ::recv(sockfd, &response_str[0], response_size, MSG_WAITALL);
    if (n != static_cast<ssize_t>(response_size)) {
        controller->SetFailed("recv response data error");
        ::close(sockfd);
        return;
    }

    if (!response->ParseFromString(response_str)) {
        controller->SetFailed("deserialize response error");
        ::close(sockfd);
        return;
    }

    ::close(sockfd);
}
