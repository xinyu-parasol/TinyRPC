#include "Controller.h"

Controller::Controller()
    : m_failed(false)
    , m_err_text("")
{
}

void Controller::Reset() {
    m_failed = false;
    m_err_text = "";
}

bool Controller::Failed() const {
    return m_failed;
}

std::string Controller::ErrorText() const {
    return m_err_text;
}

void Controller::SetFailed(const std::string &reason) {
    m_failed = true;
    m_err_text = reason;
}
