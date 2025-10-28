#include "VoiceServer.h"
#include <iostream>
#include <thread>
#include <cstring>

VoiceServer::VoiceServer() {
    running = false;
    udpSocket = INVALID_SOCKET;
}

bool VoiceServer::start(int port) {
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "❌ Không thể tạo UDP socket.\n";
        return false;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(udpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "❌ Lỗi bind cổng UDP: " << WSAGetLastError() << "\n";
        closesocket(udpSocket);
        return false;
    }

    running = true;
    std::thread(&VoiceServer::run, this).detach();

    std::cout << "✅ VoiceServer đang chạy trên UDP port " << port << "\n";
    return true;
}

void VoiceServer::stop() {
    running = false;
    if (udpSocket != INVALID_SOCKET) {
        closesocket(udpSocket);
        udpSocket = INVALID_SOCKET;
    }
}

void VoiceServer::run() {
    char buffer[4096];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (running) {
        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                                     (sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesReceived <= 0) continue;

        std::string msg(buffer, bytesReceived);

        if (msg.rfind("REGISTER:", 0) == 0) {
            std::string username = msg.substr(9);
            registerClient(username, clientAddr);
            std::cout << "🟢 " << username << " đã đăng ký voice.\n";
            continue;
        }

        // 🚀 THÊM KHỐI LỆNH NÀY
        if (msg.rfind("UNREGISTER:", 0) == 0) {
            std::string username = msg.substr(11);
            handleUnregister(username);
            continue;
        }

        if (msg.rfind("CALL:", 0) == 0) {
            handleCallRequest(msg);
            continue;
        }

        if (msg.rfind("ACCEPT_CALL:", 0) == 0) {
            handleAcceptCall(msg);
            continue;
        }

        if (msg.rfind("REJECT_CALL:", 0) == 0) {
            handleRejectCall(msg);
            continue;
        }

        // voice data: FROM:<user>|TO:<target>|<binary data>
        size_t p1 = msg.find("FROM:");
        size_t p2 = msg.find("|TO:");
        size_t p3 = msg.find("|", p2 + 4);

        if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos)
            continue;

        std::string from = msg.substr(p1 + 5, p2 - (p1 + 5));
        std::string to = msg.substr(p2 + 4, p3 - (p2 + 4));

        const char* audioData = buffer + p3 + 1;
        int audioLen = bytesReceived - (p3 + 1);

        forwardVoice(from, to, audioData, audioLen);
    }
}

void VoiceServer::registerClient(const std::string& username, const sockaddr_in& addr) {
    std::lock_guard<std::mutex> lock(mtx);
    clients[username] = addr;
}

// 🚀 THÊM HÀM MỚI NÀY
void VoiceServer::handleUnregister(const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);

    // Xóa khỏi danh sách client UDP
    clients.erase(username);

    // Kiểm tra xem họ có đang trong cuộc gọi không
    auto it = callPairs.find(username);
    if (it != callPairs.end()) {
        std::string partner = it->second;
        
        // Xóa cả hai khỏi cặp gọi
        callPairs.erase(username);
        callPairs.erase(partner);
        
        std::cout << "⏹️ " << username << " hủy đăng ký, tự động kết thúc cuộc gọi với " << partner << ".\n";
    } else {
        std::cout << "👻 " << username << " đã hủy đăng ký voice.\n";
    }
}

void VoiceServer::handleCallRequest(const std::string& msg) {
    // CALL:<caller>|TO:<callee>
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|TO:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string caller = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string callee = msg.substr(p2 + 4);

    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(callee);
    if (it == clients.end()) return;

    /*std::string notify = "INCOMING_CALL:" + caller;
    sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
           (sockaddr*)&it->second, sizeof(it->second));*/

    std::string notify = "INCOMING_CALL:" + caller + "|TO:" + callee;
    sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
        (sockaddr*)&it->second, sizeof(it->second));


    std::cout << "📞 " << caller << " đang gọi " << callee << "\n";
}

void VoiceServer::handleAcceptCall(const std::string& msg) {
    // ACCEPT_CALL:<callee>|FROM:<caller>
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|FROM:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string callee = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string caller = msg.substr(p2 + 6);

    {
        std::lock_guard<std::mutex> lock(mtx);
        callPairs[caller] = callee;
        callPairs[callee] = caller;
    }

    auto it = clients.find(caller);
    if (it != clients.end()) {
        //std::string notify = "CALL_ACCEPTED:" + callee;
        std::string notify = "CALL_ACCEPTED:" + callee + "|TO:" + caller;
        sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
               (sockaddr*)&it->second, sizeof(it->second));
    }

    std::cout << "✅ Cuộc gọi giữa " << caller << " và " << callee << " bắt đầu.\n";
}

void VoiceServer::handleRejectCall(const std::string& msg) {
    // REJECT_CALL:<callee>|FROM:<caller>
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|FROM:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string callee = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string caller = msg.substr(p2 + 6);

    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(caller);
    if (it != clients.end()) {
        //std::string notify = "CALL_REJECTED:" + callee;
        std::string notify = "CALL_REJECTED:" + callee + "|TO:" + caller;
        sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
               (sockaddr*)&it->second, sizeof(it->second));
    }

    std::cout << "❌ " << callee << " từ chối cuộc gọi từ " << caller << "\n";
}

void VoiceServer::forwardVoice(const std::string& from, const std::string& to, const char* data, int len) {
    std::lock_guard<std::mutex> lock(mtx);

    // chỉ chuyển voice khi A và B đang trong cùng cuộc gọi
    auto it = callPairs.find(from);
    if (it == callPairs.end() || it->second != to)
        return;

    auto dest = clients.find(to);
    if (dest == clients.end()) return;

    std::string header = "FROM:" + from + "|DATA|";
    std::vector<char> packet(header.begin(), header.end());
    packet.insert(packet.end(), data, data + len);

    sendto(udpSocket, packet.data(), (int)packet.size(), 0,
           (sockaddr*)&dest->second, sizeof(dest->second));
}
