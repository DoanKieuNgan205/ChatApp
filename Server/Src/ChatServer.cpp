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
    string request;
    char buf[4096];
    int bytes = 0;

    
    while (true) {
        bytes = recv(client, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) {
            closesocket(client);
            return;
        }
        buf[bytes] = '\0';
        request += buf;

        
        if (request.find('}') != string::npos) break;
    }

    string action = parseField(request, "action");

    
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

        vector<string> chatHistory = db.getChatHistory(username);
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

        string json = "{ \"action\": \"history_response\", \"chat\": [";

        for (int i = 0; i < chatHistory.size(); i++) {
            json += "\"" + escape(chatHistory[i]) + "\"";
            if (i < chatHistory.size()-1) json += ",";
        }

        json += "], \"file\": [";

        for (int i = 0; i < fileHistory.size(); i++) {
            json += "\"" + escape(fileHistory[i]) + "\"";
            if (i < fileHistory.size()-1) json += ",";
        }

        json += "], \"call\": [";

        for (int i = 0; i < callHistory.size(); i++) {
            json += "\"" + escape(callHistory[i]) + "\"";
            if (i < callHistory.size()-1) json += ",";
        }

        json += "] }\n";

        send(client, json.c_str(), (int)json.size(), 0);

        
        
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

/*void handleClient(SOCKET client) {
    string username;        // username dùng toàn bộ function
    char buf[4096];
    int bytes = 0;

    // Nhận dữ liệu từ client
    while (true) {
        bytes = recv(client, buf, sizeof(buf) - 1, 0);
        if (bytes <= 0) break;
        buf[bytes] = '\0';
        string msg(buf);

        string action = parseField(msg, "action");

        // ====== XỬ LÝ ĐĂNG KÝ ======
        if (action == "register") {
            string result = RegisterController::handleRegister(msg);
            send(client, result.c_str(), (int)result.size(), 0);
            closesocket(client);
            return;
        }

        // ====== XỬ LÝ ĐĂNG NHẬP ======
        else if (action == "login") {
            string loginResult = LoginController::handleLogin(msg);
            username = parseField(msg, "username");
            if (username.empty()) {
                cout << "[WARN] Username trống — đóng kết nối\n";
                closesocket(client);
                return;
            }

            loginResult += "\n";
            send(client, loginResult.c_str(), (int)loginResult.size(), 0);

            string status = parseField(loginResult, "status");
            if (status != "success") {
                cout << "[DEBUG] Login failed for user: " << username << endl;
                closesocket(client);
                return;
            }

            // Thêm client vào danh sách
            {
                lock_guard<mutex> lock(mtx);
                clients.push_back(client);
                userMap[username] = client;
            }
            sendOnlineList();
            cout << "[INFO] User login: " << username << endl;

            // Gửi lịch sử tổng hợp
            vector<string> chatHistory = db.getChatHistory(username);
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

            string json = "{ \"action\": \"history_response\", \"chat\": [";
            for (int i = 0; i < chatHistory.size(); i++) {
                json += "\"" + escape(chatHistory[i]) + "\"";
                if (i < chatHistory.size() - 1) json += ",";
            }
            json += "], \"file\": [";
            for (int i = 0; i < fileHistory.size(); i++) {
                json += "\"" + escape(fileHistory[i]) + "\"";
                if (i < fileHistory.size() - 1) json += ",";
            }
            json += "], \"call\": [";
            for (int i = 0; i < callHistory.size(); i++) {
                json += "\"" + escape(callHistory[i]) + "\"";
                if (i < callHistory.size() - 1) json += ",";
            }
            json += "] }\n";
            send(client, json.c_str(), (int)json.size(), 0);
        }

        // ====== XỬ LÝ DANH SÁCH ONLINE ======
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
        }

        // ====== XỬ LÝ CHAT RIÊNG ======
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

        // ====== XỬ LÝ CHAT CHUNG ======
        else if (action == "broadcast") {
            string message = parseField(msg, "message");
            if (!username.empty() && !message.empty()) {
                string chatMsg = username + ": " + message;
                cout << chatMsg << endl;
                broadcast(chatMsg, client);
                db.saveMessage(username, "ALL", message);
            }
        }

        // ====== XỬ LÝ FILE ======
        else if (action == "sendfile") {
            string from = parseField(msg, "from");
            string to = parseField(msg, "to");
            string filename = parseField(msg, "filename");
            string sizeStr = parseField(msg, "size");
            string notify = "{ \"action\": \"file_ready\", \"port\": 9999, \"message\": \"Hay gui file qua cong 9999\" }\n";
            send(client, notify.c_str(), (int)notify.size(), 0);
            db.saveFileHistory(from, to, filename);
        }

        // ====== XỬ LÝ VOICE ======
        else if (action == "voice_register") {
            string uname = parseField(msg, "username");
            string ip = parseField(msg, "ip");
            string portStr = parseField(msg, "port");

            if (uname.empty() || ip.empty() || portStr.empty()) {
                string err = R"({ "action": "error", "message": "Thong tin voice_register khong hop le" })";
                send(client, err.c_str(), (int)err.size(), 0);
                continue;
            }

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
            addr.sin_port = htons(stoi(portStr));

            voiceServer.registerClient(uname, addr);
            string ok = R"({ "action": "voice_register_ok", "message": "Da dang ky voice thanh cong" })" "\n";
            send(client, ok.c_str(), (int)ok.size(), 0);
            cout << "[VOICE] " << uname << " đăng ký địa chỉ UDP " << ip << ":" << portStr << endl;
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
            db.saveCallHistory(from, to, "started");
        }

        // ====== XỬ LÝ LỊCH SỬ ======
        else if (action == "get_history") {
            string reqUsername = parseField(msg, "username");
            if (reqUsername.empty()) reqUsername = username;

            vector<string> chatHistory = db.getChatHistory(reqUsername);
            vector<string> fileHistory = db.getFileHistory(reqUsername);
            vector<string> callHistory = db.getCallHistory(reqUsername);

            auto escape = [](string s) {
                string r = "";
                for (char c : s) {
                    if (c == '\"') r += "\\\"";
                    else r += c;
                }
                return r;
            };

            string json = "{ \"action\": \"history_response\", \"chat\": [";
            for (int i = 0; i < chatHistory.size(); i++) {
                json += "\"" + escape(chatHistory[i]) + "\"";
                if (i < chatHistory.size() - 1) json += ",";
            }
            json += "], \"file\": [";
            for (int i = 0; i < fileHistory.size(); i++) {
                json += "\"" + escape(fileHistory[i]) + "\"";
                if (i < fileHistory.size() - 1) json += ",";
            }
            json += "], \"call\": [";
            for (int i = 0; i < callHistory.size(); i++) {
                json += "\"" + escape(callHistory[i]) + "\"";
                if (i < callHistory.size() - 1) json += ",";
            }
            json += "] }\n";
            send(client, json.c_str(), (int)json.size(), 0);
        }

        // ====== XỬ LÝ UNKNOWN ======
        else {
            string err = R"({ "action": "error", "message": "Unknown action" })" "\n";
            send(client, err.c_str(), (int)err.size(), 0);
        }
    }

    // Khi client thoát
    {
        lock_guard<mutex> lock(mtx);
        clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
        if (!username.empty()) userMap.erase(username);
    }
    closesocket(client);
    sendOnlineList();
    cout << "[INFO] User disconnected: " << username << endl;
}*/

