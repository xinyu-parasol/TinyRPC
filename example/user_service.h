#ifndef TINYRPC_EXAMPLE_USER_SERVICE_H
#define TINYRPC_EXAMPLE_USER_SERVICE_H

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>
#include "user.pb.h"

namespace demo {

// 手动实现 UserServiceRpc（因为当前 protoc 不生成 service 类）
// 正常 protobuf 会自动生成这个类，这里我们手动写一遍来学习
class UserServiceRpc : public google::protobuf::Service {
public:
    virtual void Login(::google::protobuf::RpcController *controller,
                       const ::demo::LoginRequest *request,
                       ::demo::LoginResponse *response,
                       ::google::protobuf::Closure *done) {
        (void)controller; (void)request; (void)response; (void)done;
    }

    const google::protobuf::ServiceDescriptor *GetDescriptor() override {
        return service_descriptor();
    }

    void CallMethod(const google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override {
        switch (method->index()) {
            case 0:
                Login(controller,
                      static_cast<const ::demo::LoginRequest *>(request),
                      static_cast<::demo::LoginResponse *>(response),
                      done);
                break;
            default:
                break;
        }
    }

    const google::protobuf::Message &GetRequestPrototype(
        const google::protobuf::MethodDescriptor *method) const override {
        (void)method;
        return ::demo::LoginRequest::default_instance();
    }

    const google::protobuf::Message &GetResponsePrototype(
        const google::protobuf::MethodDescriptor *method) const override {
        (void)method;
        return ::demo::LoginResponse::default_instance();
    }

    static const google::protobuf::ServiceDescriptor *service_descriptor() {
        static const google::protobuf::ServiceDescriptor *desc = nullptr;
        if (desc == nullptr) {
            google::protobuf::FileDescriptorProto file_proto;
            file_proto.set_name("demo_service.proto");
            file_proto.set_package("demo");
            file_proto.add_dependency("user.proto");

            google::protobuf::ServiceDescriptorProto *sp = file_proto.add_service();
            sp->set_name("UserServiceRpc");
            google::protobuf::MethodDescriptorProto *mp = sp->add_method();
            mp->set_name("Login");
            mp->set_input_type(".demo.LoginRequest");
            mp->set_output_type(".demo.LoginResponse");

            static google::protobuf::DescriptorPool pool(
                google::protobuf::DescriptorPool::generated_pool());
            const google::protobuf::FileDescriptor *fd = pool.BuildFile(file_proto);
            if (fd && fd->service_count() > 0) {
                desc = fd->service(0);
            }
        }
        return desc;
    }
};

// 客户端 Stub：通过 Channel 转发调用
class UserServiceRpc_Stub : public UserServiceRpc {
public:
    explicit UserServiceRpc_Stub(::google::protobuf::RpcChannel *channel)
        : channel_(channel) {}

    void Login(::google::protobuf::RpcController *controller,
               const ::demo::LoginRequest *request,
               ::demo::LoginResponse *response,
               ::google::protobuf::Closure *done) override {
        channel_->CallMethod(service_descriptor()->method(0), controller,
                             request, response, done);
    }

private:
    ::google::protobuf::RpcChannel *channel_;
};

}  // namespace demo

#endif
