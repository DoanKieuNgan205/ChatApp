#include "ChatClient.h"
#include "Network.h"
#include "Login.h"
#include <iostream>

// khai báo hàm từ ChatClient.cpp
void startChat(SOCKET sock, const std::string& username);

int main() {
    SOCKET sock = Network::connectToServer("127.0.0.1", 8888);
    if (sock == INVALID_SOCKET) return 1;


    /*std::string username = showLoginForm(sock);
    if (!username.empty()) {
        startChat(sock, username);
    }

    Network::closeConnection(sock);
    return 0;*/

    std::string username = showLoginForm(sock);
    if (username.empty()) {
        std::cout << "Dang nhap that bai -> thoat chuong trinh.\n";
        Network::closeConnection(sock);
        return 0;
    }

    startChat(sock, username);

    Network::closeConnection(sock);

    // Để không tắt console ngay nếu chạy exe trực tiếp
    system("pause");
    return 0;
}










