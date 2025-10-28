#include "Auth.h"
#include <iostream>
#include "DatabaseHelper.h"

// DatabaseHelper khai báo global trong main.cpp
extern DatabaseHelper db;

bool Auth::checkCredentials(const std::string& request) {
    // Hàm parse JSON thủ công
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

    // Dùng DatabaseHelper để kiểm tra
    if (!db.isConnected()) {
        std::cerr << "[ERROR] Database chưa kết nối!\n";
        return false;
    }

    //return db.checkLogin(username, password);
    bool ok = db.checkLogin(username, password);
    std::cout << "[DEBUG][Auth] checkLogin result = " << (ok ? "true" : "false") << std::endl;
    return ok;
}

/*#include "Auth.h"
#include "DatabaseHelper.h"
#include <iostream>

// DatabaseHelper được khai báo global trong main.cpp
extern DatabaseHelper db;

// 🧩 Hàm tách giá trị JSON thủ công (đơn giản, không cần thư viện)
static std::string extractValue(const std::string& json, const std::string& key) {
    size_t pos = json.find(key);
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    size_t start = json.find("\"", pos);
    size_t end   = json.find("\"", start + 1);
    if (start == std::string::npos || end == std::string::npos) return "";
    return json.substr(start + 1, end - start - 1);
}

// ===========================
// 🔹 Kiểm tra đăng nhập
// ===========================
bool Auth::checkCredentials(const std::string& request) {
    std::string username = extractValue(request, "\"username\"");
    std::string password = extractValue(request, "\"password\"");

    std::cout << "[Auth] Kiểm tra login: user=" << username 
              << " pass=" << password << std::endl;

    if (!db.isConnected()) {
        std::cerr << "[Auth] ❌ Database chưa kết nối!\n";
        return false;
    }

    bool ok = db.checkLogin(username, password);
    std::cout << "[Auth] Kết quả checkLogin = " << (ok ? "✅" : "❌") << std::endl;
    return ok;
}

// ===========================
// 🔹 Đăng ký tài khoản mới
// ===========================
bool Auth::registerAccount(const std::string& request) {
    std::string username = extractValue(request, "\"username\"");
    std::string password = extractValue(request, "\"password\"");

    std::cout << "[Auth] Đăng ký tài khoản: user=" << username 
              << " pass=" << password << std::endl;

    if (!db.isConnected()) {
        std::cerr << "[Auth] ❌ Database chưa kết nối!\n";
        return false;
    }

    // Kiểm tra tài khoản tồn tại chưa
    if (db.checkUserExists(username)) {
        std::cerr << "[Auth] ⚠️ Tài khoản đã tồn tại!\n";
        return false;
    }

    // Thêm tài khoản mới
    bool ok = db.addUser(username, password);
    std::cout << "[Auth] Kết quả đăng ký = " << (ok ? "✅" : "❌") << std::endl;
    return ok;
}*/

















