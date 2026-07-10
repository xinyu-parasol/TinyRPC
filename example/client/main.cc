#include "Channel.h"
#include "Controller.h"
#include "Application.h"
#include "EtcdClient.h"
#include "user_service.h"

int main(int argc, char **argv) {
    Application::Init(argc, argv);

    EtcdClient etcd_client("127.0.0.1", 2379);
    Channel channel("demo.UserServiceRpc", &etcd_client);
    demo::UserServiceRpc_Stub stub(&channel);

    demo::LoginRequest request;
    request.set_name("admin");
    request.set_pwd("123456");

    for (int i = 0; i < 6; ++i) {
        demo::LoginResponse response;
        Controller controller;

        printf("\n--- Call %d ---\n", i + 1);
        stub.Login(&controller, &request, &response, nullptr);

        if (controller.Failed()) {
            printf("RPC call failed: %s\n", controller.ErrorText().c_str());
        } else {
            printf("Response: success=%d, errcode=%d, errmsg=%s\n",
                   response.success(), response.errcode(), response.errmsg().c_str());
        }
    }

    return 0;
}
