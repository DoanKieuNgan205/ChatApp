#include "ChatServer.h"
#include "RegisterController.h" 
#include "LoginController.h"
#include "DatabaseHelper.h"
#include "FileServer.h"
#include "VoiceServer.h"
#include <iostream>
#include <algorithm>
#include <mutex>
#include <map>
#include <vector>
#include <string>
#include <algorithm> // Cho std::remove
#include <thread>    // Cho std::thread


using namespace std;

extern DatabaseHelper db; // dùng lại biến db từ main

vector<SOCKET> clients;
//map<string, SOCKET> userMap; 
mutex mtx;
FileServer fileServer;     // ✅ tạo 1 đối tượng FileServer toàn cục
VoiceServer voiceServer;   // ✅ Server phụ trách truyền âm thanh UDP

// 🔹 Hàm tách field từ JSON đơn giản
string parseField(const string& json, const string& field) {
    size_t pos = json.find("\"" + field + "\"");
    if (pos == string::npos) return "";
    pos = json.find(":", pos);
    if (pos == string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

// 🔹 Gửi tin nhắn đến tất cả client trừ người gửi
void broadcast(const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(mtx);
    for (SOCKET c : clients) {
        if (c != sender) {
            int ret = send(c, msg.c_str(), (int)msg.size(), 0);
            if (ret == SOCKET_ERROR) {
                cerr << "Send error: " << WSAGetLastError() << endl;
            }
        }
    }
}

// 🔹 Gửi tin nhắn riêng
void sendToUser(const string& username, const string& msg) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        send(it->second, msg.c_str(), (int)msg.size(), 0);
    }
}

// 🔹 Gửi danh sách user online cho tất cả client
void sendOnlineList() {
    lock_guard<mutex> lock(mtx);
    string list = "{ \"action\": \"online_list\", \"users\": [";
    bool first = true;
    for (const auto& pair : userMap) {
        if (!first) list += ",";
        list += "\"" + pair.first + "\"";
        first = false;
    }
    list += "] }\n";

    for (SOCKET c : clients) {
        send(c, list.c_str(), (int)list.size(), 0);
    }
}

