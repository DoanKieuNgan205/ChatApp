#include "RegisterController.h"
#include "DatabaseHelper.h"
#include <iostream>

// DatabaseHelper được khai báo global ở main.cpp
extern DatabaseHelper db;

std::string RegisterController::handleRegister(const std::string& request) {
    // Parse JSON đơn giản
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

    if (username.empty() || password.empty()) {
        return R"({ "action": "register_response", "status": "fail", "message": "INVALID_INPUT" })";
    }

    if (!db.isConnected()) {
        return R"({ "action": "register_response", "status": "fail", "message": "DB_NOT_CONNECTED" })";
    }

    if (db.registerUser(username, password)) {
        return R"({ "action": "register_response", "status": "success", "message": "REGISTER_SUCCESS" })";
    } else {
        return R"({ "action": "register_response", "status": "fail", "message": "USERNAME_EXISTS" })";
    }
}

/*#include "RegisterController.h"
#include <iostream>

std::string RegisterController::handleRegister(const std::string& json) {
    std::cout << "[RegisterController] Received register request: " << json << "\n";
    return R"({"action":"register_ok","status":"success"})";
}*/

