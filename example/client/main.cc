#include "Channel.h"
#include "Controller.h"
#include "Application.h"
#include "user_service.h"

int main(int argc, char **argv) {
    Application::Init(argc, argv);

    Channel channel("127.0.0.1", 10000);
    demo::UserServiceRpc_Stub stub(&channel);

    demo::LoginRequest request;
    request.set_name("admin");
    request.set_pwd("123456");

    demo::LoginResponse response;
    Controller controller;

    stub.Login(&controller, &request, &response, nullptr);

    if (controller.Failed()) {
        printf("RPC call failed: %s\n", controller.ErrorText().c_str());
    } else {
        printf("Response: success=%d, errcode=%d, errmsg=%s\n",
               response.success(), response.errcode(), response.errmsg().c_str());
    }

    return 0;
}