// 🔹 Xử lý từng client
void handleClient(SOCKET client) {
    string request;
    char buf[4096];
    int bytes = 0;

    // Nhận gói login (có thể đến nhiều đợt)
    while (true) {
        bytes = recv(client, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) {
            closesocket(client);
            return;
        }
        buf[bytes] = '\0';
        request += buf;

        // Khi nhận đủ JSON có "}" thì thoát vòng lặp
        if (request.find('}') != string::npos) break;
    }

    string action = parseField(request, "action");

    // ✅ Nếu client gửi yêu cầu đăng ký tài khoản
    if (action == "register") {
        string result = RegisterController::handleRegister(request);
        send(client, result.c_str(), (int)result.size(), 0);
        closesocket(client);
        return; // không cần giữ kết nối sau khi đăng ký
    }

    // ✅ Nếu client gửi yêu cầu đăng nhập
    else if (action == "login") {
        // Kiểm tra login bằng DB (qua LoginController + Auth + DatabaseHelper)
        string loginResult = LoginController::handleLogin(request);
        string username = parseField(request, "username");
        if (username.empty()) {
            cout << "[WARN] Username trống — đóng kết nối\n";
            closesocket(client);
            return;
        }

        // thêm \n cho JSON login
        loginResult += "\n";
        send(client, loginResult.c_str(), (int)loginResult.size(), 0);


        string status = parseField(loginResult, "status");
        cout << "[DEBUG] loginResult: " << loginResult << endl;
        cout << "[DEBUG] status parse: " << status << endl;
        if (status != "success") {
            cout << "[DEBUG] Login failed for user: " << username << endl;
            closesocket(client);
            return;
        }
        cout << "[DEBUG] Login success for user: " << username << endl;

        {
            lock_guard<mutex> lock(mtx);
            clients.push_back(client);
            userMap[username] = client;
        }
        sendOnlineList(); // ✅ gửi danh sách online sau khi user vào

        cout << "[INFO] User login: " << username << endl;

        // Gửi lịch sử chat cho user sau khi đăng nhập
        vector<string> history = db.getChatHistory(username);
        if (!history.empty()) {
            cout << "[INFO] Gui lich su chat cho user: " << username << " (" << history.size() << " tin nhan)\n";
            for (auto& line : history) {
                string safeMsg = line;
                // Escape ký tự " để không lỗi JSON
                for (size_t pos = 0; (pos = safeMsg.find("\"", pos)) != string::npos; pos += 2)
                    safeMsg.replace(pos, 1, "\\\"");

                string formatted = "{ \"action\": \"history\", \"message\": \"" + safeMsg + "\" }\n";
                send(client, formatted.c_str(), (int)formatted.size(), 0);
            }
        }

        
        // Vòng lặp chat
        while (true) {
            int bytes = recv(client, buf, sizeof(buf)-1, 0);
            if (bytes <= 0) break;
            buf[bytes] = '\0';
            string msg(buf);

            string action = parseField(msg, "action");


            if (action == "list") {
                string list = "{ \"action\": \"online_list\", \"users\": [";
                {
                    lock_guard<mutex> lock(mtx);
                    bool first = true;
                    for (const auto& pair : userMap) {
                        if (!first) list += ",";
                        list += "\"" + pair.first + "\"";
                        first = false;
                    }
                }
                list += "] }\n";
                send(client, list.c_str(), (int)list.size(), 0);
            }
            
            else if (action == "private") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string message = parseField(msg, "message");
                if (!from.empty() && !to.empty() && !message.empty()) {
                    // Tạo gói JSON chuẩn
                    string chatMsg = "{ \"action\": \"private\", \"from\": \"" + from +
                                    "\", \"to\": \"" + to +
                                    "\", \"message\": \"" + message + "\" }\n";
                    sendToUser(to, chatMsg);
                    sendToUser(from, chatMsg);
                    db.saveMessage(from, to, message); // Lưu tin nhắn vào DB
                }
            }
            

            else if (action == "sendfile") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string filename = parseField(msg, "filename");
                string sizeStr = parseField(msg, "size");

                cout << "[INFO] " << from << " muon gui file " << filename << " (" << sizeStr << " bytes) -> " << to << endl;

                // Trả lời client biết rằng hãy mở kết nối riêng đến port 9999
                string notify = "{ \"action\": \"file_ready\", \"port\": 9999, \"message\": \"Hay gui file qua cong 9999\" }\n";
                send(client, notify.c_str(), notify.size(), 0);
            }
            
            else if (action == "voice_register") {
                string username = parseField(msg, "username");
                string ip = parseField(msg, "ip");
                string portStr = parseField(msg, "port");

                if (username.empty() || ip.empty() || portStr.empty()) {
                    string err = R"({ "action": "error", "message": "Thong tin voice_register khong hop le" })";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                sockaddr_in addr;
                addr.sin_family = AF_INET;
                inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
                addr.sin_port = htons(stoi(portStr));

                voiceServer.registerClient(username, addr);
                string ok = R"({ "action": "voice_register_ok", "message": "Da dang ky voice thanh cong" })" "\n";
                send(client, ok.c_str(), (int)ok.size(), 0);

                cout << "[VOICE] " << username << " đăng ký địa chỉ UDP " << ip << ":" << portStr << endl;
            }

            else if (action == "voice_call") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");

                if (from.empty() || to.empty()) {
                    string err = R"({ "action": "error", "message": "Thông tin cuộc gọi không hợp lệ" })";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                // Gửi yêu cầu tới người nhận
                string callMsg = "{ \"action\": \"incoming_call\", \"from\": \"" + from + "\" }\n";
                sendToUser(to, callMsg);
                cout << "[VOICE] Cuộc gọi voice từ " << from << " đến " << to << endl;
            }
            

            else { // Chat chung
                string message = parseField(msg, "message");
                if (!username.empty() && !message.empty()) {
                    string chatMsg = username + ": " + message;
                    cout << chatMsg << endl;
                    broadcast(chatMsg, client);
                    db.saveMessage(username, "ALL", message); // Lưu chat chung
                }
            }
        }

        {
            lock_guard<mutex> lock(mtx);
            clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
            userMap.erase(username);
        }
        closesocket(client);
        sendOnlineList(); // ✅ gửi lại danh sách khi user thoát

        cout << "[INFO] User disconnected: " << username << endl;
    }

    else {
        string err = R"({ "action": "error", "message": "Unknown action" })" "\n";
        send(client, err.c_str(), (int)err.size(), 0);
        closesocket(client);
    }
}

