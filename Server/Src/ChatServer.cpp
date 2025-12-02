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
#include <algorithm> 
#include <thread>    



using namespace std;

extern DatabaseHelper db; 

vector<SOCKET> clients;
 
mutex mtx;
FileServer fileServer;     
VoiceServer voiceServer;   


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


void sendToUser(const string& username, const string& msg) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        send(it->second, msg.c_str(), (int)msg.size(), 0);
    }
}


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


void handleClient(SOCKET client) {

    cout << "\n[INFO] ===========================================" << endl;
    cout << "[INFO] NEW TCP CONNECTION - Socket: " << client << endl;
    cout << "[INFO] ===========================================" << endl;
    
    string request;
    char buf[4096];
    int bytes = 0;

    while (true) {
        bytes = recv(client, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) {
            cout << "[WARN] Connection closed before receiving data" << endl;
            closesocket(client);
            return;
        }
        buf[bytes] = '\0';
        request += buf;

        cout << "[DEBUG] Received initial bytes: " << bytes << endl;
        cout << "[DEBUG] Buffer content: " << buf << endl;
        
        if (request.find('}') != string::npos) break;
    }

    string action = parseField(request, "action");
    cout << "[DEBUG] Initial action: " << action << endl;

    if (action == "register") {
        string result = RegisterController::handleRegister(request);
        send(client, result.c_str(), (int)result.size(), 0);
        closesocket(client);
        return; 
    }
    
    else if (action == "login") {    
        string loginResult = LoginController::handleLogin(request);
        string username = parseField(request, "username");
        if (username.empty()) {
            cout << "[WARN] Username trống — đóng kết nối\n";
            closesocket(client);
            return;
        }
        loginResult += "\n";
        send(client, loginResult.c_str(), (int)loginResult.size(), 0);

        string status = parseField(loginResult, "status");
        cout << "[DEBUG] loginResult: " << loginResult << endl;
        cout << "[DEBUG] status parse: " << status << endl;
        if (status != "success") {
            cout << "[DEBUG] Login failed for user: " << username << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            closesocket(client);
            return;
        }
        cout << "[DEBUG] Login success for user: " << username << endl;
        {
            lock_guard<mutex> lock(mtx);
            clients.push_back(client);
            userMap[username] = client;
        }
        sendOnlineList(); 

        cout << "[INFO] User login: " << username << endl;
        cout << "[INFO] ========== ENTERING MESSAGE LOOP ==========" << endl;

        /*vector<string> chatHistory = db.getChatHistory(username);
        vector<string> fileHistory = db.getFileHistory(username);
        vector<string> callHistory = db.getCallHistory(username);

        
        auto escape = [](string s) {
            string r = "";
            for (char c : s) {
                if (c == '\"') r += "\\\"";
                else r += c;
            }
            return r;
        };

        string json = "{ \"action\": \"history_response\", \"username\": \"" + username + "\", \"chatHistory\": [";
        for (int i = 0; i < chatHistory.size(); i++) {
            json += "\"" + escape(chatHistory[i]) + "\"";
            if (i < chatHistory.size()-1) json += ",";
        }

        json += "], \"fileHistory\": [";

        for (int i = 0; i < fileHistory.size(); i++) {
            json += "\"" + escape(fileHistory[i]) + "\"";
            if (i < fileHistory.size()-1) json += ",";
        }

        json += "], \"callHistory\": [";

        for (int i = 0; i < callHistory.size(); i++) {
            json += "\"" + escape(callHistory[i]) + "\"";
            if (i < callHistory.size()-1) json += ",";
        }

        json += "] }\n";

        send(client, json.c_str(), (int)json.size(), 0);*/
     
        while (true) {
            bytes = recv(client, buf, sizeof(buf)-1, 0);
            if (bytes <= 0) {
                cout << "[INFO] Connection closed or error, bytes=" << bytes << endl;
                break;
            }
            
            buf[bytes] = '\0';
            string msg(buf);

            // LOG CHI TIẾT
            cout << "\n[DEBUG] ========================================" << endl;
            cout << "[DEBUG] Received message from " << username << endl;
            cout << "[DEBUG] Message length: " << bytes << " bytes" << endl;
            cout << "[DEBUG] Raw message: " << msg << endl;
            cout << "[DEBUG] ========================================\n" << endl;

            string action = parseField(msg, "action");
            cout << "[DEBUG] Parsed action: '" << action << "'" << endl;

            if (action == "get_history") {
                string user1 = parseField(msg, "user1");
                string user2 = parseField(msg, "user2");
                
                cout << "[INFO] Gateway yêu cầu lịch sử: " << user1 << " <-> " << user2 << endl;
                
                if (user1.empty() || user2.empty()) {
                    string err = R"({ "action": "error", "message": "user1 hoặc user2 trống" })";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                // Lấy lịch sử từ database
                vector<string> chatHistory = db.getPrivateChatHistory(user1, user2); 
                vector<string> fileHistory = db.getPrivateFileHistory(user1, user2); 
                vector<string> callHistory = db.getPrivateCallHistory(user1, user2); 

                cout << "[DB] Loaded: " << chatHistory.size() << " chats, "
                     << fileHistory.size() << " files, " 
                     << callHistory.size() << " calls" << endl;

                auto escape = [](string s) {
                    string r = "";
                    for (char c : s) {
                        if (c == '\"') r += "\\\"";
                        else if (c == '\\') r += "\\\\";
                        else if (c == '\n') r += "\\n";
                        else if (c == '\r') r += "\\r";
                        else if (c == '\t') r += "\\t";
                        else r += c;
                    }
                    return r;
                };

                // Tạo response JSON
                string json = "{ \"action\": \"history_response\", \"user1\": \"" + user1 + 
                            "\", \"user2\": \"" + user2 + "\", \"chatHistory\": [";

                for (size_t i = 0; i < chatHistory.size(); i++) {
                    json += "\"" + escape(chatHistory[i]) + "\"";
                    if (i < chatHistory.size()-1) json += ",";
                }

                json += "], \"fileHistory\": [";
                for (size_t i = 0; i < fileHistory.size(); i++) {
                    json += "\"" + escape(fileHistory[i]) + "\"";
                    if (i < fileHistory.size()-1) json += ",";
                }

                json += "], \"callHistory\": [";
                for (size_t i = 0; i < callHistory.size(); i++) {
                    json += "\"" + escape(callHistory[i]) + "\"";
                    if (i < callHistory.size()-1) json += ",";
                }
                json += "] }\n";
                
                cout << "[INFO] Sending response (" << json.length() << " bytes)" << endl;
                int sent = send(client, json.c_str(), (int)json.size(), 0);
                if (sent == SOCKET_ERROR) {
                    cout << "[ERROR] Send failed: " << WSAGetLastError() << endl;
                } else {
                    cout << "[INFO] ✅ Sent " << sent << " bytes successfully" << endl;
                }
                
                continue;
            }

            if (action == "get_private_history") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                if (from.empty() || to.empty()) return;

                // Giả định bạn có hàm db.getPrivateChatHistory(user1, user2)
                // Hàm này phải lấy cả tin nhắn từ user1->user2 và user2->user1
                vector<string> chatHistory = db.getPrivateChatHistory(from, to); 
                // Lấy lịch sử file và call riêng tư (Nếu cần)
                vector<string> fileHistory = db.getPrivateFileHistory(from, to); 
                vector<string> callHistory = db.getPrivateCallHistory(from, to); 

                auto escape = [](string s) {
                    string r = "";
                    for (char c : s) {
                        if (c == '\"') r += "\\\"";
                        else r += c;
                    }
                    return r;
                };

                // Gửi phản hồi
                string json = "{ \"action\": \"history_response\", \"username\": \"" + from + "\", \"to\": \"" + to + "\", \"chatHistory\": [";

                for (int i = 0; i < chatHistory.size(); i++) {
                    json += "\"" + escape(chatHistory[i]) + "\"";
                    if (i < chatHistory.size()-1) json += ",";
                }

                json += "], \"fileHistory\": [";
                for (int i = 0; i < fileHistory.size(); i++) {
                    json += "\"" + escape(fileHistory[i]) + "\"";
                    if (i < fileHistory.size()-1) json += ",";
                }

                json += "], \"callHistory\": [";
                for (int i = 0; i < callHistory.size(); i++) {
                    json += "\"" + escape(callHistory[i]) + "\"";
                    if (i < callHistory.size()-1) json += ",";
                }
                json += "] }\n";
                send(client, json.c_str(), (int)json.size(), 0);

                cout << "[INFO] Đã gửi lịch sử riêng tư cho " << from << " chat với " << to << ".\n";

                continue;
            }

            else if (action == "save_private_message") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string message = parseField(msg, "message");
                if (!from.empty() && !to.empty() && !message.empty()) {
                    db.saveMessage(from, to, message); // Thực hiện lưu trữ
                    cout << "[DB] Saved private message via Gateway: " << from << " -> " << to << endl;
                }

                continue;
            }

            else if (action == "list") {
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

                continue;
            }
            
            else if (action == "private") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string message = parseField(msg, "message");
                if (!from.empty() && !to.empty() && !message.empty()) {
                    
                    string chatMsg = "{ \"action\": \"private\", \"from\": \"" + from +
                                    "\", \"to\": \"" + to +
                                    "\", \"message\": \"" + message + "\" }\n";
                    sendToUser(to, chatMsg);
                    sendToUser(from, chatMsg);
                    db.saveMessage(from, to, message); 
                }
            }
            
            else if (action == "sendfile") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string filename = parseField(msg, "filename");
                string sizeStr = parseField(msg, "size");

                cout << "[INFO] " << from << " muon gui file " << filename << " (" << sizeStr << " bytes) -> " << to << endl;

                
                string notify = "{ \"action\": \"file_ready\", \"port\": 9999, \"message\": \"Hay gui file qua cong 9999\" }\n";
                send(client, notify.c_str(), notify.size(), 0);
                db.saveFileHistory(from, to, filename);
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

                continue;
            }

            else if (action == "voice_call") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");

                if (from.empty() || to.empty()) {
                    string err = R"({ "action": "error", "message": "Thông tin cuộc gọi không hợp lệ" })";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                
                string callMsg = "{ \"action\": \"incoming_call\", \"from\": \"" + from + "\" }\n";
                sendToUser(to, callMsg);
                cout << "[VOICE] Cuộc gọi voice từ " << from << " đến " << to << endl;
                db.saveCallHistory(from, to, "started");

                continue;
            }
            
            else { 
                string message = parseField(msg, "message");
                if (!username.empty() && !message.empty()) {
                    string chatMsg = username + ": " + message;
                    cout << chatMsg << endl;
                    broadcast(chatMsg, client);
                    db.saveMessage(username, "ALL", message); 
                }
            }
        }

        {
            lock_guard<mutex> lock(mtx);
            clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
            userMap.erase(username);
        }
        closesocket(client);
        sendOnlineList(); 

        cout << "[INFO] User disconnected: " << username << endl;
    }

    else {
        string err = R"({ "action": "error", "message": "Unknown action" })" "\n";
        send(client, err.c_str(), (int)err.size(), 0);
        closesocket(client);
    }
}






