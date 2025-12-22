#include "VoiceServer.h"
#include <iostream>
#include <thread>
#include <cstring>

std::function<void(const std::string&, const std::string&)> notifyCallEventCallback = nullptr;


VoiceServer::VoiceServer() {
    running = false;
    udpSocket = INVALID_SOCKET;
}

void VoiceServer::setNotifyCallback(std::function<void(const std::string&, const std::string&)> callback) {
    notifyCallEventCallback = callback;
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
    
    const int BUFSIZE = 65536;
    std::vector<char> buffer(BUFSIZE);
    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);


    while (running) {
        int bytesReceived = recvfrom(udpSocket, buffer.data(), (int)buffer.size(), 0,
                                     (sockaddr*)&clientAddr, &clientAddrLen);
        if (bytesReceived == SOCKET_ERROR || bytesReceived <= 0) {
            continue;
        }

        std::string msg(buffer.data(), bytesReceived);

        if (msg.rfind("REGISTER:", 0) == 0) {
            std::string username = msg.substr(9);
            registerClient(username, clientAddr);
            std::cout << "🟢 " << username << " đã đăng ký voice.\n";
            continue;
        }

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

        if (msg.rfind("END_CALL:", 0) == 0) {
            handleEndCall(msg);
            continue;
        }

        const std::string dataMarker = "|DATA|";

        std::string msgStr(buffer.data(), bytesReceived);

        size_t markerPos = msgStr.find(dataMarker);
        if (markerPos == std::string::npos) {
            continue;
        }

        std::string header = msgStr.substr(0, markerPos); 
        size_t p1 = header.find("FROM:");
        size_t p2 = header.find("|TO:");
        if (p1 == std::string::npos || p2 == std::string::npos) {
            continue;
        }
        std::string from = header.substr(p1 + 5, p2 - (p1 + 5));
        std::string to = header.substr(p2 + 4); 

        int audioOffset = (int)(markerPos + dataMarker.length());
        int audioLen = bytesReceived - audioOffset;
        if (audioLen <= 0) continue;
        const char* audioData = buffer.data() + audioOffset;

        forwardVoice(from, to, audioData, audioLen);
    }
}

void VoiceServer::registerClient(const std::string& username, const sockaddr_in& addr) {
    std::lock_guard<std::mutex> lock(mtx);
    
    clients[username] = addr;
}

void VoiceServer::handleUnregister(const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);

    clients.erase(username);

    auto it = callPairs.find(username);
    if (it != callPairs.end()) {
        std::string partner = it->second;
        
        callPairs.erase(username);
        callPairs.erase(partner);
        
        std::cout << "⏹️ " << username << " hủy đăng ký, tự động kết thúc cuộc gọi với " << partner << ".\n";
    } else {
        std::cout << "👻 " << username << " đã hủy đăng ký voice.\n";
    }
}

void VoiceServer::handleCallRequest(const std::string& msg) {
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|TO:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string caller = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string callee = msg.substr(p2 + 4);

    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(callee);
    if (it == clients.end()) return;


    std::string notify = "INCOMING_CALL:" + caller + "|TO:" + callee;
    sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
        (sockaddr*)&it->second, sizeof(it->second));


    std::cout << "📞 " << caller << " đang gọi " << callee << "\n";

    // THÊM: Thông báo cho ChatServer
    if (notifyCallEventCallback) {
        std::string jsonMsg = "{\"action\":\"INCOMING_CALL\",\"from\":\"" + caller + "\",\"to\":\"" + callee + "\"}";
        notifyCallEventCallback(callee, jsonMsg);
    }
}

void VoiceServer::handleAcceptCall(const std::string& msg) {
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
        std::string notify = "CALL_ACCEPTED:" + callee + "|TO:" + caller;
        sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
               (sockaddr*)&it->second, sizeof(it->second));
    }

    std::cout << "✅ Cuộc gọi giữa " << caller << " và " << callee << " bắt đầu.\n";

    if (notifyCallEventCallback) {
        std::string jsonMsg1 = "{\"action\":\"CALL_ACCEPTED\",\"from\":\"" + callee + "\",\"to\":\"" + caller + "\"}";
        notifyCallEventCallback(caller, jsonMsg1);
        
        std::string jsonMsg2 = "{\"action\":\"CALL_ACCEPTED\",\"from\":\"" + callee + "\",\"to\":\"" + caller + "\"}";
        notifyCallEventCallback(callee, jsonMsg2);
    }
}

void VoiceServer::handleRejectCall(const std::string& msg) {
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|FROM:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string callee = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string caller = msg.substr(p2 + 6);

    std::lock_guard<std::mutex> lock(mtx);
    auto it = clients.find(caller);
    if (it != clients.end()) {
        std::string notify = "CALL_REJECTED:" + callee + "|TO:" + caller;
        sendto(udpSocket, notify.c_str(), (int)notify.size(), 0,
               (sockaddr*)&it->second, sizeof(it->second));
    }

    std::cout << "❌ " << callee << " từ chối cuộc gọi từ " << caller << "\n";

    if (notifyCallEventCallback) {
        std::string jsonMsg = "{\"action\":\"CALL_REJECTED\",\"from\":\"" + callee + "\",\"to\":\"" + caller + "\"}";
        notifyCallEventCallback(caller, jsonMsg);
    }
}

void VoiceServer::handleEndCall(const std::string& msg) {
    size_t p1 = msg.find(":");
    size_t p2 = msg.find("|TO:");
    if (p1 == std::string::npos || p2 == std::string::npos) return;

    std::string caller = msg.substr(p1 + 1, p2 - (p1 + 1));
    std::string callee = msg.substr(p2 + 4);

    std::lock_guard<std::mutex> lock(mtx);
    
    callPairs.erase(caller);
    callPairs.erase(callee);
    
    std::cout << "📴 Cuộc gọi kết thúc: " << caller << " <-> " << callee << "\n";
}

void VoiceServer::forwardVoice(const std::string& from, const std::string& to, const char* data, int len) {
    std::lock_guard<std::mutex> lock(mtx);

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
