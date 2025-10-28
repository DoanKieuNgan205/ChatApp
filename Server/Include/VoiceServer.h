#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class VoiceServer {
private:
    SOCKET udpSocket;
    sockaddr_in serverAddr;
    bool running;
    std::mutex mtx;

    // Danh sách client: username -> địa chỉ
    std::unordered_map<std::string, sockaddr_in> clients;

    // Danh sách các cặp đang gọi nhau
    std::unordered_map<std::string, std::string> callPairs;

public:
    VoiceServer();
    bool start(int port);
    void stop();

    // Đăng ký client mới
    void registerClient(const std::string& username, const sockaddr_in& addr);
    void handleUnregister(const std::string& username);

private:
    void run();

    // Gọi thoại
    void handleCallRequest(const std::string& msg);
    void handleAcceptCall(const std::string& msg);
    void handleRejectCall(const std::string& msg);

    // Chuyển dữ liệu âm thanh
    void forwardVoice(const std::string& from, const std::string& to, const char* data, int len);
};
