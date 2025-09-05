#include "ChatClient.h"
#include "Network.h"
#include "Login.h"
#include <iostream>
using namespace std;

// khai báo hàm từ ChatClient.cpp
void startChat(SOCKET sock, const string& username);

int main() {
    SOCKET sock = Network::connectToServer("127.0.0.1", 8888);
    if (sock == INVALID_SOCKET) return 1;

    /*if (showLoginForm(sock)) {
        startChat(sock);
    }*/

    string username = showLoginForm(sock);
    if (!username.empty()) {
        startChat(sock, username);
    }

    Network::closeConnection(sock);
    return 0;
}










