#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

class VoiceServer {
private:
    SOCKET udpSocket;
    sockaddr_in serverAddr;
    bool running;
    std::mutex mtx;
    std::unordered_map<std::string, sockaddr_in> clients;
    std::unordered_map<std::string, std::string> callPairs;

public:
    VoiceServer();
    bool start(int port);
    void stop();
    void registerClient(const std::string& username, const sockaddr_in& addr);
    void handleUnregister(const std::string& username);
    void setNotifyCallback(std::function<void(const std::string&, const std::string&)> callback);


private:
    void run();
    void handleCallRequest(const std::string& msg);
    void handleAcceptCall(const std::string& msg);
    void handleRejectCall(const std::string& msg);
    void handleEndCall(const std::string& msg); 
    void forwardVoice(const std::string& from, const std::string& to, const char* data, int len);
};
