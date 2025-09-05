#include "ChatServer.h"
#include "LoginController.h"


vector<SOCKET> clients;
map<string, SOCKET> userMap; // Map username -> socket
mutex mtx;

string parseField(const string& json, const string& field) {
    size_t pos = json.find("\"" + field + "\":");
    if (pos == string::npos) return "";
    pos = json.find("\"", pos + field.length() + 3) + 1;
    size_t end = json.find("\"", pos);
    return json.substr(pos, end - pos);
}

void broadcast(const string& msg, SOCKET sender) {
    lock_guard<mutex> lock(mtx);
    for (SOCKET c : clients) {
        if (c != sender) {
            send(c, msg.c_str(), msg.size(), 0);
        }
    }
}

void sendToUser(const string& username, const string& msg) {
    lock_guard<mutex> lock(mtx);
    auto it = userMap.find(username);
    if (it != userMap.end()) {
        send(it->second, msg.c_str(), msg.size(), 0);
    }
}

// Hàm parse đơn giản lấy username và message từ JSON
string parseUsername(const string& json) {
    size_t u1 = json.find("\"username\":");
    if (u1 == string::npos) return "";
    u1 = json.find("\"", u1 + 11) + 1;
    size_t u2 = json.find("\"", u1);
    return json.substr(u1, u2 - u1);
}

string parseMessage(const string& json) {
    size_t m1 = json.find("\"message\":");
    if (m1 == string::npos) return "";
    m1 = json.find("\"", m1 + 10) + 1;
    size_t m2 = json.find("\"", m1);
    return json.substr(m1, m2 - m1);
}

void handleClient(SOCKET client) {
    char buf[256];
    int bytes = recv(client, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) { closesocket(client); return; }
    buf[bytes] = '\0';

    string request(buf);
    string loginResult = LoginController::handleLogin(request);
    string username = parseUsername(request); // Lấy username khi đăng nhập
    send(client, loginResult.c_str(), loginResult.size(), 0);

    if (loginResult != "LOGIN_SUCCESS") {
        closesocket(client);
        return;
    }

    {
        lock_guard<mutex> lock(mtx);
        clients.push_back(client);
        userMap[username] = client;
    }

    while (true) {
        int bytes = recv(client, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) break;
        buf[bytes] = '\0';
        std::string msg = buf;
        
        // Thêm dòng này để lấy action từ JSON
        string action = parseField(msg, "action");

        if (action == "private") {
            string from = parseField(msg, "from");
            string to = parseField(msg, "to");
            string message = parseField(msg, "message");
            if (!from.empty() && !to.empty() && !message.empty()) {
                string chatMsg = from + ": " + message; // Định dạng giống chat chung
                sendToUser(to, chatMsg);
            }
        } else {
            string username = parseUsername(msg);
            string message = parseMessage(msg);
            if (!username.empty() && !message.empty()) {
                string chatMsg = username + ": " + message;
                cout << chatMsg << endl;
                broadcast(chatMsg, client);
            }
        }
    }

    {
        lock_guard<mutex> lock(mtx);
        clients.erase(remove(clients.begin(), clients.end(), client), clients.end());
        userMap.erase(username);
    }
    closesocket(client);
}



