#pragma once
#include "Library.h"
#include <string>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
using namespace std;

class Auth {
public:
    // Kiểm tra username/password trong JSON request
    static bool checkCredentials(const string& request);
};

/*#pragma once
#include <string>

class Auth {
public:
    // Kiểm tra thông tin đăng nhập
    static bool checkCredentials(const std::string& request);

    // Đăng ký tài khoản mới
    static bool registerAccount(const std::string& request);
};*/











