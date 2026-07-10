#ifndef TINYRPC_CONTROLLER_H
#define TINYRPC_CONTROLLER_H

#include <google/protobuf/service.h>
#include <string>

class Controller : public google::protobuf::RpcController {
public:
    Controller();
    void Reset() override;
    bool Failed() const override;
    std::string ErrorText() const override;
    void SetFailed(const std::string &reason) override;
    void StartCancel() override {}
    bool IsCanceled() const override { return false; }
    void NotifyOnCancel(google::protobuf::Closure *callback) override {}
private:
    bool m_failed;
    std::string m_err_text;
};

#endif
