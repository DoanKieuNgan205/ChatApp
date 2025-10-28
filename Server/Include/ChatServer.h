#pragma once
#include "Library.h"
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include <iostream>
#include <algorithm>
#include <winsock2.h>


extern std::vector<SOCKET> clients;        // Danh sách client kết nối
extern std::map<std::string, SOCKET> userMap; // Map username -> socket
extern std::mutex mtx;                     // Dùng để tránh race condition


// Xử lý client kết nối
void handleClient(SOCKET client);

// Gửi tin nhắn đến tất cả client (trừ sender)
void broadcast(const std::string& msg, SOCKET sender);

// Gửi tin nhắn riêng cho 1 user
void sendToUser(const std::string& username, const std::string& msg);

// Hàm parse đơn giản lấy field từ JSON
std::string parseField(const std::string& json, const std::string& field);

/*#pragma once
#include <string>
#include <winsock2.h>
#include <map>
#include <mutex>
#include <vector>

class ChatServer {
public:
    // 🔹 Hàm xử lý logic từ JSON (được gọi bởi ServerCore)
    static std::string handleChat(const std::string& jsonRequest);

private:
    static void broadcast(const std::string& msg, SOCKET sender);
    static void sendToUser(const std::string& username, const std::string& msg);
};*/



