#include "Channel.h"
#include "Controller.h"
#include "EtcdClient.h"
#include "ConnectionPool.h"
#include "header.pb.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>

Channel::Channel(const std::string &ip, uint16_t port)
    : m_ip(ip), m_port(port) {
    m_pool = new ConnectionPool();
}

Channel::Channel(const std::string &service_name, EtcdClient *etcd_client)
    : m_use_discovery(true)
    , m_service_name(service_name)
    , m_etcd_client(etcd_client) {
    std::srand(std::time(nullptr));
    m_pool = new ConnectionPool();
}

Channel::~Channel() {
    delete m_pool;
}

bool Channel::ResolveAllInstances() {
    m_instances = m_etcd_client->ListInstances(m_service_name);
    if (m_instances.empty()) {
        fprintf(stderr, "[Channel] No instances found for: %s\n", m_service_name.c_str());
        return false;
    }

    printf("[Channel] Discovered %zu instances for %s\n",
           m_instances.size(), m_service_name.c_str());
    for (size_t i = 0; i < m_instances.size(); ++i) {
        printf("[Channel]   [%zu] %s\n", i, m_instances[i].c_str());
    }

    m_resolved = true;
    return true;
}

static bool ParseAddr(const std::string &addr, std::string &ip, uint16_t &port) {
    size_t colon = addr.find(':');
    if (colon == std::string::npos) return false;
    ip = addr.substr(0, colon);
    port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)));
    return true;
}

void Channel::CallMethod(const google::protobuf::MethodDescriptor *method,
                         google::protobuf::RpcController *controller,
                         const google::protobuf::Message *request,
                         google::protobuf::Message *response,
                         google::protobuf::Closure *done) {

    // Service discovery: resolve all instances on first call
    if (m_use_discovery && !m_resolved) {
        if (!ResolveAllInstances()) {
            controller->SetFailed("service discovery failed");
            return;
        }
    }

    // Pick a random instance on every call (load balancing)
    if (m_use_discovery && m_resolved && !m_instances.empty()) {
        size_t idx = std::rand() % m_instances.size();
        if (!ParseAddr(m_instances[idx], m_ip, m_port)) {
            controller->SetFailed("parse address failed");
            return;
        }
        printf("[Channel] Selected instance [%zu] %s:%d\n", idx, m_ip.c_str(), m_port);
    }

    int sockfd = m_pool->Borrow(m_ip, m_port);
    if (sockfd == -1) {
        controller->SetFailed("borrow connection failed");
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

    m_pool->Return(m_ip, m_port, sockfd);

    if (!response->ParseFromString(response_str)) {
        controller->SetFailed("deserialize response error");
        return;
    }
}
