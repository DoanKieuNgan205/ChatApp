/*#include "Login.h"
#include <iostream>

std::string showLoginForm(SOCKET sock) {
    std::string username, password;
    std::cout << "Nhap username: ";
    std::cin >> username;
    std::cout << "Nhap password: ";
    std::cin >> password;

    // Chuẩn bị request
    std::string request = "{ \"username\": \"" + username + "\", \"password\": \"" + password + "\" }";
    send(sock, request.c_str(), request.size(), 0);

    char buf[256];
    int bytes = recv(sock, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) return "";
    buf[bytes] = '\0';

    std::string response(buf);
    if (response == "LOGIN_SUCCESS") {
        std::cout << "Dang nhap thanh cong!\n";
        return username;
    } else {
        std::cout << "Dang nhap that bai!\n";
        return "";
    }
}*/

#include "Login.h"
#include <iostream>

std::string showLoginForm(SOCKET sock) {
    std::string username, password;
    std::cout << "Nhap username: ";
    std::cin >> username;
    std::cout << "Nhap password: ";
    std::cin >> password;

    // Gửi request login với action rõ ràng
    std::string request = "{ \"action\": \"login\", \"username\": \"" + username +
                          "\", \"password\": \"" + password + "\" }";
    send(sock, request.c_str(), request.size(), 0);

    char buf[256];
    int bytes = recv(sock, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) {
        std::cerr << "[DEBUG] Server dong ket noi.\n";
        return "";
    }
    buf[bytes] = '\0';

    /*std::string response(buf);
    if (response.find("LOGIN_SUCCESS") != std::string::npos) {
        std::cout << "Dang nhap thanh cong!\n";
        return username;
    } else {
        std::cout << "Dang nhap that bai! (" << response << ")\n";
        return "";
    }*/

    // Hàm tách field từ JSON
    auto extractField = [&](const std::string& json, const std::string& key) {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return std::string("");
        pos = json.find(":", pos);
        if (pos == std::string::npos) return std::string("");
        pos = json.find("\"", pos);
        if (pos == std::string::npos) return std::string("");
        size_t end = json.find("\"", pos + 1);
        if (end == std::string::npos) return std::string("");
        return json.substr(pos + 1, end - pos - 1);
    };

    std::string response(buf);
    std::cout << "[DEBUG] Response from server: " << response << std::endl;

    // Parse JSON server trả về
    std::string status = extractField(response, "status");
    std::string message = extractField(response, "message");

    std::cout << "[DEBUG] status=" << status << ", message=" << message << std::endl;

    // Kiểm tra login
    if (status == "success" && message == "LOGIN_SUCCESS") {
        std::cout << "Dang nhap thanh cong!\n";
        return username;
    } else {
        std::cout << "Dang nhap that bai! (" << response << ")\n";
        return "";
    }
}













