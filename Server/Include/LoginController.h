#pragma once
#include <string>
using namespace std;

class LoginController {
public:
    // Xử lý yêu cầu login, trả về JSON response
    static string handleLogin(const string& request);
};



