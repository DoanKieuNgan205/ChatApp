#pragma once
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
    static bool checkCredentials(const std::string& request);
};







