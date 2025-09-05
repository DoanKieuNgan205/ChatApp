#include <thread>
#include <string>
#include <iostream>
#include <winsock2.h>
using namespace std;

void receiveMessages(SOCKET sock) {
    char buf[512];
    while (true) {
        int bytes = recv(sock, buf, sizeof(buf)-1, 0);
        if (bytes <= 0) break;
        buf[bytes] = '\0';
        cout << "\n " << buf << "\n> ";
    }
}

void startChat(SOCKET sock, const std::string& username) {
    thread receiver(receiveMessages, sock);
    receiver.detach();

    string msg;
    while (true) {
        cout << "> ";
        getline(cin, msg);
        if (msg == "/quit") break;

        // Nếu bắt đầu bằng /pm username tin nhắn
        if (msg.rfind("/pm ", 0) == 0) {
            // Tách người nhận và nội dung
            size_t space1 = msg.find(' ', 4);
            if (space1 != string::npos) {
                string recipient = msg.substr(4, space1 - 4);
                string content = msg.substr(space1 + 1);
                string sendMsg = "{ \"action\": \"private\", \"from\": \"" + username + "\", \"to\": \"" + recipient + "\", \"message\": \"" + content + "\" }";
                send(sock, sendMsg.c_str(), sendMsg.size(), 0);
                continue;
            }
        }

        // Gửi tin nhắn kèm username (dạng JSON)
        string sendMsg = "{ \"action\": \"chat\", \"username\": \"" + username + "\", \"message\": \"" + msg + "\" }";
        send(sock, sendMsg.c_str(), sendMsg.size(), 0);
    }
}





