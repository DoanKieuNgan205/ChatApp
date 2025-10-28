#pragma once
#include "Library.h"
#include <string>

class LoginController {
public:
    // Xử lý yêu cầu login, trả về JSON response
    static std::string handleLogin(const std::string& request);
};

/*#pragma once
#include <string>

class LoginController {
public:
    // Xử lý yêu cầu login từ client (chuỗi JSON)
    static std::string handleLogin(const std::string& request);

    // Có thể mở rộng thêm sau (đăng ký, đổi mật khẩu,...)
    static std::string handleRegister(const std::string& request);
};*/