/*#include "ChatServer.h"
#include "DatabaseHelper.h"
#include "Packet.h"
#include <iostream>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

// Dùng biến toàn cục đã khai báo ở main
extern DatabaseHelper db;
extern std::map<std::string, SOCKET> userMap;
extern std::mutex mtx;
extern std::vector<SOCKET> clients;

// 🔹 Hàm phụ: parse field từ JSON (đơn giản)
static std::string parseField(const std::string& json, const std::string& field) {
    size_t pos = json.find("\"" + field + "\"");
    if (pos == string::npos) return "";
    pos = json.find(":", pos);
    if (pos == string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == string::npos) return "";
    size_t end = json.find("\"", pos + 1);
    if (end == string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

// 🔹 Gửi tin nhắn tới toàn bộ client trừ người gửi
void ChatServer::broadcast(const std::string& msg, SOCKET sender) {
    lock_guard<mutex> lock(mtx);
    for (SOCKET c : clients) {
        if (c != sender) {
            send(c, msg.c_str(), (int)msg.size(), 0);
        }
    }
}

// 🔹 Gửi riêng tin nhắn đến user
void ChatServer::sendToUser(const std::string& username, const std::string& msg) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        send(it->second, msg.c_str(), (int)msg.size(), 0);
    }
}

// 🔹 Xử lý gói chat JSON từ client
std::string ChatServer::handleChat(const std::string& jsonRequest) {
    std::string action = parseField(jsonRequest, "action");
    std::string from = parseField(jsonRequest, "from");
    std::string to = parseField(jsonRequest, "to");
    std::string message = parseField(jsonRequest, "message");
    std::string username = parseField(jsonRequest, "username");

    // ✅ Người dùng vừa JOIN chat
    if (action == "join_chat") {
        std::lock_guard<std::mutex> lock(mtx);
        if (!username.empty()) {
            userMap[username] = INVALID_SOCKET; // sẽ được cập nhật khi accept socket thực
            std::cout << "[JOIN] " << username << " đã tham gia phòng chat.\n";

            // Gửi danh sách online về Gateway (hoặc các client khác)
            std::string listMsg = "{ \"action\": \"online_list\", \"users\": [";
            bool first = true;
            for (auto& p : userMap) {
                if (!first) listMsg += ",";
                listMsg += "\"" + p.first + "\"";
                first = false;
            }
            listMsg += "] }";

            return listMsg;
        }
        return R"({"action":"error","message":"Thiếu username trong join_chat"})";
    }

    // ✅ Chat riêng
    if (action == "private") {
        if (from.empty() || to.empty() || message.empty())
            return R"({"action":"error","message":"Thiếu tham số trong gói tin chat"})";

        std::string formatted =
            "{ \"action\": \"private\", \"from\": \"" + from +
            "\", \"to\": \"" + to +
            "\", \"message\": \"" + message + "\" }\n";

        sendToUser(to, formatted);
        sendToUser(from, formatted);

        db.saveMessage(from, to, message, "", "text");

        std::cout << "[CHAT] " << from << " -> " << to << ": " << message << std::endl;

        return R"({"action":"private_ok","status":"success"})";
    }


    // ✅ Chat broadcast (toàn bộ)
    else if (action == "public") {
        if (from.empty() || message.empty())
            return R"({"action":"error","message":"Thiếu tham số trong broadcast"})";

        std::string formatted =
            "{ \"action\": \"public\", \"from\": \"" + from +
            "\", \"message\": \"" + message + "\" }\n";

        SOCKET senderSock = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = userMap.find(from);
            if (it != userMap.end()) senderSock = it->second;
        }

        broadcast(formatted, senderSock);
        db.saveMessage(from, "ALL", message, "", "text");

        std::cout << "[BROADCAST] " << from << ": " << message << std::endl;

        return R"({"action":"broadcast_ok","status":"success"})";
    }

    // ✅ Lấy lịch sử chat
    else if (action == "history") {
        std::vector<std::string> history = db.getHistory(from);
        std::string result = "{ \"action\": \"history\", \"messages\": [";
        for (size_t i = 0; i < history.size(); ++i) {
            if (i > 0) result += ",";
            result += "\"" + history[i] + "\"";
        }
        result += "] }";
        return result;
    }
    
    // ❌ Không hợp lệ
    else {
        return R"({"action":"error","message":"Unknown chat action"})";
    }
}*/


