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

void sendMessageToUser(const std::string& username, const std::string& message) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        std::string msg = message + "\n";
        send(it->second, msg.c_str(), (int)msg.size(), 0);
        cout << "[VOICE->CHAT] Sent to " << username << ": " << message << endl;
    }
}

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
        result += "\n";
        
        int sent = send(client, result.c_str(), (int)result.size(), 0);
        
        if (sent != SOCKET_ERROR) {
            cout << "[INFO] Đã gửi phản hồi đăng ký cho Gateway." << endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
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

        while (true) {
            bytes = recv(client, buf, sizeof(buf)-1, 0);
            if (bytes <= 0) {
                cout << "[INFO] Connection closed or error, bytes=" << bytes << endl;
                break;
            }
            
            buf[bytes] = '\0';
            string msg(buf);

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
                
                cout << "[DEBUG] JSON Response (first 500 chars):\n" << json.substr(0, 500) << endl;
                
                cout << "[INFO] Sending response (" << json.length() << " bytes)" << endl;
                int sent = send(client, json.c_str(), (int)json.size(), 0);
                if (sent == SOCKET_ERROR) {
                    cout << "[ERROR] Send failed: " << WSAGetLastError() << endl;
                } else {
                    cout << "[INFO] ✅ Sent " << sent << " bytes successfully" << endl;
                }
                
                continue;
            }

            else if (action == "get_user_info") {
                string user = parseField(msg, "username");

                if (user.empty()) {
                    string err = R"({ "action": "my_profile_response", "error": "MISSING_USERNAME" })" "\n";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                cout << "[INFO] Truy vấn profile cho user: " << user << endl;

                string userEmail = db.getUserEmail(user);

                string jsonResponse = "{ \"action\": \"my_profile_response\", "
                                    "\"username\": \"" + user + "\", "
                                    "\"email\": \"" + userEmail + "\" }\n";

                int sent = send(client, jsonResponse.c_str(), (int)jsonResponse.size(), 0);
                
                if (sent != SOCKET_ERROR) {
                    cout << "[INFO] Đã gửi thông tin profile của " << user << " về Gateway." << endl;
                }
                continue;
            }
            else if (action == "update_email") {
                string user = parseField(msg, "username");
                string newEmail = parseField(msg, "newEmail");

                bool success = db.updateUserEmail(user, newEmail);

                string response = "{ \"action\": \"update_profile_response\", \"status\": \"" +
                                string(success ? "success" : "error") +
                                "\", \"message\": \"" +
                                string(success ? "Cap nhat thanh cong" : "Loi database") +
                                "\", \"newEmail\": \"" + (success ? newEmail : "") + "\" }\n";

                send(client, response.c_str(), (int)response.size(), 0);
            }
            else if (action == "change_password") {
                string user = parseField(msg, "username");
                string oldPass = parseField(msg, "oldPass");
                string newPass = parseField(msg, "newPass");

                cout << "[INFO] Yêu cầu đổi mật khẩu cho user: " << user << endl;

                if (user.empty() || oldPass.empty() || newPass.empty()) {
                    string err = R"({ "action": "update_profile_response", "status": "error", "message": "Dữ liệu không hợp lệ" })" "\n";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }
                bool isChanged = db.updatePassword(user, oldPass, newPass);

                string response;
                if (isChanged) {
                    cout << "[DB] Đổi mật khẩu thành công cho: " << user << endl;
                    response = "{ \"action\": \"update_profile_response\", \"status\": \"success\", \"message\": \"Doi mat khau thanh cong\" }\n";
            } else {
                    cout << "[DB] Đổi mật khẩu thất bại (Sai pass cũ hoặc lỗi hệ thống)" << endl;
                    response = "{ \"action\": \"update_profile_response\", \"status\": \"error\", \"message\": \"Mat khau cu khong chinh xac hoac loi he thong\" }\n";
                }
                send(client, response.c_str(), (int)response.size(), 0);
                continue;
            }

            else if (action == "save_private_message") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string message = parseField(msg, "message");
                if (!from.empty() && !to.empty() && !message.empty()) {
                    db.saveMessage(from, to, message);
                    cout << "[DB] Saved private message via Gateway: " << from << " -> " << to << endl;
                }
                continue;
            }

            else if (action == "save_file") {
                string from = parseField(msg, "from");
                string to = parseField(msg, "to");
                string filename = parseField(msg, "filename");
                
                if (!from.empty() && !to.empty() && !filename.empty()) {
                    db.saveFileHistory(from, to, filename);
                    cout << "[DB] ✅ Saved file history: " << from << " -> " << to << ": " << filename << endl;
                    
                    string ack = R"({ "action": "file_saved", "status": "success" })" "\n";
                    send(client, ack.c_str(), (int)ack.size(), 0);
                } else {
                    cout << "[WARN] save_file: Missing from/to/filename" << endl;
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
                continue;
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
                continue;
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
                string to   = parseField(msg, "to");

                if (from.empty() || to.empty()) {
                    string err = R"({ "action": "error", "message": "Thông tin cuộc gọi không hợp lệ" })";
                    send(client, err.c_str(), (int)err.size(), 0);
                    continue;
                }

                string callMsg =
                    "{ \"action\": \"incoming_call\", \"from\": \"" + from + "\" }\n";

                sendToUser(to, callMsg);

                cout << "[VOICE] Cuộc gọi voice từ " << from << " đến " << to << endl;

                continue;
            }


            else if (action == "SAVE_CALL_HISTORY") {
                string from   = parseField(msg, "from");
                string to     = parseField(msg, "to");
                string status = parseField(msg, "status");
                
                int duration = 0;
                size_t durPos = msg.find("\"duration\"");
                if (durPos != string::npos) {
                    durPos = msg.find(":", durPos);
                    if (durPos != string::npos) {
                        durPos++;
                        while (durPos < msg.length() && (msg[durPos] == ' ' || msg[durPos] == '\t')) durPos++;
                        
                        string numStr = "";
                        while (durPos < msg.length() && isdigit(msg[durPos])) {
                            numStr += msg[durPos];
                            durPos++;
                        }
                        
                        if (!numStr.empty()) {
                            try {
                                duration = stoi(numStr);
                            } catch (const exception& e) {
                                cout << "[WARN] Cannot parse duration: " << e.what() << endl;
                                duration = 0;
                            }
                        }
                    }
                }

                if (from.empty() || to.empty() || status.empty()) {
                    cout << "[CALL] ❌ Missing fields (from/to/status)" << endl;
                    
                    string errJson = "{ \"action\": \"CALL_HISTORY_SAVED\", \"status\": \"error\", \"message\": \"Missing fields\" }\n";
                    send(client, errJson.c_str(), (int)errJson.size(), 0);
                    continue;
                }

                if (duration <= 0) {
                    cout << "[CALL] ⚠️ Ignored: duration=" << duration << endl;
                    continue;
                }

                cout << "[CALL] 💾 Saving call: " << from << " -> " << to 
                    << ", duration=" << duration << "s, status=" << status << endl;

                bool saved = db.saveCallHistory(from, to, duration, status);
                
                if (saved) {
                    cout << "[DB] ✅ Call history saved successfully" << endl;
                    
                    string ackJson = "{ \"action\": \"CALL_HISTORY_SAVED\", \"status\": \"success\", "
                                    "\"from\": \"" + from + "\", \"to\": \"" + to + "\" }\n";
                    
                    send(client, ackJson.c_str(), (int)ackJson.size(), 0);
                    
                    sendToUser(from, ackJson);
                    sendToUser(to, ackJson);
                    
                } else {
                    cout << "[DB] ❌ Failed to save call history" << endl;
                    
                    string errJson = "{ \"action\": \"CALL_HISTORY_SAVED\", \"status\": \"error\" }\n";
                    send(client, errJson.c_str(), (int)errJson.size(), 0);
                }

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






