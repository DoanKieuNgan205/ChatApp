#include "RegisterController.h"
#include "DatabaseHelper.h"
#include <iostream>

extern DatabaseHelper db;

std::string RegisterController::handleRegister(const std::string& request) {
    
    auto extractValue = [&](const std::string& key) {
        size_t pos = request.find(key);
        if (pos == std::string::npos) return std::string("");
        pos = request.find(":", pos);
        if (pos == std::string::npos) return std::string("");
        size_t start = request.find("\"", pos);
        size_t end   = request.find("\"", start+1);
        if (start == std::string::npos || end == std::string::npos) return std::string("");
        return request.substr(start+1, end-start-1);
    };

    std::string username = extractValue("\"username\"");
    std::string password = extractValue("\"password\"");
    std::string email = extractValue("\"email\""); 

    if (username.empty() || password.empty()) {
        return R"({ "action": "register_response", "status": "fail", "message": "INVALID_INPUT" })";
    }
    if (!email.empty()) {
        if (email.find('@') == std::string::npos || email.find('.') == std::string::npos) {
            return R"({ "action": "register_response", "status": "fail", "message": "INVALID_EMAIL_FORMAT" })";
        }
    }
    if (!db.isConnected()) {
        return R"({ "action": "register_response", "status": "fail", "message": "DB_NOT_CONNECTED" })";
    }

    if (db.registerUser(username, password,email)) {
        return R"({ "action": "register_response", "status": "success", "message": "REGISTER_SUCCESS" })";
    } else {
        return R"({ "action": "register_response", "status": "fail", "message": "USERNAME_EXISTS" })";
    }
}



