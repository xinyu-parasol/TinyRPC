#include "Provider.h"
#include "Application.h"
#include "EtcdClient.h"
#include "user_service.h"

class UserService : public demo::UserServiceRpc {
public:
    void Login(google::protobuf::RpcController *controller,
               const demo::LoginRequest *request,
               demo::LoginResponse *response,
               google::protobuf::Closure *done) override {
        (void)controller;
        (void)done;

    std::string name = request->name();
    std::string pwd = request->pwd();

    printf("[UserService:%s] Login called: name=%s, pwd=%s\n",
           Application::GetConfig().Load("rpc_server_port").c_str(),
           name.c_str(), pwd.c_str());

        if (name == "admin" && pwd == "123456") {
            response->set_success(true);
            response->set_errcode(0);
            response->set_errmsg("OK");
        } else {
            response->set_success(false);
            response->set_errcode(1);
            response->set_errmsg("Invalid username or password");
        }
    }
};

int main(int argc, char **argv) {
    Application::Init(argc, argv);

    EtcdClient etcd_client("127.0.0.1", 2379);
    UserService service;
    Provider provider;

    provider.SetEtcdClient(&etcd_client);
    provider.NotifyService(&service);

    provider.Run();
    return 0;
}
