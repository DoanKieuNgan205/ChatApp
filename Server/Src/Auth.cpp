#include "Auth.h"
#include <iostream>
#include "DatabaseHelper.h"

//
extern DatabaseHelper db;

bool Auth::checkCredentials(const std::string& request) {
   
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

    std::cout << "[DEBUG] Username: " << username 
              << ", Password: " << password << std::endl;

    if (!db.isConnected()) {
        std::cerr << "[ERROR] Database chưa kết nối!\n";
        return false;
    }

    bool ok = db.checkLogin(username, password);
    std::cout << "[DEBUG][Auth] checkLogin result = " << (ok ? "true" : "false") << std::endl;
    return ok;
}



















